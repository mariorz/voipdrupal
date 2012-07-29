<?php
// $id$

/**
 * @file
 * Implementation of tropo-related functionality
 */

/**
 * Internal functions
 */
/*
 * Process incoming requests and responses from tropo
 *
 * @param string $tropo_action set to the action to be executed. 
// TODO: update documentation below for $tropo_action
 *   It can assume any of the following values:
 *   - process_conference, to handle responses to conference call requests
 *   - process_inbound_calls, to handle incoming calls
 *   - process_outbound_calls, to handle the response to outgoing call requests
 *   - process_dial, to handle the response to the "dial" script command
 *   - process_get_input, to handle responses to "get input" script commands
 *   - process_hangup, to handle call termination
 *   - process_record, process_record_mp3 and process_empty_recording, to handle
 *     responses to 'record' script commands
 *   - process_wait, to continue after wait command
 *   - process_send_sms, to handle responses to 'send sms' script commands
 *   - process_sms_callback, to handle final responses to 'send sms'.
 *
 * @param string $cid with the id of the call associated with the response
 *
 * @return void
 */
function _voiptropo_call_handler($tropo_action='', $cid=NULL, $dial_response_name='') {

$msg = "in voiptropo_call_handler() with tropo_action: $tropo_action, cid: $cid, event: $dial_response_name";
watchdog('voiptropo_test', $msg);

  $server = VoipServer::getServer('tropo');
  $server_config = $server->getConfig();

  // determine tropo_action and cid based on the incoming session object
  if ($tropo_action != 'save_record') {
    //save_record doesn't have json
    try {
      // attempt to handle new outbound and inbound voice/text calls
      $session = new Session();
$msg = "Tropo session: ". print_r($session, TRUE);
watchdog('voiptropo_test', $msg, array());
      $tropo_action = $session->getParameters('tropo_action');
      if (empty($tropo_action)) {
        $tropo_action = 'process_inbound_calls';
      }
      $cid = $session->getParameters('cid');

      // process call requests
      $call = _voiptropo_handle_new_calls($tropo_action, $session);
// TODO: process call script only if incoming call
    }
    catch (TropoException $e) {
      try{
        // attempt to process responses/signals/events from ongoing calls
        $result = new Result();
watchdog('voiptropo_test', 'Result ' . print_r($result, TRUE) , array());
        $call = _voiptropo_handle_ongoing_calls($result, $tropo_action, $cid, $dial_response_name);
      }
      catch (TropoException $e) {
        watchdog('voiptropo', 'error in _voiptropo_call_handler, Tropo reported: '. $e->getMessage(), array(), WATCHDOG_ERROR);
        return FALSE; 
      }
    }
  }
watchdog('debug', "I am here");

  if($call){ 
    // get ready to process the call script
    $tropo = new Tropo();
    $menu_path = 'voip/tropo/callhandler';
  
    // process the call script
    $rc = _voiptropo_script_handler($call, $tropo);
  
watchdog('debug', "I am here now");
    // add common event triggers
    if ($tropo_action != 'process_callstatus' && $channel != 'text') {
      $cid = $call->getCid();
      $response_url = $menu_path . '/process_callstatus/' . $cid . '/continue' ;
      $action = url($response_url, array('absolute' => TRUE));
watchdog('debug', "cid: $cid, response_url: $response_url, action: $action");
      $tropo->on(array("event" => "continue", "next" => $action));
    }

    // send response back to tropo
    $tropo->renderJSON();

    // If global user has been replaced for call user.
    // Process shouldn't go further, so just for good practice.
    if(! empty($stored_user)) {
      $user = $stored_user;
watchdog('voiptropo', 'Reverted to logged in user %uid', array('%uid' => $user->uid));
    }
  }

  return;
}

/**
 * Process voipcall scripts
 *
 * @param $call, VoipCall instance with the call information
 *
 * @return boolean.  TRUE on success or FALSE on failure.  Check voip_error() for processing failure.
 */
