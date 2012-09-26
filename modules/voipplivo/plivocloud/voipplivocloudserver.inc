<?php

// $Id$

/**
 * @file
 * Implementation of the voipplivocloudServer class
 */


/**
 * File dependencies
 */

module_load_include('inc', 'voip', 'includes/voipserver');

/**
 * Class definitions
 */

class VoipPlivoCloudServer extends VoipServer {

  /*
   * Constructors
   */

  function __construct() {
    $id = 'plivocloud';
    $name = 'Plivo Cloud';
    $config_form = 'voipplivocloud_admin_form';
    parent::__construct($id, $name, $config_form);
  }


  /*
   * Public methods
   */

  /**
   * Implementation of VoipServer() API method
   */

  /*
   * API methods
   */

  /**
   * Implementation of the VoipServer dial() API method
   */
  function dial($voipcall, $server_config = NULL) {

    $rc = FALSE;
    voip_error_reset();

    if (!$server_config) {
      $server_config = $this->getConfig();
    }

    // Plivo configuration
    $api_version = 'v0.1';
    $rest_api_url = $server_config['voipplivocloud_restapi_url'];
    $account_sid = $server_config['voipplivocloud_auth_id'];
    $auth_token = $server_config['voipplivocloud_auth_token'];
	
	$caller_number = ($voipcall->getCallerNumber())
      ? $voipcall->getCallerNumber()
      : variable_get('voipcall_cid_number', NULL);
    $dest_number = $voipcall->getDestNumber();

   // (re)start the session associated with the call
    $voip_session = VoipSession::getSessionByNumber($caller_number, $dest_number);
    if ($voip_session) {
      $voip_session->resetTimer();
	   //we must preserve cid
	  $cid = $voip_session->getCid();
	  //load existing call
	  $voipcall = VoipCall::load($cid);
    }
    else {
      $voip_session = new VoipSession();
      $voip_session->setOriginNumber($caller_number);
      $voip_session->setDestinationNumber($dest_number);
	  
	  // Update call variables
      $voipcall->setSource('drupal');
      $voipcall->setDirection(VoipCall::OUTBOUND);
	  $voipcall->save();
      $cid = $voipcall->getCid();
	  $voip_session->setCid($cid);
    }
    $voip_session->save();

    # Define Channel Variable - http://wiki.freeswitch.org/wiki/Channel_Variables
    $extra_dial_string = "bridge_early_media=true,hangup_after_bridge=true";

    $menu_path = 'voip/plivo/callhandler';
    $response_url = $menu_path . '/process_outbound_calls/' . $cid;
    $call_handler_url = url($response_url, array('absolute' => TRUE));
    $response_url = $menu_path . '/process_hangup/' . $cid;
    $status_callback_url = url($response_url, array('absolute' => TRUE));

    $gateways = $server_config['voipplivocloud_call_gateways'];
    $gateway_codecs = $server_config['voipplivocloud_call_gateway_codec'];
    $gateway_timeouts = $server_config['voipplivocloud_call_gateway_timeouts'];
    $gateway_retries = $server_config['voipplivocloud_call_gateway_retries'];

    $vars = array(
      'From' => $caller_number,
      # Caller Id
      'To' => $dest_number,
      # User Number to Call
      'Gateways' => $gateways,
      # Gateway string to try dialing our separated by comma. First in list will be tried first
      'GatewayCodecs' => $gateway_codecs,
      # Codec string as needed by FS for each gateway separated by comma
      'GatewayTimeouts' => $gateway_timeouts,
      # Seconds to timeout in string for each gateway separated by comma
      'GatewayRetries' => $gateway_retries,
      # Retry String for Gateways separated by comma, on how many times each gateway should be retried
      'ExtraDialString' => $extra_dial_string,
      'AnswerUrl' => $call_handler_url,
      //TODO: Check that hangup_url works
      'HangupUrl' => $status_callback_url,
      //TODO: Whats the purpose of ringing URLs? Is it optional?
      //'RingUrl' => "http://127.0.0.1:5000/ringing/"
    );

    // Instantiate a new Plivo Rest Client
    $client = new PlivoRestClient($rest_api_url, $account_id, $auth_token, $api_version);
    $response = $client->call($vars);

    // update call based on response
    if ($response->IsError) {
      // differentiate between processing errors and higher-level errors
      $plivo_error_code = (string) $response->HttpStatus;
      switch ($plivo_error_code) {
        case '401': // wrong auth token and auth sid or if you your ip is not allowed in ALLOWED_IPS
          $error_msg = t('Wrong auth token and auth sid or ip is not allowed.');
          $voipcall->setErrorMessage($error_msg);
          $voipcall->setCallStatus(VoipCall::ERROR);
          $voipcall->save();
          break;
        default: // processing error
          $plivo_error_msg = $response->ErrorMessage;
          $msg = "Processing failure. Please contact your system's administrator. Plivo error @plivo_code: @plivo_msg";
          $params = array(
            '@plivo_code' => $plivo_error_code,
            '@plivo_msg' => $plivo_error_msg
          );
          $voipcall->report_error('voipplivocloud', $msg, $params);
          break;
      }
      $rc = FALSE;
    }
    else {
      $rc = TRUE;
    }

    return $rc;
  }

