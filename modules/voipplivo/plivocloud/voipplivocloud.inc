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
  module_load_include('inc', 'voipplivoframework', 'includes/voipplivocore');
  
}

/**
 * Process voipcall scripts
 *
 * @param $call, VoipCall instance with the call information
 *
 * @return boolean.  TRUE on success or FALSE on failure.  Check voip_error() for processing failure.
 */
function _voipplivocloud_script_handler($call) {
  module_load_include('inc', 'voipplivoframework', 'includes/voipplivocore');
}