function _voiptropo_script_handler($call, &$tropo) {
  $msg = 'Entering script_handler with voipcall: ' . print_r($call, TRUE);
  watchdog('voiptropo', $msg, array(), WATCHDOG_INFO);

  $rc = TRUE;

  // get the script object associated with the call
  $script = $call->getScript();
  // check if call is in a hangup state
  $is_hangup = $call->isHangup();
  $processing_hangup = $call->getProcessingHangup();
watchdog('voiptropo_test', "hangup: $hangup, processing_hangup: $processing_hangup");
  if ($is_hangup && !$processing_hangup) {
    $hangup_script_name = $call->getHangupScriptName();
    // Stop processing if no hangup script defined
    if (!$hangup_script_name) {
      return TRUE;
    }
    else {
      // load hangup script commands
      $rc = $script->runGosub($hangup_script_name);
      if ($script->isError()) {
        $msg = $script->getErrorMessage();
        $params = array();
        $call->report_error('voiptropo', $msg, $params);
        return FALSE;
      }
      else {
        // make sure the call does not reload the hangup script once again
        $call->setProcessingHangup(TRUE);
        $call->setCallStatus(VoipCall::IN_PROGRESS);
      }
    }
  }
  
watchdog('voiptropo_test', "p1");
  $processing = TRUE;
  $response = '';

  //Set script language
  $language= $script->getLanguage();
  $voice = $script->getVoice();
  $tropo->setVoice($voice);
watchdog('voiptropo_test', "about to process script");
  while ($processing) {

    $cmd = $script->getNextCommand();
    if (!$cmd) {
      $processing = FALSE;
      // no more commands to be processed
      $stack_level = $script->getStackLevel();
      if ($stack_level>0) {
        //return to previous stack
        $script->runReturn();
        $processing = TRUE;
      }
      else if ($call->getCurrentChannel() != 'text') {
        // for voice channel send a final hangup request 
        if(!$call->getProcessingHangup()){
          $tropo->hangup();
          $msg = 'Script @name ended without a "hangup" command.';
          $params = array('@name' => $script->getName());
          watchdog('voiptropo', $msg, $params, WATCHDOG_WARNING);
          $call->SetCallStatus(VoipCall::COMPLETED);
          $call->setHangupReason(VoipCall::HANGUP_SCRIPT_END);
          $call->setEndTime();
        }
      }
      //for text channel don't do anything
    }
    // process current command
    else {
      $cmd_id = $cmd->getId();

      switch ($cmd_id) {
        default:
          $msg = "Invalid command id '@cmd_id' in script '@script_name': @script";
          $params = array('@cmd_id' => $cmd_id, '@script_name' => $script->getName(), '@script' => print_r($script, TRUE));
          $call->report_error('voiptropo', $msg, $params);
          $rc = FALSE;
          $processing = FALSE;
          break;

        case 'VoipCmdDial';
          $call->setCallStatus(VoipCall::IN_PROGRESS);
          $number = $script->evalString($cmd->getParam('number'));
          $number = str_replace(" ", "", $number); //remove any spaces from number
          if ($call->getCurrentChannel() == 'text'  || $call->getCurrentChannel() == 'offline_text') {
            //In case VoipCmdDial is called within text channel then instead of transfer we call the sender.
            //we are starting a call, hangup() the existing session
            $tropo->hangup();
            
            if (!$number) {
              $number = $call->getCallerNumber();
            }
            //$call = new VoipCall();
            $call->setDestNumber($number);
            $call->setScript($script);
            $call->save();
            voip_dial($call);
          }
          else {
            $menu_path = 'voip/tropo/callhandler';
            $cid = $call->getCid();
            $timeout = $script->evalString($cmd->getParam('timeout', 30));
            //TODO: What command for timeLimit, hangupOnStar?
            /*$hangup_on_star =  $script->evalString($cmd->getParam('hangup_on_star', false));
            $hangup_on_star =  $hangup_on_star? 'true':'false';
            $time_limit = $script->evalString($cmd->getParam('time_limit', '14400'));*/
            $caller_number = $script->evalString($script->getVar('caller_number'));
            $caller_id = $script->evalString($cmd->getParam('caller_id', $caller_number)); 

            $params = array(
                //'timeout' => $timeout,  //TODO: Timeout causes immediate hangup, disabled for now.
                'from' => $caller_id,
            );
            $tropo->transfer($number, $params);

            //Add events
            $response_url = $menu_path . '/process_dial/' . $cid . '/error';
            $action = url($response_url, array('absolute' => TRUE));
            $tropo->on(array("event" => "error", "next" => $action));

            $response_url = $menu_path . '/process_dial/' . $cid . '/incomplete';
            $action = url($response_url, array('absolute' => TRUE));
            $tropo->on(array("event" => "incomplete", "next" => $action));

            $response_url = $menu_path . '/process_hangup/' . $cid ;
            $action = url($response_url, array('absolute' => TRUE));
            $tropo->on(array("event" => "hangup", "next" => $action));
          }
         
          $rc = TRUE;
          $processing = FALSE;
          break;
 
        case 'VoipCmdGetInput':
          _voiptropo_get_input('dtmf', $call, $cmd, $script, $tropo);
          $rc = TRUE;
          $processing = FALSE;
          break;

        case 'VoipCmdGetVoiceInput':
          _voiptropo_get_input('voice', $call, $cmd, $script, $tropo);
          $rc = TRUE;
          $processing = FALSE;
          break;

        case 'VoipCmdGosub':
          $script_name = $script->evalString($cmd->getParam('script_name'));
          $options = $script->evalString($cmd->getParam('options'));
          $rc = $script->runGosub($script_name, $options);
          if ($script->isError()) {
            $msg = $script->getErrorMessage();
            $params = array();
            $call->report_error('voiptropo', $msg, $params);
            $rc = FALSE;
            $processing = FALSE;
            $found = TRUE;
            $text = 'Processing error. Please contact your system administrator.';
            $langcode = $script->getLanguage();
            $text = t($text, NULL, $langcode);
            $tropo->say($text);
          }
          break;

          case 'VoipCmdGoto';
          $label_name = $script->evalString($cmd->getParam('label_name', ''));
          $rc = $script->runGoto($label_name);
          if ($script->isError()) {
            $msg = $script->getErrorMessage();
            $params = array();
            $call->report_error('voiptropo', $msg, $params);
            $rc = FALSE;
            $processing = FALSE;
            $found = TRUE;
            $text = 'Processing error. Please contact your system administrator.';
            $langcode = $script->getLanguage();
            $text = t($text, NULL, $langcode);
            $tropo->say($text);
          }
          break;

          case 'VoipCmdGotoIf';
          $condition = $script->evalString($cmd->getParam('condition', ''));
          if ($condition) { 
            $label_name = $script->evalString($cmd->getParam('label_name', ''));
            $rc = $script->runGoto($label_name, $condition);
            if ($script->isError()) {
              $msg = $script->getErrorMessage();
              $params = array();
              $call->report_error('voiptropo', $msg, $params);
              $rc = FALSE;
              $processing = FALSE;
              $found = TRUE;
              $text = 'Processing error. Please contact your system administrator.';
              $langcode = $script->getLanguage();
              $text = t($text, NULL, $langcode);
              $tropo->say($text);
            }
          }
          break;

        case 'VoipCmdHangup':
          $call->SetCallStatus(VoipCall::COMPLETED);
          $call->setHangupReason(VoipCall::HANGUP_SCRIPT_COMMAND);
          $start = $call->getStartTime();
          $end = $call->setEndTime();
          $duration = $end - $start;
          $call->setDuration($duration);
          $hangup_mode = $script->evalString($cmd->getParam('hangup_mode'));
          
          switch ($hangup_mode) {
            case 'resume':
              // don't do anything
              break;
            case 'reset':
              //Reset script
              $script->resetIndex();
              break;
            case 'end_session':
              //Kill the session if any
              $cid = $call->getCid();
              $session = VoipSession::getSessionByCid($cid);
              if ($session) {
                $session->destroy();
              }
            break;
          }
          if ($call->getCurrentChannel() != 'text') {
            // send the hangup request and stop processing script commands
            $tropo->hangup();
          }
          $processing = FALSE;
          break;

          case 'VoipCmdJoinConference';
          $call->setCallStatus(VoipCall::IN_PROGRESS);
          $menu_path = 'voip/tropo/callhandler';
          $cid = $call->getCid();
          $response_url = $menu_path . '/process_conference/' . $cid;
          $action = url($response_url, array('absolute' => TRUE));

          $room = $script->evalString($cmd->getParam('room'));
          $muted =  $script->evalString($cmd->getParam('muted', FALSE));
          $muted =  $muted? 'true':'false';
          /*$beep =  $script->evalString($cmd->getParam('beep', false));
            $beep =  $beep? 'true':'false';
            $start_on_enter =  $script->evalString($cmd->getParam('start_on_enter', false));
            $start_on_enter =  $start_on_enter? 'true':'false';
            $end_on_exit =  $script->evalString($cmd->getParam('end_on_exit', false));
            $end_on_exit =  $end_on_exit? 'true':'false';*/
          $wait_url =  $script->evalString($cmd->getParam('wait_url', ''));
          $hangup_on_star =  $script->evalString($cmd->getParam('hangup_on_star', FALSE));
          //$hangup_on_star =  $hangup_on_star? 'true':'false';
          if ($hangup_on_star) {
            $terminator="*";
          }
          else{
            $terminator="";
          }
          //$time_limit = $script->evalString($cmd->getParam('time_limit', '3600'));

          //TODO: What about this options: beep, start_on_enter, end_on_exit, time_limit?
          $params = array(
              'id' => $room,
              'mute' => $muted,
              'terminator' => $terminator,
              );

          $tropo->conference($room, $params);
          $tropo->on(array("event" => "continue", "next" => $action));

          $response_url = $menu_path . '/process_hangup/' . $cid ;
          $action = url($response_url, array('absolute' => TRUE));
          $tropo->on(array("event" => "hangup", "next" => $action));

          $rc = TRUE;
          $processing = FALSE;
          break;

        case 'VoipCmdLabel':
          // don't do anything. Labels are just placeholders that mark a position within the script.
          break;

        case 'VoipCmdLog':
          $input = $script->evalString($cmd->getParam('input', ''));
          $name = $script->evalString($cmd->getParam('name', NULL));
          $script->runLog($input, $name);
          break;

        case 'VoipCmdRecord':
          $call->setCallStatus(VoipCall::IN_PROGRESS);
          $menu_path = 'voip/tropo/callhandler';
          $cid = $call->getCid();
          $format = $script->evalString($cmd->getParam('format'));
          if ($format == 'mp3') {
            $record_format = 'audio/mp3';
          }
          else {
            // return wav file        
            $record_format = 'audio/wav';
          }
          $response_url = $menu_path . '/save_record/' . $cid;
          $record_action = url($response_url, array('absolute' => TRUE));

          $response_url = $menu_path . '/process_record/' . $cid;
          $success_action = url($response_url, array('absolute' => TRUE));
          $method = 'POST';
          $timeout = $script->evalString($cmd->getParam('timeout', 5.0));
          $end_key = $script->evalString($cmd->getParam('end_key', '#'));
          $max_length = $script->evalString($cmd->getParam('max_length', 3600));

          $play_beep = $script->evalString($cmd->getParam('play_beep'));
          $play_beep = $play_beep ? 'true' : 'false';
          $response_url = $menu_path . '/process_empty_recording/' . $cid;
          $timeout_action = url($response_url, array('absolute' => TRUE));

          //$choices->terminator=$end_key;
          $choices=$end_key;
          $params = array(
              'maxTime' => $max_length,
              'maxSilence' => (float)$timeout,
              'beep' => $play_beep,
              'format' => $record_format,
              'method' => $method,
              'url' => $record_action, 
              'choices' => $choices,
              );

          $prompt = _voiptropo_prompt_validate($script, $cmd->getParam('prompt'));
          $default_language = $script->getLanguage();
          $default_voice = $script->getVoice();

          /*bug with Tropo in VoipCmdRecord. If say parameter is missing from tropo record() command, an error will happen terminating 
            the current call. Therefore getting last item in array and moving it to record() command.*/
          $last_say = array_pop($prompt);
          $text = $script->evalString($last_say->getText());
          $text = _voiptropo_strip_special_chars($text);
          $voice_id = $last_say->getVoice() ? $last_say->getVoice() :  $default_voice;
          $as = $last_say->getAs() ? $script->evalString($last_say->getAs()) : '';

          if (!empty($as) && in_array($as, array('digits', 'date'))) {
            $params['as'] = $as; 
          }
          $params['voice'] = $voice_id;
          $params['say'] = $text;

          foreach ($prompt as $p) {
            $text = $script->evalString($p->getText());
            $text = _voiptropo_strip_special_chars($text);
            $voice_id = $p->getVoice() ? $p->getVoice() : $default_voice;
            $as = $p->getAs() ? $script->evalString($p->getAs()) : '';
            $say_params = array();
            if (!empty($as) && in_array($as, array('digits', 'date'))) {
              $say_params['as'] = $as; 
            }
            $say_params['voice'] = $voice_id;
            //Solution for Tropo error with sprintf and url encoded strings
            //https://www.tropo.com/bizblog/viewer?&bb-name=tropo_support&bb-q=sprintf&&bb-cid=99&bb-tid=1193463#bb
            $text = str_replace("%","%%",$text);
            $tropo->say($text, $say_params);
          }

          $tropo->record($params);
          //What next:
          $tropo->on(array("event" => "continue", "next" => $success_action));
          // in case of empty recording...
          $tropo->on(array("event" => "incomplete", "next" => $timeout_action));
          $response_url = $menu_path . '/process_record_hangup/' . $cid;      
          $hangup_action = url($response_url, array('absolute' => TRUE));
          $tropo->on(array("event" => "hangup", "next" => $hangup_action));

          $rc = TRUE;
          $processing = FALSE;
          break;

        case 'VoipCmdReject':
          $call->setHangupReason(VoipCall::HANGUP_REJECTED);
          //TODO: Is there reason param in Tropo?
          $tropo->reject();
          $call->SetCallStatus(VoipCall::REJECTED);
          $start = $call->getStartTime();
          $end = $call->setEndTime();
          $duration = $end - $start;
          $call->setDuration($duration);
          $rc = TRUE;
          $processing = FALSE;
          break;

        case 'VoipCmdReturn':
          $rc = $script->runReturn();
          if ($script->isError()) {
            $msg = $script->getErrorMessage();
            $params = array();
            $call->report_error('voiptropo', $msg, $params);
            $rc = FALSE;
            $processing = FALSE;
            $found = TRUE;
            // report the error to the phone user
            $text = 'Processing error. Please contact your system administrator.';
            // translate the text
            $langcode = $script->getLanguage();
            $text = t($text, NULL, $langcode);
            $tropo->say($text);
          }
          break;

        case 'VoipCmdRunIvrMenu':
          $options['prompt'] = $cmd->getParam('prompt');
          $options['input_options'] = $script->evalString($cmd->getParam('input_options'));
          $options['invalid_msg'] = $cmd->getParam('invalid_msg');
          $options['no_input_msg'] = $cmd->getParam('no_input_msg');
          $options['max_attempts'] = $script->evalString($cmd->getParam('max_attempts'));
          $options['timeout'] = $script->evalString($cmd->getParam('timeout'));
          $rc = $script->runIvrMenu($options);
          break;  

        case 'VoipCmdSay':
          $call->setCallStatus(VoipCall::IN_PROGRESS);
          $loop = $script->evalString($cmd->getParam('loop'));
          $prompt = _voiptropo_prompt_validate($script, $cmd->getParam('prompt'));
          $default_language = $script->getLanguage();
          $default_voice = $script->getVoice();

          $menu_path = 'voip/tropo/callhandler';
          $cid = $call->getCid();
          $response_url = $menu_path . '/process_hangup/' . $cid ;
          $action = url($response_url, array('absolute' => TRUE));

          for ($i=0;$i<$loop;$i++) {              
            foreach ($prompt as $p) {
              $text = $script->evalString($p->getText());
              $text = _voiptropo_strip_special_chars($text);
              $voice_id = $p->getVoice() ? $p->getVoice() : $default_voice;
//watchdog("debug", "Say: voice: $voice_id, language: $language, text: $text");
              $as = $p->getAs() ? $script->evalString($p->getAs()) : '';
              $say_params = array();
              if($as == 'digits'){
                $text = "<speak>"._voiptropo_parse_numbers($text)."</speak>";
              }
              elseif($as == 'date'){
                $say_params['as'] = $as; 
              }
              
              $say_params['voice'] = $voice_id;
              //Solution for Tropo error with sprintf and url encoded strings
              //https://www.tropo.com/bizblog/viewer?&bb-name=tropo_support&bb-q=sprintf&&bb-cid=99&bb-tid=1193463#bb
              $text = str_replace("%","%%",$text);
              $tropo->say($text, $say_params);

              $tropo->on(array("event" => "hangup", "next" => $action));
            }
          }
          break;

        case 'VoipCmdSendText';
          $call->setCallStatus(VoipCall::IN_PROGRESS);
          $menu_path = 'voip/tropo/callhandler';
          $cid = $call->getCid();

          $callback_url = $menu_path . '/process_sms_callback/' . $cid;
          $callback_continue = url($callback_url . '/continue', array('absolute' => TRUE));
          $callback_error = url($callback_url . '/error', array('absolute' => TRUE));
          $callback_hangup = url($callback_url . '/hangup', array('absolute' => TRUE)); 
          $callback_incomplete = url($callback_url . '/incomplete', array('absolute' => TRUE));
          
          $to = $script->evalString($cmd->getParam('to'));
          //If "to" is empty then send message to the caller
          if (!$to) {
            $to = $script->evalString($script->getVar('caller_number'));
          }
          $text = $script->evalString($cmd->getParam('text'));
          $network = $script->evalString($cmd->getParam('network'));
          if (voipscript_use_sms_framework() && (strtolower($network) == 'sms' || strtolower($call->getCurrentNetwork())=='sms')) {
            //Use SMS Framework to send SMS message if user enabled this option
            voipscript_send_sms_framework($to, $text);
          }
          else if(strtolower($call->getCurrentChannel()) == 'offline_text'){
            //Use voip_text()
            $reply = TRUE;
            $call->setCurrentChannel('text');
            voip_text($text, $call, $reply);
            $processing = TRUE;
          }
          else {
            $params = array(
              'to' => $to,
              'channel' => 'TEXT',
              'network' => $network,
              );
            if (strtolower($network) == 'sms') {
              //We need caller id only for sms
              // Note: If no caller number provided, use the system's number as the default caller id
              $default_cid_number = variable_get('voipcall_cid_number', NULL);
              $caller_id = $script->evalString($cmd->getParam('caller_id', $default_cid_number)); 
              //$params['from'] = $caller_id;
            }
            $tropo->message($text, $params);
            //Register events:
            //$tropo->on(array("event" => "continue", "next" =>  $callback_continue));
            $tropo->on(array("event" => "error", "next" =>  $callback_error));
            $tropo->on(array("event" => "incomplete", "next" =>  $callback_incomplete));
            $response_url = $menu_path . '/process_hangup/' . $cid ;
            $action = url($response_url, array('absolute' => TRUE));
            $tropo->on(array("event" => "hangup", "next" => $action));
          }
          $rc = TRUE;
          $processing = TRUE;
          break;

        case 'VoipCmdBeep';
          $beep_url = url('voip/sound/beep', array('absolute' => TRUE));
          $tropo->say($beep_url);
          break;

        case 'VoipCmdSet';
          $var_name = $script->evalString($cmd->getParam('var_name'));
          $var_value = $script->evalString($cmd->getParam('var_value'));
          $script->setVar($var_name, $var_value);
          break;

        case 'VoipCmdSetVoice';
          $voice_id = $script->evalString($cmd->getParam('voice'));
          $voice = VoipVoice::getVoice($voice_id);
          $voice_id = $voice->getVoiceId();
          $script->setVoice($voice_id);
          break;

        case 'VoipCmdUnset';
          $var_name = $script->evalString($cmd->getParam('var_name'));
          $script->unsetVar($var_name);
          break;
          
        case 'VoipCmdWait';
          $time_limit = $script->evalString($cmd->getParam('time_limit')) / 0.55;  //Looks like one period matches 0.55sec.
          $menu_path = 'voip/tropo/callhandler';
          $cid = $call->getCid();
          $response_url = $menu_path . '/process_wait/' . $cid;
          $redirect_url = url($response_url, array('absolute' => TRUE));
          //#1430227 - Equivalent of the wait function?: https://www.tropo.com/account/tickets/tickets.jsp?bb-cid=155&bb-tid=1429227&bb-name=tropo_support
          for ($i=0; $i<$time_limit; $i++) {
            $periods.=". ";
          }
          $tropo->say($periods);
          $tropo->on(array("event" => "continue", "next" =>  $redirect_url));

          $response_url = $menu_path . '/process_hangup/' . $cid ;
          $action = url($response_url, array('absolute' => TRUE));
          $tropo->on(array("event" => "hangup", "next" => $action));
          $processing = FALSE;
          break;  
      }
      // go to next command
    }
  }

watchdog('voiptropo_test', "about to leave script " . $script->getName());
  // save script for next iteration
  $call->setScript($script);
  $call->save();
  return $rc;
}