  /**
   * Implementation of the VoipServer send_text() API method
   */
  function send_text($text, $voipcall, $reply = FALSE, $server_config = NULL) {
    $rc = FALSE;
    voip_error_reset();

    // determine from/to numbers
    if ($reply) {
      $to = $voipcall->getCallerNumber();
      $from = $voipcall->getDestNumber();
      if (!isset($from)) {
        $voipcall->report_error('voipplivocloud', "Attempt to send a text message reply with no caller number.");
        return FALSE;
      }
    }
    else {
      $to = $voipcall->getDestNumber();
      $from = ($voipcall->getCallerNumber())
        ? $voipcall->getCallerNumber()
        : variable_get('voipcall_cid_number', NULL);
    }

    // (re)start the session associated with the call
    $voip_session = VoipSession::getSessionByNumber($from, $to);
    if ($voip_session) {
      $voip_session->resetTimer();
	  $cid = $voip_session->getCid();
	  //load existing call
	  $voipcall = VoipCall::load($cid);
    }
    else {
      $voip_session = new VoipSession();
      $voip_session->setOriginNumber($from);
      $voip_session->setDestinationNumber($to);
	  
	  // update call variables
      $voipcall->setSource('drupal');
      $voipcall->setDirection(VoipCall::OUTBOUND);
      $voipcall->setCurrentChannel('text');
      $voipcall->setCurrentNetwork('sms');
	  $voipcall->save();
      $cid = $voipcall->getCid();
	  $voip_session->setCid($cid);
    }

    $voip_session->save();

    // send the sms
    if (voipscript_use_sms_framework()) {
      // use SMS Framework to send SMS message if user enabled this option
      voipscript_send_sms_framework($to, $text);
      return TRUE;
    }
    // else use VoIP Drupal:
    // Plivo doesn't support SMS yet
    return FALSE;
  }

  /**
   * Implementation of the VoipServer hangup() API method
   */
  function hangup($voipcall, $server_config = NULL) {
    if (voip_debug()) {
      $id = $voipcall->getCallId();
      $status = $voipcall->getCallStatus();
      watchdog('voipplivocloud', "in hangup() with call id: $id and status: $status");
    }
    $rc = FALSE;
    voip_error_reset();

    if (!$server_config) {
      $server_config = $this->getConfig();
    }

    // Plivo configuration
    $api_version = 'v0.1';
    $rest_api_url = $server_config['voipplivocloud_restapi_url'];
    $account_sid = $server_config['voipplivocloud_auth_id'];
    $auth_token = $server_config['voipplivocloud_auth_token'];

    // Instantiate a new Plivo Rest Client
    $client = new PlivoRestClient($rest_api_url, $account_sid, $auth_token, $api_version);

    $call_id = $voipcall->getCallId();
    if (!$call_id) {
      $msg = "Attempt to hangup a Plivo call (cid = @cid) without a call id";
      $params = array('@cid' => $voipcall->getCid());
      $voipcall->report_error('voipplivocloud', $msg, $params);
      $rc = FALSE;
    }
    else {
      //$plivo_path = "/$api_version/Accounts/$account_sid/Calls/$call_id";
      $vars = array(
        'CallUUID' => $call_id, # Request UUID to hangup call
      );

      $response = $client->hangup_call($vars);

      // update call based on response
      if ($response->IsError) {
        $msg = "Plivo hangup command failed: @error";
        $params = array('@error' => $response->ErrorMessage);
        $voipcall->report_error('voipplivocloud', $msg, $params);
      }
      else {
        $voipcall->setHangupReason(VoipCall::HANGUP_API_REQUEST);
        $voipcall->save();
        $rc = TRUE;
      }
    }

    return $rc;
  }

