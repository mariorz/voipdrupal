<?php

// $Id$

/**
 * @file
 * Implementation of core functionality associated with voip scripts
 */


/*
 * Class definitions
 */

        
/*
 * Class that keeps track of both the commands as well as the current status of 
 * a VoIP Drupal Script
 */
class VoipScript {
  
  // Script variables

  /**
   * Script name
   */
  private $name;

  /**
   * Array of script commands
   */
  private $commands; 

  /**
   * Index of the next command to be executed
   */
  private $index; // index of the next command to be executed

  /**
   * Script variables
   */
  private $variables;

  /**
   * Stack of scripts being executed via gosub commands.
   *
   * Each element of the array stores the current status of a script,
   * including name, commands, and command index
   */
  private $stack;

  /**
   * Voice being currently used for text-to-speech generation
   */
  private $voice;

  /**
   * Language being currently used for text-to-speech generation
   */
  private $language;

  /**
   * Flag indicating script execution error
   */
  protected $is_error;

  /**
   * Script error message
   */
  protected $error_message;


  /**
   * Constants
   */

  /**
   * Error code
   */
//TODO: do we really need this constant?
  const ERROR='processing_error';

  /**
   * Values associated with the dial command results
   */
  const DIAL_COMPLETED='dial_completed';
  const DIAL_BUSY='dial_busy';
  const DIAL_NO_ANSWER='dial_no_answer';
  const DIAL_FAILED='dial_failed';
  const DIAL_CANCELED='dial_canceled';
 

  /**
   * Indicator of no keypad input received
   */
  const NO_INPUT='no_input';

  /**
   * 'Reject' command options
   */
  const REJECT_INVALID='reject_invalid';
  const REJECT_BUSY='reject_busy';

  /**
   * VoipScript constructor
   *
   * @param $name 
   *   A string containing the name of the new script
   * @param $variables 
   *   An array with the initial value of the script variables
   *
   * @return
   *  The new script created
   */
  function __construct($name, $variables=array()) {
    $this->name = $name;
    $this->commands = array();
    $this->index = 0;
    $this->variables = $variables;
    $this->stack = array();
    $this->is_error = FALSE;
    $this->error_message = NULL;
  }

   /**
   * Returns the name of all the scripts available in the system
   *
   * @return
   *   Array with the script names sorted alphabetically
   */
  public static function getScriptNames() {
    $script_names = array();
    // Invoke hook_voipscript_get_script_names()
    foreach (module_implements('voipscript_get_script_names') as $module) {
      $function = $module .'_'. 'voipscript_get_script_names';
      $new_names = call_user_func($function);
      if(isset($new_names)) {
        // include new script names
        $script_names = array_merge($script_names, $new_names);
      }
    }
    sort($script_names);
    foreach($script_names as $k => $v) $sorted_names[$v] = $v;
    return $sorted_names;
  }



  /**
   * Returns a new instance of an already-existing script
   *
   * @param $script_name
   *   String containing the name of the script to be loaded
   * @param array $params
   *   Array with the value pairs to be passed to hook_voipscript_load_script()
   *
   * @return
   *   The new script instance, or NULL in case of invalid script name
   */
  public static function loadScript($script_name, $params = NULL){
    $rc = NULL;
    $script_tmp = new VoipScript($script_name);
    $script_tmp->runLoad($script_name, $params);
    if(!$script_tmp->isError()) {
      $rc = $script_tmp;
    }
    return $rc;
  }
        

  /*
   * Public methods
   */

  /**
   * Returns the script name
   */
  function getName() {
    return $this->name;
  }

  /**
   * Returns the value of the specified script variable
   *
   * NOTE: This method is only to be used inside VoIP Drupal server modules.
   *       To retrieve the value of a variable from inside the script execution,
   *       use the %variable_name notation as in addSay("You typed: %input").
   *       In this case, %input is going to be replaced by the value of the
   *       script variable 'input'
   */
  function getVar($name) {
    return $this->variables[$name];
  }

  /**
   * Sets the value of the specified script variable
   *
   * NOTE: This method is only to be used inside VoIP Drupal server modules.
   *       VoIP Scripts should use the addSet() method
   */
  function setVar($name, $value) {
    $this->variables[$name] = $value;
  }