function _voiptropo_file_save_upload($source, $validators, $dest) {
  global $user;
  $extensions='mp3 wav';
  // Add in our check of the the file name length.
  $validators['file_validate_name_length'] = array();

  // Begin building file object.
  $file = new stdClass();
  $file->filename = file_munge_filename(trim(basename($_FILES['filename']['name']), '.'), $extensions);
  $file->filepath = $_FILES['filename']['tmp_name'];
  $file->filemime = file_get_mimetype($file->filename);

  $file->source = $source;
  $file->destination = file_destination(file_create_path($dest . '/' . $file->filename), $replace);
  $file->filesize = $_FILES['filename']['size'];

  // Call the validation functions.
  $errors = array();
  foreach ($validators as $function => $args) {
    array_unshift($args, $file);
    // Make sure $file is passed around by reference.
    $args[0] = &$file;
    $errors = array_merge($errors, call_user_func_array($function, $args));
  }

  // Rename potentially executable files, to help prevent exploits.
  if (preg_match('/\.(php|pl|py|cgi|asp|js)$/i', $file->filename) && (substr($file->filename, -4) != '.txt')) {
    $file->filemime = 'text/plain';
    $file->filepath .= '.txt';
    $file->filename .= '.txt';
    // As the file may be named example.php.txt, we need to munge again to
    // convert to example.php_.txt, then create the correct destination.
    $file->filename = file_munge_filename($file->filename, $extensions);
    $file->destination = file_destination(file_create_path($dest . '/' . $file->filename), $replace);
  }


  // Check for validation errors.
  if (!empty($errors)) {
    $message = t('The selected file %name could not be uploaded.', array('%name' => $file->filename));
    if (count($errors) > 1) {
      $message .= '<ul><li>' . implode('</li><li>', $errors) . '</li></ul>';
    }
    else {
      $message .= ' ' . array_pop($errors);
    }
    drupal_set_message($message);
    return 0;
  }

  // Move uploaded files from PHP's upload_tmp_dir to Drupal's temporary directory.
  // This overcomes open_basedir restrictions for future file operations.
  $file->filepath = $file->destination;
  if (!move_uploaded_file($_FILES['filename']['tmp_name'], $file->filepath)) {
    form_set_error($source, t('File upload error. Could not move uploaded file.'));
    watchdog('file', 'Upload error. Could not move uploaded file %file to destination %destination.', array('%file' => $file->filename, '%destination' => $file->filepath));
    return 0;
  }

  // If we made it this far it's safe to record this file in the database.
  $file->uid = $user->uid;
  $file->status = FILE_STATUS_TEMPORARY;
  $file->timestamp = time();
  drupal_write_record('files', $file);
  return $file;
}