  /**
   * Implementation of the VoipServer text_handler() API method
   */
  function text_handler($text_content, $origin, $destination, $time, $network, $server_name) {
    $menu_path = 'voip/plivo/callhandler';
    $request_url = $menu_path . '/process_inbound_text/';
    $url = url($request_url, array('absolute' => TRUE));

    $server_config = $this->getConfig();
    $vars = array(
      'AccountSid' => $server_config['voipplivocloud_auth_id'],
      'SmsSid' => $todo,
      'From' => $origin,
      'To' => $destination,
      'Body' => $text_content,
      'Direction' => 'inbound',
      'Channel' => 'offline_text'
    );

    //url-ify the data for the POST
    foreach ($vars as $key => $value) {
      $fields_string .= $key . '=' . urlencode($value) . '&';
    }
    $fields_string = rtrim($fields_string, '&');

    //open connection
    $ch = curl_init();

    //set the url, number of POST vars, POST data
    curl_setopt($ch, CURLOPT_URL, $url);
    curl_setopt($ch, CURLOPT_POST, count($vars));
    curl_setopt($ch, CURLOPT_POSTFIELDS, $fields_string);
    curl_setopt($ch, CURLOPT_RETURNTRANSFER, TRUE);

    //execute post
    $result = curl_exec($ch);

    //close connection
    curl_close($ch);
  }

  /**
   * Implementation of the VoipServer ping() API method
   */
  function ping($server_config = NULL) {

    if (!$server_config) {
      $server_config = $this->getConfig();
    }

    // Plivo configuration
    $auth_id = $server_config['voipplivocloud_auth_id'];
    $auth_token = $server_config['voipplivocloud_auth_token'];

    // Instantiate a new Plivo Rest Client
    try {
	  $client = new RestAPI($auth_id, $auth_token);

      //Request a account information as a way to test the server connectivity
      voip_error_reset();
      $vars = array();
      $response = $client->get_account($vars);
var_dump($response);die();
      if ($response->IsError) {
        voip_error("Plivo Cloud command Ping failed: " . $response->ErrorMessage);
        return FALSE;
      }
      return TRUE;
    } catch (Exception $e) {
      voip_error($e->getMessage());
      return FALSE;
    }
  }