  /**
   * Unsets the specified script variable
   */
  function unsetVar($name) {
    unset($this->variables[$name]);
  }

  /**
   * Set the current language of the script
   */
  function setLanguage($language) {
    $this->language = $language;
  }

  /**
   * Return the current language of the script
   */
  function getLanguage() {
    $language = $this->language;
    if(!$language) {
      $server = VoipServer::getDefaultServer();
      $language = $server->getDefaultLanguage();
    }
    return $language;
  }

  /**
   * Set the current voice of the script
   */
  function setVoice($voice) {
    $this->voice = $voice;
  }

  /**
   * Return the current voice of the script
   */
  function getVoice() {
    $voice = $this->voice;
    if(!$voice) {
      $server = VoipServer::getDefaultServer();
      $voice = $server->getDefaultVoice();
    }
    return $voice;
  }


  /*
   * Methods used during script execution
   */

  /**
   * Returns the next script command to be executed
   */
  function getNextCommand() {
    $cmd = $this->commands[$this->index];
    $this->index++;
    return $cmd;
  }

  /**
   * Resets the script index
   */
  function resetIndex() {
    $this->index = 0;
  }

  /**
   * Evaluates the given string based on the current value of script variables.
   *
   * @param $string
   *   The string to be evaluated
   *
   * @return 
   *   The result of the evaluation
   *
   * @see _voipscript_expand_variables() for the actual syntax allowed in the 
   *   string to be evaluated
   */
  function evalString($string) {
    $rc = _voipscript_expand_variables($string, $this->variables);
    return $rc;
  }

  /**
   * Returns TRUE in case a script processing error has been detected
   */
  function isError(){
    return $this->is_error;
  }

  /**
   * Retuns the message associated with a script error.
   */
  function getErrorMessage() {
    return $this->error_message;
  }

  /*
   * Supported script commands
   */

  /**
   * Adds a "dial" command to the script.
   *
   * When executed, the 'dial' command connects the current caller to another
   * phone. If the called party picks up, the two parties are connected and
   * can communicate until one hangs up. If the called party does not pick up,
   * or if a busy signal is received, or the number doesn't exist, the dial
   * command will finish.
   *
   * @param $number
   *   String with a valid number to be dialed
   *
   * @param $timeout
   *   Integer with the number of seconds to wait for the called party to
   *   answer the call
   *
   * @param $hangup_on_star
   *   If TRUE, lets the calling party hang up on the called party by pressing
   *   the '*' key on his phone. This feature allows the calling party to hang
   *   up on the called party without having to hang up the phone and end the
   *   session. When the caller presses '*' Voip Drupal will hang up on the
   *   called party and will move on to execute the next command in the script.
   *
   * @param $time_limit
   *   Integer with the maximum duration of the call in seconds
   *
   * @param $caller_id
   *   String with the caller id to be used for the call. By default, this is
   *   the caller id defined in the 'default call configuration' settings
   *
   * @return
   *   The 'dial' command sets the script variable dial_status with the
   *   outcomes of the call:
   *   - VoipScript::DIAL_COMPLETED
   *   - VoipScript::DIAL_BUSY
   *   - VoipScript::DIAL_NO_ANSWER
   *   - VoipScript::DIAL_FAILED
   *   - VoipScript::DIAL_CANCELED
   *
   *   If the call was successful, the variable dial_duration gets set with the
   *   number of seconds that the call lasted for.
   */
  function addDial($number, $timeout=30, $hangup_on_star=false, $time_limit=3600, $caller_id=''){
    $params['number'] = $number;
    $params['timeout'] = $timeout;
    $params['hangup_on_star'] = $hangup_on_star;
    $params['time_limit'] = $time_limit;
    $params['caller_id'] = $caller_id;
    $this->commands[] = new VoipCommand('VoipCmdDial', $params);
  }