/**
 * Update 'call status' and 'hangup reason' based on tropo's response
 */
function voiptropo_update_call_status($call, $tropo_call_status) {
 
  switch ($tropo_call_status) {
    default:
      $msg = 'Invalid CallStatus: @status for call cid: @cid';
      $cid = $call->getCid();
      $params = array('@status' => $call_status, '@cid' => $cid);
      $call->report_error('voiptropo', $msg, $params);
      return NULL;
      break;
    case 'incomplete': // call failed, most likely due to invalid number
      $call->setCallStatus(VoipCall::INVALID);
      $call->setHangupReason(VoipCall::HANGUP_INVALID_NUMBER);
      break;
    case 'ringing':
      $call->setCallStatus(VoipCall::RINGING);
      break;
    case 'in-progress':
      $call->setCallStatus(VoipCall::IN_PROGRESS);
      break;
    case 'continue':
      $call->setCallStatus(VoipCall::COMPLETED);
      $hangup_reason = $call->getHangupReason();
      if (!$hangup_reason) {
        $call->setHangupReason(VoipCall::HANGUP_BY_USER);
      }
      break;
    case 'busy':
      $call->setCallStatus(VoipCall::BUSY);
      $call->setHangupReason(VoipCall::HANGUP_BUSY);
      break;
    case 'error':
      $call->setCallStatus(VoipCall::NO_ANSWER);
      $call->setHangupReason(VoipCall::HANGUP_NO_ANSWER);
      break;
    case 'canceled':
      $call->setCallStatus(VoipCall::CANCELED);
      $call->setHangupReason(VoipCall::HANGUP_API_REQUEST);
      break;
  }
  return $call;
}


