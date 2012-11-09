<?php
// $id$

/**
 * @file
 * Implementation of Plivo-related functionality
 */


/**
 * Internal functions
 */

/*
 * Process incoming requests and responses from Plivo
 *
 * @param string $plivo_action set to the action to be executed.
 *   It can assume any of the following values:
 *   - process_conference, to handle responses to conference call requests
 *   - process_inbound_calls, to handle incoming calls
 *   - process_inbound_text, to handle incoming text
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
 * @param string $cid with the cid of the call
 *
 * @return void
 */
function _voipplivocloud_call_handler($plivo_action, $cid = NULL) {
  module_load_include('inc', 'voipplivocloud', 'includes/voipplivocore');
  $server = 'plivocloud';
  return voipplivocore_call_handler($server, $plivo_action, $cid);
}

function _voipplivocloud_validate_request() {
  //todo: hardcoding this until i figure out how to validate request
  return TRUE;

  $server = VoipServer::getServer('plivocloud');
  $server_config = $server->getConfig();
  $auth_token = $server_config['voipplivocloud_auth_token'];


  // Note, that if your URL uses an implied "index" document
  // (index.php), then apache often adds a slash to the SCRIPT_URI
  // while Plivo's original request will not have a slash
  // Example: if Plivo requested http://mycompany.com/answer
  //   and that url is handled by an index.php script
  //   Apache/PHP will report the URI as being:
  //   http://mycompany.com/answer/
  //   But the hash should be calculated without the trailing slash

  // Also note, if you're using URL rewriting, then you should check
  // to see that PHP is reporting your SCRIPT_URI and
  // QUERY_STRING correctly.

  if ($_SERVER['HTTPS']) {
    $http = "https://";
  }
  else {
    $http = "http://";
  }

  $url = $http . $_SERVER['HTTP_HOST'] . $_SERVER['REQUEST_URI']."/";

  if (isset($_POST)) {
    // copy the post data
    $post_params = $_POST;
  }

  $signature = $_SERVER["HTTP_X_PLIVO_SIGNATURE"];
  watchdog('voipplivocloud', "server ".print_r($_SERVER,true));
  watchdog('voipplivocloud', "url $url signature $signature auth_token $auth_token post params ".print_r($post_params,true));
  if (validate_signature($url, $post_params, $signature, $auth_token)) {
    return TRUE;
  }
  watchdog('voipplivocloud', "failed validating");
  return FALSE;
}