  /*
   * Adds a 'get input' command to the script. 
   *
   * When executed, the 'get input' command reads user input from the phone
   * keypad. The system stops reading the input whenever there is a
   * timeout, the user presses a predefined key, or the users presses the
   * maximum number of digits expected.
   *
   * @param $prompt 
   *   String combining text and audio URLs to be played to the user prompting
   *   him/her to type something.
   *
   * @param $timeout
   *   Integer specifying the maximum number of seconds to wait in between
   *   digits
   *
   * @param $end_key
   *   Key used to end the data input. The allowed values are the numbers 0-9,
   *   # , * and the empty string.
   *
   * @param $num_digits
   *   Integer with the number of digits that are expected.
   *
   * @return
   *   The 'get input' command sets the script variable input_digits to the
   *   input received.  In case nothing is typed, input_digits gets set to
   *   VoipScript::NO_INPUT
   */
// TODO: consider creating a addIvr() command to generate a IVR menu and simplify the way getInput is used... That command should automatically check for valid inputs, repeat the question a number of times, etc.
  function addGetInput($prompt='', $timeout=5, $end_key='#', $num_digits=NULL) {
    $params['prompt'] = $prompt;
    $params['timeout'] = $timeout;
    $params['end_key'] = $end_key;
    $params['num_digits'] = $num_digits;
    $this->commands[] = new VoipCommand('VoipCmdGetInput', $params);
  }

  /**
   * Adds a "gosub" command to the script.
   *
   * When executed, the 'gosub' command saves the state of the current script
   * into the local stack and loads the commands and variables of specified
   * script.
   * 
   * @param $script_name
   *   The name of the script to be loaded
   *
   * @see VoipScript::addReturn()
   */
  function addGosub($script_name) {
    $params['script_name'] = $script_name;
    $this->commands[] = new VoipCommand('VoipCmdGosub', $params);
  }

  /** 
   * Adds a "go to" command to the script.
   * 
   * When ran, the 'go to' command updates the index of the script and skips
   * the script execution to the command that follows the specified label.
   *
   * @param $label_name
   *   String with the name of the label to go to.
   *
   * @see VoipScript::addLabel()
   * @see VoipScript::addGotoIf()
   */ 
  function addGoto($label_name) {
    $params['label_name'] = $label_name;
    $this->commands[] = new VoipCommand('VoipCmdGoto', $params);
  }

  /**
   * Adds a 'goto if' command to the script.
   *
   * This command is similar to VoipScript:addGoto(). However, it only goes to
   * the specified label in case the specified condition evaluates to TRUE.
   *
   * @param $label_name
   *   String with the name of the label to go to.
   * @param $condition
   *   If TRUE, skips script execution to the specified label.
   *
   * @see VoipScript::addLabel()
   * @see VoipScript::addGoto()
   */
  function addGotoIf($label_name, $condition) {
    $params['label_name'] = $label_name;
    $params['condition'] = $condition;
    $this->commands[] = new VoipCommand('VoipCmdGotoIf', $params);
  }

  /**
   * Adds a 'hangup' command to the script.
   *
   * When executed, the 'hangup' command ends the call.
   */
  function addHangup() {
    $this->commands[] = new VoipCommand('VoipCmdHangup');
  }

  /**
   * Adds a 'join conference' command to the script.
   *
   * When executed, the 'join conference' command connects the current call
   * to the specified conference room.
   *
   * @param $room 
   *   String with the name of the conference call room
   *
   * @param $muted
   *   Boolean indicating whether the participant can speak on the conference
   *
   * @param $hangup_on_star
   *   If TRUE, allows participants to leave the conference by pressing '*'
   *
   * @return
   *   The 'join conference' command sets the script variable dial_status with
   *   the outcomes of the conference call:
   *   - VoipScript::DIAL_COMPLETED
   *   - VoipScript::DIAL_FAILED
   *
   */
  function addJoinConference($room, $muted=FALSE, $hangup_on_star=true) {
    $params['room'] = $room;
    $params['muted'] = $muted;
    $params['hangup_on_star'] = $hangup_on_star;
    $this->commands[] = new VoipCommand('VoipCmdJoinConference', $params);
  }

  /**
   * Adds a label to the script.
   *
   * @see VoipScript::addGoto()
   * @see VoipScript::addGotoIf()
   */
  function addLabel($label_name) {
    $params['label_name'] = $label_name;
    $this->commands[] = new VoipCommand('VoipCmdLabel', $params);
  }