/**
 * Process the response to a "dial" script command
 */ 
function _voiptropo_process_dial_response($call, $request, $dial_response_name) {
//TODO: do we really need the param $dial_response_name above?
  $script = $call->getScript();
  // update the 'dial_sid' script variable
  $script->setVar('dial_sid', $request->getSessionId());//TODO: What is this? I used session id

  // update the 'dial_status' script variable
  $dial_status = $request->getState();
  switch (strtolower($dial_status)) {
    default:
      $cid = $call->getCid();
      $msg = 'Invalid DialCallStatus: @status for call cid: @cid';
      $params = array('@status' => $dial_status, '@cid' => $cid);
      $call->report_error('voiptropo', $msg, $params);
      return NULL;
      break;
    case 'completed':
    case 'answered':
      $script->setVar('dial_status', VoipScript::DIAL_COMPLETED);
      break;
    case 'busy':
      $script->setVar('dial_status', VoipScript::DIAL_BUSY);
      break;
    case 'no-answer':
      $script->setVar('dial_status', VoipScript::DIAL_NO_ANSWER);
      break;
    case 'disconnected':
      $script->setVar('dial_status', VoipScript::DIAL_FAILED);
      break;
    case 'canceled':
      $script->setVar('dial_status', VoipScript::DIAL_CANCELED);
      break;
  }

  $call->setScript($script);
  return $call;
}

/**
 * Process the response to a 'sms send' script command
 */