  /**
   * Private methods
   */
  static function _get_available_voices() {
    $voices = array();
    $voices['plivo-arabic'] = array(
      'language' => 'arabic', 'voice' => 'man',
      'description' => t('Male voice in Arabic')
    );
    
	$voices['plivo-chinese-woman'] = array(
      'language' => 'zh', 'region' => 'CN', 'voice' => 'woman',
      'description' => t('Female voice in Chinese')
    );	
	$voices['plivo-chinese-man'] = array(
      'language' => 'zh', 'region' => 'CN', 'voice' => 'man',
      'description' => t('Male voice in Chinese')
    );
	
	$voices['plivo-czech-woman'] = array(
      'language' => 'cs', 'region' => 'CZ', 'voice' => 'woman',
      'description' => t('Female voice in Czech')
    );	
	
	$voices['plivo-danish-woman'] = array(
      'language' => 'da', 'region' => 'dk', 'voice' => 'woman',
      'description' => t('Female voice in Danish')
    );	
	
	$voices['plivo-german-woman'] = array(
      'language' => 'de', 'region' => 'DE', 'voice' => 'woman',
      'description' => t('Female voice in German')
    );	
	$voices['plivo-german-man'] = array(
      'language' => 'de', 'region' => 'DE', 'voice' => 'man',
      'description' => t('Male voice in German')
    );
	
	$voices['plivo-greek-woman'] = array(
      'language' => 'el', 'region' => 'GR', 'voice' => 'woman',
      'description' => t('Female voice in Greek')
    );	
	
	$voices['plivo-australian-woman'] = array(
      'language' => 'en', 'region' => 'AU', 'voice' => 'woman',
      'description' => t('Female voice in English (Australia)')
    );

    $voices['plivo-canada-woman'] = array(
      'language' => 'en', 'region' => 'CA', 'voice' => 'woman',
      'description' => t('Female voice in English (Canada)')
    );	

    $voices['plivo-uk-woman'] = array(
      'language' => 'en', 'region' => 'GB', 'voice' => 'woman',
      'description' => t('Female voice in English (UK)')
    );	
	$voices['plivo-uk-man'] = array(
      'language' => 'en', 'region' => 'GB', 'voice' => 'man',
      'description' => t('Male voice in English (UK)')
    );	
	
	$voices['plivo-us-woman'] = array(
      'language' => 'en', 'region' => 'US', 'voice' => 'woman',
      'description' => t('Female voice in English (US)')
    );	
	$voices['plivo-us-man'] = array(
      'language' => 'en', 'region' => 'US', 'voice' => 'man',
      'description' => t('Male voice in English (US)')
    );	
	
	$voices['plivo-spanish-woman'] = array(
      'language' => 'es', 'region' => 'ES', 'voice' => 'woman',
      'description' => t('Female voice in Spanish')
    );	
	$voices['plivo-spanish-man'] = array(
      'language' => 'es', 'region' => 'ES', 'voice' => 'man',
      'description' => t('Male voice in Spanish')
    );	
	
	$voices['plivo-usspanish-woman'] = array(
      'language' => 'es', 'region' => 'US', 'voice' => 'woman',
      'description' => t('Female voice in Spanish (US)')
    );	
	$voices['plivo-usspanish-man'] = array(
      'language' => 'es', 'region' => 'US', 'voice' => 'man',
      'description' => t('Male voice in Spanish (US)')
    );
	
	$voices['plivo-finland-woman'] = array(
      'language' => 'fi', 'region' => 'FI', 'voice' => 'woman',
      'description' => t('Female voice in Finnish')
    );	
	
	$voices['plivo-franceca-woman'] = array(
      'language' => 'fr', 'region' => 'CA', 'voice' => 'woman',
      'description' => t('Female voice in French (Canada)')
    );	
	$voices['plivo-franceca-man'] = array(
      'language' => 'fr', 'region' => 'CA', 'voice' => 'man',
      'description' => t('Male voice in French (Canada)')
    );	
	
	$voices['plivo-france-woman'] = array(
      'language' => 'fr', 'region' => 'FR', 'voice' => 'woman',
      'description' => t('Female voice in French')
    );	
	$voices['plivo-france-man'] = array(
      'language' => 'fr', 'region' => 'FR', 'voice' => 'man',
      'description' => t('Male voice in French')
    );
	
	$voices['plivo-hungary-woman'] = array(
      'language' => 'hu', 'region' => 'HU', 'voice' => 'woman',
      'description' => t('Female voice in Hungarian')
    );	
	
	$voices['plivo-italy-woman'] = array(
      'language' => 'it', 'region' => 'IT', 'voice' => 'woman',
      'description' => t('Female voice in Italian')
    );	
	$voices['plivo-italy-man'] = array(
      'language' => 'it', 'region' => 'IT', 'voice' => 'man',
      'description' => t('Male voice in Italian')
    );
	
	//bug in Plivo cloud it should be ja-JP instead of jp-JP
	$voices['plivo-japan-woman'] = array(
      'language' => 'ja', 'region' => 'JP', 'voice' => 'woman',
      'description' => t('Female voice in Japanese')
    );	
	$voices['plivo-japan-man'] = array(
      'language' => 'jp', 'region' => 'JP', 'voice' => 'man',
      'description' => t('Male voice in Japanese')
    );
	
	$voices['plivo-dutch-woman'] = array(
      'language' => 'nl', 'region' => 'NL', 'voice' => 'woman',
      'description' => t('Female voice in Dutch')
    );	
	$voices['plivo-dutch-man'] = array(
      'language' => 'nl', 'region' => 'NL', 'voice' => 'man',
      'description' => t('Male voice in Dutch')
    );
	
	$voices['plivo-poland-woman'] = array(
      'language' => 'pl', 'region' => 'PL', 'voice' => 'woman',
      'description' => t('Female voice in Polish')
    );
	
	$voices['plivo-brazil-woman'] = array(
      'language' => 'pt', 'region' => 'BR', 'voice' => 'woman',
      'description' => t('Female voice in Portuguese (Brazil)')
    );
	
	$voices['plivo-portugal-woman'] = array(
      'language' => 'pt', 'region' => 'PT', 'voice' => 'woman',
      'description' => t('Female voice in Portuguese (Portugal)')
    );
	
	$voices['plivo-russian-woman'] = array(
      'language' => 'ru', 'region' => 'RU', 'voice' => 'woman',
      'description' => t('Female voice in Russian')
    );	
	$voices['plivo-russian-man'] = array(
      'language' => 'ru', 'region' => 'RU', 'voice' => 'man',
      'description' => t('Male voice in Russian')
    );
	
	$voices['plivo-sweden-woman'] = array(
      'language' => 'sv', 'region' => 'SE', 'voice' => 'woman',
      'description' => t('Female voice in Swedish')
    );	
	
    return $voices;
  }
}