  /**
   * Adds a 'record' command to the script.
   *
   * When executed, the 'record' command records audio input from the phone.
   * The system stops recording whenever there is a timeout, the user presses    * the predefined 'end' key, or the maximum length of the recording is
   * reached.
   *
   * @param $prompt 
   *   String combining text and audio URLs to be played to the user prompting
   *   him/her to start recording.
   *
   * @param $timeout
   *   Integer specifying the maximum number of seconds of silence to wait
   *   before ending the recording.
   *
   * @param $end_key
   *   Key used to end the recording. The allowed values are the numbers 0-9,
   *   # , * and the empty string.
   *
   * @param $max_length
   *   Integer with the maximum number of seconds allowed for the recording
   *
   * @param $format
   *   The format in which to record. By default, servers should support at
   *   least the 'mp3' format
   *
   * @return
   *   The execution of the 'record' command sets the following script
   *   variables: 
   *   - recording_url, with the location of the file recorded
   *   - recording_duration, with the duration in seconds of the recording
   *   - recording_digits, the key (if any) pressed to end the recording
   */
  function addRecord($prompt='', $timeout=5, $end_key='#', $max_length=3600, $format='mp3') {
    $params['prompt'] = $prompt;
    $params['timeout'] = $timeout;
    $params['end_key'] = $end_key;
    $params['max_length'] = $max_length;
    $params['format'] = $format;
    $this->commands[] = new VoipCommand('VoipCmdRecord', $params);
  }

  /**
   * Adds a 'reject' command to the script.
   *
   * Rejects an incoming call without billing you. Has to be the first command
   * to be executed for an incoming call.
   */
  function addReject($reason=VoipScript::REJECT_INVALID) {
    $params['reason'] = $reason;
    $this->commands[] = new VoipCommand('VoipCmdReject', $params);
  }

  /**
   * Add a 'return' command to the script.
   *
   * Returns call control to the previous script in the script stack.
   */
  function addReturn() {
    $this->commands[] = new VoipCommand('VoipCmdReturn');
  }

  /**
   * Add a 'say' command to the script.
   *
   * Says the given prompt the specified number of times in the given voice.
   *
   * @param $prompt
   *   String combining text to be converted to speech and audio URLs to be
   *   played.
   *
   * @param $as
   *   String with value either 'number' or 'digits'.
   *
   * @param $loop
   *   Integer with the number of times that the prompt should be repeated
   *
   * @param $voice DEPRECATED
   *   String with the type of voice to be used.  If NULL, use the script's
   *   current voice as defined by the 'voipscript_voice' script variable. In
   *   case that variable is not set, use the default 'voice' setting defined
   *   by the voip server.
   *
   * @param $language. DEPRECATED
   *   String with the language entonation associated with the voice. If NULL,
   *   use the script's current language as defined by the 
   *   'voipscript_language' script variable. If that variable is not set,
   *   use the default 'language' setting defined by the voip server.
   * 
   *
   * @return
   *   The execution of the 'say' command does not return anything.
   */
  /*function addSay($prompt, $loop=1, $voice=NULL, $language=NULL) {
    $params['prompt'] = $prompt;
    $params['loop'] = $loop;
    $params['voice'] = $voice;
    $params['language'] = $language;
    $this->commands[] = new VoipCommand('VoipCmdSay', $params);
  }*/
  
  function addSay($prompt, $as='', $loop=1) {
    $params['prompt'] = $prompt;
    $params['loop'] = $loop;
    $params['as'] = $as;
    $this->commands[] = new VoipCommand('VoipCmdSay', $params);
  }