function _voiptropo_process_sms_response($call, $sms_status_tmp) {
watchdog('voiptropo_test', "proc_sms: status: $sms_status_tmp");
  $cid = $call->getCid();

  switch ($sms_status_tmp) {
    default:
      $msg = 'Invalid SmsStatus: @status for call cid: @cid';
      $params = array('@status' => $sms_status_tmp, '@cid' => $cid);
      $call->report_error('voiptropo', $msg, $params);
      return NULL;
      break;
    case 'error':
      $msg = 'SMS sendind failed for call cid: @cid. Make sure your tropo number is sms-enabled.';
      $params = array('@cid' => $cid);
      watchdog('voiptropo', $msg, $params, WATCHDOG_WARNING);
      $sms_status = VoipCall::TEXT_FAILED;
      break;
    case 'incomplete':
      $msg = 'Invalid SMS parameter for call cid: @cid';
      $params = array('@cid' => $cid);
      watchdog('voiptropo', $msg, $params, WATCHDOG_WARNING);
      $sms_status = VoipCall::TEXT_INVALID;
      break;
    case 'sending':
      $sms_status = VoipCall::TEXT_SENDING;
      break;
    case 'continue':
      $sms_status = VoipCall::TEXT_SENT;
      break;
  }
  $script = $call->getScript();
  $script->setVar('send_text_status', $sms_status);
  $call->setScript($script);
  $call->setTextStatus($sms_status);
  return $call;
}

function _voiptropo_prompt_validate($script, $prompt) {
  if (is_string($prompt) && (substr($prompt, 0, 1) == '^')) {
    $prompt = $script->evalString($prompt);            
  }

  if (is_string($prompt)) {
    $prompt_list[] = new VoipPrompt($prompt);
  }
  elseif (is_object($prompt)) {
    $prompt_list[] = $prompt;
  }
  else {
    $prompt_list=$prompt;
  }
  return $prompt_list;
}

function _voiptropo_get_input($type, $call, $cmd,  $script, &$tropo){
  $call->setCallStatus(VoipCall::IN_PROGRESS);
  $menu_path = 'voip/tropo/callhandler';
  $cid = $call->getCid();

  $timeout = $script->evalString($cmd->getParam('timeout', 5.0)); 
  $end_key = $script->evalString($cmd->getParam('end_key', '')); 
  $default_language = $script->getLanguage();
  $default_voice = $script->getVoice();
          
  if($type=='voice'){
    $response_url = $menu_path . '/process_get_voice_input/' . $cid;
    $choices = $script->evalString($cmd->getParam('choices'));
    $mode = $script->evalString($cmd->getParam('mode'));

    $recognize_as_tmp = $script->evalString($cmd->getParam('recognize_as'));
    $default_region = VoipTropoServer::getVoiceRegionId($default_voice, $default_language);
    $recognizer = $recognize_as_tmp ? $recognize_as_tmp : $default_region;
            
    $allowed_modes = array('keypad', 'speech', 'any');
    if(in_array($mode, $allowed_modes)) {
      //Convert keypad to dtmf to match Tropo allowed mode values
      if($mode == 'keypad'){
          $mode = 'dtmf';
      }
    }
    else {
        $mode='speech';
    }
  }
  else {
    $response_url = $menu_path . '/process_get_input/' . $cid;
    $mode='dtmf';
    $num_digits = $script->evalString($cmd->getParam('num_digits'), 1);
    
    if (empty($num_digits)) {
      //If empty it means that number of digits is variable:
      $num_digits = 999;
    }
    $grxml_path="voip/tropo/grammar/numeric_star_hash/". $num_digits;
    $choices = url($grxml_path, array('absolute' => TRUE));
  }
  $attempts = 1;
  $timeout = (float)$timeout;
  $prompt = _voiptropo_prompt_validate($script, $cmd->getParam('prompt'));
  $ask_text = '<speak>';
    foreach ($prompt as $p) {
    $text = $script->evalString($p->getText());
    $text = _voiptropo_strip_special_chars($text);
    $voice_id = $p->getVoice() ? $p->getVoice() : $default_voice;
    $as = $p->getAs() ? $script->evalString($p->getAs()) : '';
    if (empty($as) && !in_array($as, array('digits', 'date'))) {
            $as = 'number'; 
    }
   
    
    //Tropo doesn't support multiple voices in ask(): https://www.tropo.com/bizblog/viewer?&bb-name=tropo_support&bb-q=&&bb-cid=99&bb-tid=1427328#bb
    //Therefore using SSML directly
    $text =_voiptropo_parse_audio($text);
    $ask_text .= '<voice name=\''.$voice_id.'\'><say-as interpret-as=\'vxml:'.$as.'\'>'.$text.'</say-as></voice>';
  }
  $ask_text .= '</speak>';
  $params = array(
    'choices' => $choices,
    'mode' => $mode,
    'terminator' => $end_key,
    'attempts' => $attempts,
    'timeout' => $timeout,
    'recognizer' => $recognizer,
  );
  $tropo->ask($ask_text, $params);

  // event triggers
  $action = url($response_url, array('absolute' => TRUE));
  $tropo->on(array("event" => "continue", "next" => $action)); 

  $action = url($response_url . '/incomplete', array('absolute' => TRUE));
  $tropo->on(array("event" => "incomplete", "next" => $action));

  $response_url = $menu_path . '/process_hangup/' . $cid ;
  $action = url($response_url, array('absolute' => TRUE));
  $tropo->on(array("event" => "hangup", "next" => $action));
}

function _voiptropo_parse_numbers($str) {
  $str_array = explode(" ",$str);
  foreach ($str_array as $key => $word) {
    if(!_voiptropo_is_url($word)){
      $str_array[$key] = preg_replace_callback('{(\d+)}', '_voiptropo_ssml_digits', $word);
    }
  }
  
  return implode(" ", $str_array);
}

/*Replace url with <audio> tag*/
function _voiptropo_parse_audio($str) {
  $str_array = explode(" ",$str);
  foreach ($str_array as $key => $word) {
    if(_voiptropo_is_url($word)){
      $str_array[$key] = "<audio src='".$word."'/>";
    }
  }
  
  return implode(" ", $str_array);
}

function _voiptropo_ssml_digits($matches) {
  return "<say-as interpret-as='vxml:digits'>".$matches[0]."</say-as>";
}

/**
 * Check if the given string is a url
 */
function _voiptropo_is_url($str) {
  // Note: Although the checking performed is not as complete as
  //       http://stackoverflow.com/questions/161738/what-is-the-best-regular-expression-to-check-if-a-string-is-a-valid-url , it seems to serve our purposes!
  $rc = preg_match('#(https?://\S+(?<![,.;?\:\!]))#', $str);
  return $rc;
}