  /** 
   * Adds a "send sms" command to the script.
   * 
   * When ran, this command sends the given text to the specified phone number. 
   * The outcome of the command is stored in the 'sms_status' variable.
   * 
   * @uses VoipScript::$variables
   *
   * @param $text
   *   String with the text to be sent
   *
   * @param $to
   *   For SMS string with the SMS number, or string with destination username for IM. When sending on an 
   *   incoming call, 'to' defaults to the caller.  When sending on
   *   an outgoing call, 'to' defaults to the called party.
   *
   * @param $channel
   *   String withe channel to be used. By default the channel will be SMS. 
   *   Allowed options are SMS, AIM, MSN, JABBER, YAHOO and GTALK.
   * @param $caller_id
   *   String with the caller id to be used. It can be either a number or IM username. When sending on an incoming 
   *   call, 'caller_id' defaults to the called party.  When sending
   *   on an outgoing call, 'caller id' defaults to the calling party.
   * 
   * @return
   *   The execution of the 'send sms' command sets the variable sms_status to 
   *   any of the following values:
   *   - VoipCall::SMS_FAILED
   *   - VoipCall::SMS_INVALID
   *   - VoipCall::SMS_SENDING
   *   - VoipCall::SMS_SENT
   */ 
  /*function addSendSms($text, $number='', $caller_id=''){
    $params['number'] = $number;
    $params['text'] = $text;
    $params['caller_id'] = $caller_id;
    $this->commands[] = new VoipCommand('VoipCmdSendSms', $params);
  }*/
  function addSendText($text, $to='', $channel='sms', $caller_id=''){
    $params['to'] = $to;
    $params['text'] = $text;
    $params['channel'] = $channel;
    $params['caller_id'] = $caller_id;
    $this->commands[] = new VoipCommand('VoipCmdSendText', $params);
  }

  /**
   * Set the value of the specified script variable
   */
  function addSet($var_name, $var_value) {
    $params['var_name'] = $var_name;
    $params['var_value'] = $var_value;
    $this->commands[] = new VoipCommand('VoipCmdSet', $params);
  }

  /**
   * Set the current language of the script
   */
  function addSetLanguage($language='en') {
    $params['language'] = $language;
    $this->commands[] = new VoipCommand('VoipCmdSetLanguage', $params);
  }

  /**
   * Set the current voice of the script
   */
  function addSetVoice($voice='man') {
    $params['voice'] = $voice;
    $this->commands[] = new VoipCommand('VoipCmdSetVoice', $params);
  }

  /**
   * Unset the specified script variable
   */
  function addUnset($var_name) {
    $params['var_name'] = $var_name;
    $this->commands[] = new VoipCommand('VoipCmdUnset');
  }


  /*
   * Execution of basic script commands that are not dependent on the voip server
   */

  function runGosub($script_name) {
    $script_state = array('name' => $this->name,
                          'commands' => $this->commands,
                          'index' => $this->index);
    $this->stack[] = $script_state;
    $rc =  $this->runLoad($script_name);
    return $rc;
  }

  function runGoto($label_name) {
    $rc = TRUE;
    $found = FALSE;
    $i = 0;
    while(!$found) {
      $cmd = $this->commands[$i];
      if(!$cmd) {
        // label nor found
        $this->is_error = TRUE;
        $script_name = $this->name;
        $this->error_message = "Label '$label_name' not found in script '$script_name': " . print_r($this, true);
        $rc = FALSE;
        $found = TRUE;
      }
      else {
        if($cmd->getId() == 'VoipCmdLabel') {
          $string_tmp = $cmd->getParam('label_name');
          $label_name_tmp = $this->evalString($string_tmp);
          if(strcasecmp($label_name_tmp, $label_name) == 0) {
            $this->index = $i;
            $found = TRUE;
          }
        }
        // move to the next command in the script
        $i++;
      }
    }
    return $rc;
  }

  /**
   * Load the specified script
   *
   * @param $script_name
   *  string, name of script to be loaded
   * 
   * @param $params
   *  array, optional parameters 
   *  to be passed to implementations of hook_voipscript_load_script
   */
  function runLoad($script_name, $params = NULL) {
    $rc = TRUE;
    $found = FALSE;

    // Invoke hook_voipscript_load_script() giving modules the 
    // opportunity to return a custom script. 
    foreach (module_implements('voipscript_load_script') as $module) {
      $function = $module .'_'. 'voipscript_load_script';
      $new_script = call_user_func($function, $script_name, $params);
      if(isset($new_script)) {
        $found = TRUE;
        // loads new name and commands
        $this->name = $new_script->getName();
        $this->commands = $new_script->getCommands();
        $this->index = 0;
        // override variables with the same name and preserve the others
        $this->variables = array_merge($this->variables, $new_script->getVariables());
        // preserve the stack as is
          // $this->stack = array();
        $this->is_error = FALSE;
        // scripts must have unique names. exit the foreach
        break;
      }
    }

    if(!$found) {
      // no script found with the given name
      $rc = FALSE;
      $this->is_error = TRUE;
      $this->error_message = "Script '$script_name' not found for script '" . $this->name . "': " . print_r($this->commands, TRUE);
    }

    return $rc;
  } 

  function runReturn() {
    $rc = TRUE;
    $script_state = array_pop($this->stack);
    if ($script_state) {
      $this->name = $script_state['name'];
      $this->commands = $script_state['commands'];
      // Note: index is already pointing to the next command to be executed
      $this->index = $script_state['index'];
    }
    else {
      // invalid return command
      $this->is_error = TRUE;
      $this->error_message = "Invalid return command for script '" . $this->name . "': " . print_r($this, true);
    }
    return $rc;
  }


  /**
   * Protected methods
   */

  protected function getCommands() {
    return $this->commands;
  }

  protected function getVariables() {
    return $this->variables;
  }

  // the following function is provided in case there is a need to
  // extend the script API to include additional commands
  protected function addCommand($command) {
    $this->commands[] = $command;
  }

}


/**
 * Implementation of command-related classes
 */

class VoipCommand {

  private $id; // command id
  private $params; // command parameters

  function __construct($id, $params=array()) {
    $this->id = $id;
    $this->params = $params;
  }

  function getId() {
    return $this->id;
  }

  function getParam($param_name, $default=NULL) {
    return $this->params[$param_name]?$this->params[$param_name]:$default;
  }
}
    


/**
 * Internal functions
 */

/**
 * The following function recursively replaces predefined variables from
 * inside the given string.  It was based on code made available at 
 * http://stackoverflow.com/questions/3241070/php-recursive-variable-replacement
 *
 * Notes:
 * - Strings that start with '^' are treated as expressions to be evaluated
 * - Placeholders for script variables start with '%'
 * - This function is smart enough to process variables referenced by
 *   other variables
 * - There is no check against circular inclusion, which might simply lead
 *   to an infinite loop. (Example: $vars['s'] = '%s'; ..) 
 * - When defining expressions that include string variables, make sure the
 *   variable placeholder is enclosed in " or '.  For instance,
 *     Correct form:   "^print_r('The content is ' . '%msg');"
 *     Incorrect form: "^print_r('The content is ' . %msg);"
 *   The incorrect form might produce a parser error if the variable msg
 *   contains characters such as a space, math sign, etc... It might also
 *   produce undesirable results if the variable starts with 0.
 */
function _voipscript_expand_variables($str, $vars) {
  $eval = substr($str, 0, 1) == '^';
  $regex = '/\%(\w+)/e';
  $replacement = "_voipscript_replace_variable(\$1, \$vars, \$eval);" ;
  $res = preg_replace($regex, $replacement ,$str);
  if($eval) {
    ob_start();
    $expr = substr($res, 1);
    if(eval('$res = ' . $expr . ';')===false) {
      ob_end_clean();
        watchdog('voipscript', 'Invalid PHP expression: @expr', array('@expr' => $expr), WATCHDOG_ERROR);
    }
    ob_end_clean();
  }
  return $res;
}

function _voipscript_replace_variable($var_name, $vars, $eval) {
  if(isset($vars[$var_name])) {
    $expanded = _voipscript_expand_variables($vars[$var_name], $vars);
    if($eval) {
      // Special handling since $str is going to be evaluated ..
      if(!is_numeric($expanded) || (substr($expanded . '', 0, 1)==='0'
          && strpos($expanded . '', '.')===false)) {
        $expanded = "'$expanded'";
      }
    }
    return $expanded;
  } else {
    // Variable does not exist in $vars array
    if($eval) {
      return 'null';
    }
//  return "$var_name";
    return '';
  }
}