function _voiptropo_strip_special_chars($string) {
  //Tropo doesn't support double quotes https://www.tropo.com/bizblog/viewer?&bb-name=tropo_support&bb-q=quotes&&bb-cid=100&bb-tid=1337679#bb
  $string = str_replace('"','&quot;',$string);
  //This would be better but it breaks SSML:
  //$string = htmlspecialchars($string);
  return $string;
}

function _voiptropo_handle_new_calls($tropo_action, $session) {
  $rc = FALSE;

  // Determine current channel and network
  if ($tropo_action == 'process_outbound_calls') {
    $channel = 'voice';
    $network = 'voip';
  }
  else if ($tropo_action == 'process_outbound_text') {
    $channel = 'text';
    $network = 'sms';
  }
  else {
    // handle process_inbound_calls
    $tmp = $session->getTo();
    $channel = strtolower($tmp['channel']);
    $network = strtolower($tmp['network']);
  }

  // Determine call origin and destination
  if (($tropo_action == 'process_outbound_calls')
     || ($tropo_action == 'process_outbound_text')) {
    $from = $session->getParameters('caller_number');
    $to = $session->getParameters('dest_number');
  }
  else {
    $tmp = $session->getFrom();
    $from = $tmp['id'];
    $tmp = $session->getTo();
    $to = $tmp['id'];
  }
  // prefix the numbers with a "+" to follow the standard formatting
// TODO: move this functionality elsewhere
  $from = voipcall_normalize_phone_number($from);
  $to = voipcall_normalize_phone_number($to);

$msg = "tropo_action: $tropo_action, channel: $channel, network: $network, from: $from, to: $to";
$vars = array();
watchdog('voiptropo_test', $msg, $vars, WATCHDOG_INFO);

  // determine the cid and voip_session
  $cid = $session->getParameters('cid');
  // load VoIP Drupal session and cid based on request parameters
  if ($cid) {
    $voip_session = VoipSession::getSessionByCid($cid);
watchdog('voiptropo_test', "session from cid: " . print_r($voip_session,true));
  }
  else {
    $voip_session = VoipSession::getSessionByNumber($from, $to);
    if($voip_session) {
      $cid = $voip_session->getCid();
     }
watchdog('voiptropo_test', "session from numbers: " . print_r($voip_session,true));
  }
  if(!$voip_session) {
    $voip_session = new VoipSession();
    $voip_session->setOriginNumber($from);
    $voip_session->setDestinationNumber($to);
watchdog('voiptropo_test', "new session: " . print_r($voip_session,true));
  }

  // load the already existing call object, if any
  if($cid) {
    $call = VoipCall::load($cid);
  }
  else {
    $call = new VoipCall();
  }
watchdog('voiptropo_test', "call 1: " . print_r($call,true));

  // load global $user based on call user
  global $user;
  $stored_user = $user;
  $call->loadGlobalUser();
watchdog('voiptropo', 'Logged user %uid in.', array('%uid' => $user->uid));

  // set the call object

  // handle incoming call requests (both text and voice)
  if ($tropo_action == 'process_inbound_calls') {
    $call_id=$session->getId();
    $call->setCallId($call_id);
    $call->setSource('tropo');
    $dir_tmp = VoipCall::INBOUND;
    $call->setDirection($dir_tmp);
    $call->setAnsweredBy(VoipCall::ANSWERED_DRUPAL);
    $call->setStartTime(time());
    $call->setCallerNumber($from);
    $tmp = $session->getFrom();
    if($tmp['name'])
      $call->setCallerName($tmp['name']);
    $call->setDestNumber($to);
    $call->setCallChannel($channel);
    $call->setCallNetwork($network);
  }
  else {
    // handle outbound call requests
    // note that those calls have already been set by the server
    // store tropo call id for all outbound calls
    $call_id=$session->getId();
    $call->setCallId($call_id);
  }

  // update common call variables
  $call->setCurrentChannel($channel);
  $call->setCurrentNetwork($network);

  // set text channel variables
  if (in_array($channel, array('offline_text','text'))) {
    $call->SetCallStatus(VoipCall::IN_PROGRESS);
    $inbound_text_contents = $session->getInitialText();
    $call->setTextVariables($inbound_text_contents, $from, $to, $network);
  }
  else {
    $call->setCallStatus(VoipCall::RINGING);
  }

  // update the call session object
  $cid = $call->getCid();
  if(!$voip_session->getCid()){
    $voip_session->setCid($cid);
  }
  $voip_session->resetTimer();
watchdog('voiptropo_test', "session about to be saved: " . print_r($voip_session, true));
  $voip_session->save();

  // update the voipcall with its new values
  $call = $call->save();
watchdog('voiptropo_test', "call 4: " . print_r($call,true));

  // process the call requests
  $tropo = new Tropo();
  $menu_path = 'voip/tropo/callhandler';

  if(($tropo_action == 'process_outbound_calls') || ($tropo_action == 'process_outbound_text')) {

    if ($tropo_action == 'process_outbound_calls') {
      //Make outbound call
      $dest_number = $call->getDestNumber();
      $params = array(
        'from' => $call->getCallerNumber(),
        'allowSignals' => 'hangup',
        );
watchdog('voiptropo_test', "about to call $dest_number");
      $tropo->call($dest_number, $params);

      //Add events for outbound dial.
      $response_url = $menu_path . '/process_hangup/' . $cid ;
      $action = url($response_url, array('absolute' => TRUE));
      $tropo->on(array("event" => "hangup", "next" => $action));
  
      $response_url = $menu_path . '/process_callstatus/' . $cid;
      $action = url($response_url . '/error', array('absolute' => TRUE));
      $tropo->on(array("event" => "error", "next" => $action));

      $action = url($response_url . '/incomplete', array('absolute' => TRUE));
      $tropo->on(array("event" => "incomplete", "next" => $action));

      $response_url = $menu_path . '/process_callstatus/' . $cid . '/in-progress' ;
      $action = url($response_url, array('absolute' => TRUE));
      $tropo->on(array("event" => "continue", "next" => $action));

      // $response_url = $menu_path . '/process_recorded_test/' . $cid;
      //$action = url($response_url, array('absolute' => TRUE));
      /* $record_params = array(
         'say' =>'start',
         'beep' => 'false',
        //'timeout' => '10.0',
      'maxSilence' => '1.0',
      'maxTime' => '10.0',
      //'url' => $action,
      );

      $tropo->record($record_params);
  
      $response_url = $menu_path . '/process_voice_analysis/' . $cid;
      $action = url($response_url, array('absolute' => TRUE));
      $tropo->on(array("event" => "continue", "next" => $action));
       */
    }
    else if($tropo_action == 'process_outbound_text') {
     $to = $call->getDestNumber();
     $text = $session->getParameters('text');
     $params = array(
       'to' => $to,
       'channel' => 'TEXT',
       'network' => 'SMS',
      );
      $tropo->message($text, $params);
watchdog('voiptropo_test', "sms sent: " . print_r($tropo, true));
    }
    // send the request to Tropo
    $tropo->renderJSON();
  }

  // special processing for incoming text messages
  else if ($tropo_action == 'process_inbound_calls' && in_array($call->getCurrentChannel(), array('offline_text','text'))) {
    $script = $call->getScript();
    $text_handler = $script->getVar('text_handler');
    if ($text_handler) {
      $message = $script->getVar('inbound_text_contents');
      $res = call_user_func_array($text_handler, array($message, $call->getCallerNumber(), $call->getDestNumber(), $call, $call->getCurrentNetwork()));
    }
  }

  if($tropo_action == 'process_inbound_calls') {
    // return inbound calls so that they can have their scripts processed
    $rc = $call;
  }

  return $rc;
}


function _voiptropo_handle_ongoing_calls($result, $tropo_action='', $cid=NULL, $dial_response_name='') {

  // 'process_hangup' for incoming calls does not come with a $cid
  if ($tropo_action == 'process_hangup') {
    if (!$cid) {
    }
  }

  // load VoIP Drupal session and cid based on request parameters
  if (!$cid) {
    $call_id = $result->getSessionId();
    $cid = VoipCall::getCidFromCallId($call_id);
  }
  $voip_session = VoipSession::getSessionByCid($cid);
watchdog('voiptropo_test', "session from cid: " . print_r($voip_session,true));

  // load the already existing call object, if any
  if($cid) {
    $call = VoipCall::load($cid);
  }
  else {
    $call = new VoipCall();
  }
watchdog('voiptropo_test', "call 1: " . print_r($call,true));

  // load global $user based on call user
  global $user;
  $stored_user = $user;
  $call->loadGlobalUser();
watchdog('voiptropo', 'Logged user %uid in.', array('%uid' => $user->uid));

  // initialize common variables
  if ($tropo_action == 'process_sms_callback') {
    $call->SetCallStatus(VoipCall::IN_PROGRESS);
  }

  // save the call and initialize its script before setting internal variables
  $call->save();
watchdog('voiptropo_test', "call 2: " . print_r($call,true));

  // update script variables

  // process responses to requests previously-issued by the call script
  $is_hangup = $call->isHangup();
  if (!$is_hangup) {
    switch ($tropo_action) {
      default:
        $msg = 'Invalid tropo_action: @action';
        $params = array('action' => $tropo_action);
        $call->report_error('voiptropo', $msg, $params);
        return FALSE;
        break;
      case 'process_wait':
        // nothing special to be done here.
        break;
      case 'process_hangup':
        $call->SetCallStatus(VoipCall::COMPLETED);
        $call->setHangupReason(VoipCall::HANGUP_BY_USER);
        break;
      case 'process_dial':
      case 'process_conference':
        $call = _voiptropo_process_dial_response($call, $result, $dial_response_name);
        break;
      case 'process_callstatus':
        voiptropo_update_call_status($call, $dial_response_name);
        // update call duration
        $end_time = $call->setEndTime();
        $start_time = $call->getStartTime();
        if ($end_time) {
          $duration  = $end_time - $start_time;
        }
        else {
          //get call duration from tropo
          $duration = $result->getSessionDuration();
        }
        $call->setDuration($duration);
        break;
      case 'process_get_input':
        $script = $call->getScript();
        $digits = str_replace(" ","",$result->getValue());
        $script->setVar('input_digits', $digits);
        $call->setScript($script);
        break;
     case 'process_get_voice_input':
        $script = $call->getScript();
        $script->setVar('input_digits', $result->getValue());
        $call->setScript($script);
        break;
      case 'save_record':
        $validators = array();
        $filename=$_FILES['filename']['name'];
        $dest = file_directory_path();
        $file = _voiptropo_file_save_upload($filename, $validators, $dest);
        $script = $call->getScript();
        $script->setVar('recording_path', file_create_url($file->filepath));
        $script->setVar('recording_public_url', file_create_url($file->filepath));
        $script->setVar('recording_digits', NULL);
        $script->setVar('recording_fid', $file->fid);
        $call->setScript($script);
        // update the voipcall with the new values
        $call = $call->save();
        return;
        break;
      case 'process_record':
      case 'process_record_hangup':
        //Save record duration
        $rec_actions = $result->getActions();
        $script = $call->getScript();
        $script->setVar('recording_duration', $rec_actions->duration);
        if($tropo_action) {
          $digits = 'hangup';
        }
        $script->setVar('recording_digits', $digits);
        $call->setScript($script);
        break;
      case 'process_empty_recording':
        $script = $call->getScript();
        $script->setVar('recording_path', '');
        $script->setVar('recording_public_url', '');
        $script->setVar('recording_duration', 0);
        $script->setVar('recording_digits', NULL);
        $script->setVar('recording_fid', NULL);
        $call->setScript($script);
        break;
      case 'process_send_sms':
      case 'process_sms_callback':
        $call = _voiptropo_process_sms_response($call, $dial_response_name);
        break;
    }
  }

watchdog('voiptropo_test', "call 3: " . print_r($call,true));
  // update call duration
  // get call duration from tropo
  $duration = $result->getSessionDuration();
  if (!duration) {
    $end_time = $call->setEndTime();
    $start_time = $call->getStartTime();
    if ($start_time) {
      $duration  = $end_time - $start_time;
    }
  }
  $call->setDuration($duration);

  // update the voipcall with its new values
  $call = $call->save();
watchdog('voiptropo_test', "call 4: " . print_r($call,true));

  // update the call session object
  $cid = $call->getCid();
  if(!$voip_session->getCid()){
    $voip_session->setCid($cid);
  }
  $voip_session->resetTimer();
watchdog('voiptropo_test', "session about to be saved: " . print_r($voip_session, true));
  $voip_session->save();

  return $call;
}
