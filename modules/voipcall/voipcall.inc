<?php

// $Id$

/**
 * @file
 * Implementation of core functionality associated with voip calls
 */


/*
 * Class definitions
 */

        
/*
 * Class that keeps track of both the commands as well as the current status of 
 * a voip call
 */
class VoipCall {

  /**
   * Instance variables
   */

  /**
   * VoipCall node
   */
   private $call_node;


  /**
   * Constants
   */

  /**
   * 'call status' field values
   */
  const ERROR='processing_error';

  const NOT_CALLED='not_called'; // the call has yet to be called
  const QUEUED='queued'; // the call is waiting in line before going out

// TODO: replace DIALING by RINGING
  const RINGING='ringing'; // the call is currently ringing
  const IN_PROGRESS='in_progress'; // the call is in progress

  const COMPLETED='completed';
  const NO_ANSWER='no_answer';
  const BUSY='busy';
  const INVALID='invalid_number';
  const CANCELED='canceled'; // canceled by the system while queued or ringing

//TODO: deprecate MACHINE_HANGUP

  /**
   * 'direction' field values
   */
  const INBOUND='inbound';
  const OUTBOUND='outbound';
  const OUTBOUND_DIAL='outbound_dial';

  /**
   * 'answered by' field values
   */
  const NOT_CHECKED='not_checked';
  const ANSWERED_DRUPAL='answered_by_drupal';
  const ANSWERED_MACHINE='answered_by_machine';
  const ANSWERED_HUMAN='answered_by_human';

  /**
   * 'text status' field values
   */
  const TEXT_FAILED='text_failed';
  const TEXT_INVALID='text_invalid';
  const TEXT_NOT_SENT='text_not_sent';
  const TEXT_SENDING='text_sending';
  const TEXT_SENT='text_sent';

  /**
   * 'hangup reason' field values
   */
  const HANGUP_BY_USER='hangup_by_user'; // call hangup by phone user
  const HANGUP_API_REQUEST='hangup_api_request'; // requested via server API
  const HANGUP_BUSY='hangup_busy';
  const HANGUP_INVALID_NUMBER='hangup_invalid_number';
  const HANGUP_NO_ANSWER='hangup_no_answer';
  const HANGUP_REJECTED='hangup_rejected';
  const HANGUP_SCRIPT_COMMAND='hangup_script_command';
  const HANGUP_SCRIPT_END='hangup_script_end';
  const HANGUP_SYSTEM_ERROR='hangup_system_error';


  /**
   * VoipCall constructor
   */
  function __construct($call_node=NULL) {
    if(is_array($call_node)){
      $call_node = (object)$node;
    }
    $this->call_node = $call_node;
  }


  /**
   * Static methods
   */

  /**
   * Load a call from the database
   *
   * @param $call_nid
   *   The nid of the call node
   *
   * @return
   *   The VoipCall instance, or NULL in case of an invalid nid
   */
  public static function load($call_nid){
    $call = NULL;
    $call_node = node_load($call_nid);
    if($call_node){
      // unserialize the call script
// TODO: move the unserialization to inside _nodeapi()
      $script = unserialize($call_node->field_voipcall_script[0]['value']);
      $call_node->field_voipcall_script[0]['value'] = $script;
      $call = new VoipCall($call_node);
    }
    return $call;
  }
        
  /**
   * Return the voipcall nid associated with the given call_id.
   * 
   * @param string $call_id, the unique identifier of a call
   *
   * @return $nid or NULL depending on the result of the query
   */
  public static function getNidFromCallId($call_id){
    // fetch the desired voipcall node
    $call_nid = db_result(db_query("SELECT `nid` FROM `content_type_voipcall` WHERE `field_voipcall_call_id_value` = '%s'", $call_id));
    return $call_nid;
  }



  /*
   * Public methods
   */

  /**
   * Save the call node to the database
   *
   * @return
   *   The updated VoipCall instance, or NULL in case of an invalid nid
   */
  public function save(){

    $call_node = $this->_voipcall_save($this->call_node);
    // unserialize the call script
    $script = unserialize($call_node->field_voipcall_script[0]['value']);
    $call_node->field_voipcall_script[0]['value'] = $script;
    $this->call_node = $call_node;
    return $this;
  }
        

  /**
   * Check if the call is in a hangup state
   */
  public function isHangup() {
    $rc = FALSE;
    $status = $this->call_node->field_voipcall_status[0]['value'];
    switch($status) {
      case VoipCall::BUSY:
      case VoipCall::CANCELED:
      case VoipCall::ERROR:
      case VoipCall::INVALID:
      case VoipCall::NO_ANSWER:
      case VoipCall::COMPLETED:
        $rc = TRUE;
        break;
    }
    return $rc;
  }


  /**
   * Report a voipcall-related processing error
   *
   * @param string $module with the name of the module reporting the error
   *
   * @param string $msg with the error message with placeholders for the params
   *
   * @param array $params with pairs string->string that will replaced in $msg
   *
   * @return void
   */
  public function report_error($module, $msg, $params=array()){
    watchdog($module ,$msg, $params, WATCHDOG_ERROR);
    $msg_text = "($module) " . strtr($msg, $params);
    voip_error($msg_text);
    $this->setErrorMessage($msg_text);
    $this->setCallStatus(VoipCall::ERROR);
    $this->setHangupReason(VoipCall::HANGUP_SYSTEM_ERROR);
    $this->save();
  }



  /**
   * Getters and setters
   */

  public function getCallNode(){
    // NOTE: in order to protect the architecture of this class, please
    // only use this method to access generic node info such as nid, uid, etc.
    return $this->call_node;
  }

  public function getNodeId(){
    return $this->call_node->nid;
  }
    
  public function getUid(){
    return $this->call_node->uid;
  }
    
  public function setUid($uid){
    $this->call_node->uid = $uid;
  }
    
  public function getCallId(){
    return $this->call_node->field_voipcall_call_id[0]['value'];
  }
    
  public function setCallId($cid){
    $this->call_node->field_voipcall_call_id[0]['value'] = $cid;
  }
    
  public function getDestNumber(){
    return $this->call_node->field_voipcall_dest_number[0]['value'];
  }

  public function setDestNumber($number){
    $this->call_node->field_voipcall_dest_number[0]['value'] = $number;
  }

  public function getDestName(){
    return $this->call_node->field_voipcall_dest_name[0]['value'];
  }

  public function setDestName($name){
    $this->call_node->field_voipcall_dest_name[0]['value'] = $name;
  }

  public function getCallerNumber(){
    return $this->call_node->field_voipcall_caller_number[0]['value'];
  }

  public function setCallerNumber($number){
    $this->call_node->field_voipcall_caller_number[0]['value'] = $number;
  }

  public function getCallerName(){
    return $this->call_node->field_voipcall_caller_name[0]['value'];
  }

  public function setCallerName($name){
    $this->call_node->field_voipcall_caller_name[0]['value'] = $name;
  }

  public function getCallStatus(){
    return $this->call_node->field_voipcall_status[0]['value'];
  }
    
  public function setCallStatus($status){
    $this->call_node->field_voipcall_status[0]['value'] = $status;
  }
    
  public function getCallStatusDescription(){
    $description = NULL;
    $status = $this->getCallStatus();
    if(!$status){
      $description = t('Not set.');
    }
    else {
      switch($status){
        default:
          $module = "voipcall";
          $msg = t('Invalid status passed to getCallStatusDescription(): @status.');
          $params = array('@status' => $status);
          $reason = $this->report_error($module, $msg, $params);
          break;
        case VoipCall::ERROR:
          $description = t('System error.');
          break;
        case VoipCall::NOT_CALLED:
          $description = t('Not called.');
          break;
        case VoipCall::QUEUED:
          $description = t('Queued.');
          break;
        case VoipCall::RINGING:
          $description = t('Ringing.');
          break;
        case VoipCall::IN_PROGRESS:
          $description = t('In progress.');
          break;
        case VoipCall::COMPLETED:
          $description = t('Completed.');
          break;
        case VoipCall::NO_ANSWER:
          $description = t('No answer.');
          break;
        case VoipCall::BUSY:
          $description = t('Busy.');
          break;
        case VoipCall::INVALID:
          $description = t('Invalid number.');
          break;
        case VoipCall::CANCELED:
          $description = t('Canceled.');
          break;
      }
    }
    return $description;
  }

  public function getAnsweredBy(){
    return $this->call_node->field_voipcall_answered_by[0]['value'];
  }

  public function setAnsweredBy($answered_by){
    $this->call_node->field_voipcall_answered_by[0]['value'] = $answered_by;
  }

  public function getTextStatus(){
    return $this->call_node->field_voipcall_text_status[0]['value'];
  }

  public function setTextStatus($status){
    $this->call_node->field_voipcall_text_status[0]['value'] = $status;
  }
    
  public function getSource(){
    return $this->call_node->field_voipcall_source[0]['value'];
  }

  public function setSource($source){
    $this->call_node->field_voipcall_source[0]['value'] = $source;
  }

  public function getDirection(){
    return $this->call_node->field_voipcall_direction[0]['value'];
  }

  public function setDirection($direction){
    $this->call_node->field_voipcall_direction[0]['value'] = $direction;
  }

  public function getStartTime(){
    return $this->call_node->field_voipcall_start_time[0]['value'];
  }

  public function setStartTime($time=NULL){
    if(!$time) $time = time();
    return $this->call_node->field_voipcall_start_time[0]['value'] = $time;
  }

  public function getEndTime(){
    return $this->call_node->field_voipcall_end_time[0]['value'];
  }

  public function setEndTime($time=NULL){
    if(!$time) $time = time();
    return $this->call_node->field_voipcall_end_time[0]['value'] = $time;
  }

  public function getDuration(){
    return $this->call_node->field_voipcall_duration[0]['value'];
  }

  public function setDuration($time){
    return $this->call_node->field_voipcall_duration[0]['value'] = $time;
  }

  public function getErrorMessage(){
    return $this->call_node->field_voipcall_error_message[0]['value'];
  }

  public function setErrorMessage($msg){
    $this->call_node->field_voipcall_error_message[0]['value'] = $msg;
  }


  public function getScript(){
    $script = $this->call_node->field_voipcall_script[0]['value'];
    $updated_script = $this->_voipcall_update_script($script);
    return $updated_script;
  }

  public function setScript($script) {
    $this->call_node->field_voipcall_script[0]['value'] = $script;
  }


  public function getHangupScriptName(){
    return $this->call_node->field_voipcall_hangup_script[0]['value'];
  }

  public function setHangupScriptName($name){
    $this->call_node->field_voipcall_hangup_script[0]['value'] = $name;
  }

  public function getHangupReason(){
    return $this->call_node->field_voipcall_hangup_reason[0]['value'];
  }

  public function setHangupReason($reason){
    $this->call_node->field_voipcall_hangup_reason[0]['value'] = $reason;
  }

  public function getHangupReasonDescription(){
    $description = NULL;
    $reason = $this->getHangupReason();
    if(!$reason){
      $description = t('Not hangup.');
    }
    else {
      switch($reason){
        default:
          $module = "voipcall";
          $msg = t('Invalid reason passed to getHangupSystemDescription(): @reason.');
          $params = array('@reason' => $reason);
          $reason = $this->report_error($module, $msg, $params);
          break;
        case VoipCall::HANGUP_BY_USER:
          $description = t('Call hangup by either caller or callee.');
          break;
        case VoipCall::HANGUP_API_REQUEST:
          $description = t('Call completed due to server request.');
          break;
        case VoipCall::HANGUP_BUSY:
          $description = t('Busy line.');
          break;
        case VoipCall::HANGUP_INVALID_NUMBER:
          $description = t('Invalid number.');
          break;
        case VoipCall::HANGUP_NO_ANSWER:
          $description = t('No answer.');
          break;
        case VoipCall::HANGUP_REJECTED:
          $description = t('Call rejected.');
          break;
        case VoipCall::HANGUP_SCRIPT_COMMAND:
          $description = t('Call completed due to dialplan script request.');
          break;
        case VoipCall::HANGUP_SCRIPT_END:
          $description = t('Reached end of dialplan script.');
          break;
        case VoipCall::HANGUP_SYSTEM_ERROR:
          $description = t('System error.');
          break;
      }
    }
    return $description;
  }

  public function getCheckMachine(){
    return $this->call_node->field_voipcall_check_machine[0]['value'];
  }

  public function setCheckMachine($bool){
    $this->call_node->field_voipcall_check_machine[0]['value'] = $bool;
  }


  /* 
   * Private methods
   */

  /**
   * Save VoipCall nodes. 
   * 
   * @param 
   *  $details, VoipCall instance with the fields to be saved
   * 
   * @return
   *  $node, the newly saved call_node node.
   */
  private function _voipcall_save($details=NULL) {
  
    if(is_array($details)){
      $details = (object)$details;
    }
  
    // if this is a new call node being created
    if(is_null($details->nid)){
  // TODO Why does this get called so many times when a call goes out? 
  // TODO Why doesn't this save new nodes every time a call blast goes out, e.g. unchecking blast sent ?
  
      $node = new stdClass();
  
      // set the node type
      $node->type = 'voipcall';

      // set the node uid
      global $user;
      $node->uid = $user->uid;
  
      // initialize the 'Call Id' field
      if(!isset($details->field_voipcall_call_id[0]['value'])) {
        $node->field_voipcall_call_id[0]['value'] = NULL;
      }
  
      // initialize 'Call Status' field
      if(!isset($details->field_voipcall_status[0]['value'])) {
        $node->field_voipcall_status[0]['value'] = VoipCall::NOT_CALLED;
      }
  
      // initialize 'Answered By' field
      if(!isset($details->field_voipcall_answered_by[0]['value'])) {
        $node->field_voipcall_answered_by[0]['value'] = VoipCall::NOT_CHECKED;
      }
  
      // initialize 'Text Status' field
      if(!isset($details->field_voipcall_text_status[0]['value'])) {
        $node->field_voipcall_text_status[0]['value'] = VoipCall::TEXT_NOT_SENT;
      }

      // initialize 'Call Source' field
      if(!isset($details->field_voipcall_source[0]['value'])) {
      $node->field_voipcall_source[0]['value'] = 'drupal';
    }
  
      // initialize 'Call Direction' field
      if(!isset($details->field_voipcall_direction[0]['value'])) {
        $node->field_voipcall_direction[0]['value'] = VoipCall::OUTBOUND;
      }
  
      // initialize voice call script with the default inbound/outbound one
      if(!isset($details->field_voipcall_script[0]['value'])) {
        $dir_tmp = $details->field_voipcall_direction[0]['value'];
        $direction = $dir_tmp ? $dir_tmp
                              : $node->field_voipcall_direction[0]['value'];
        if($direction == VoipCall::INBOUND){
          $script_name = voipcall_get_default_inbound_call_script_name();
        }
        else {
          $script_name = voipcall_get_default_outbound_call_script_name();
        }
        $default_script = VoipScript::loadScript($script_name);
        $node->field_voipcall_script[0]['value'] = serialize($default_script);
      }
  
/** TODO: Awaiting SMS integration
      // initialize text script with the default inbound one
      if(!isset($details->field_voipcall_text_script[0]['value'])) {
        $script_name = voipcall_get_default_inbound_text_script_name();
        $default_script = VoipScript::loadScript($script_name);
        $node->field_voipcall_text_script[0]['value'] = serialize($default_script);
      }
************/
  
      // initialize 'Hangup Reason' field
      if(!isset($details->field_voipcall_hangup_reason[0]['value'])) {
        $node->field_voipcall_hangup_reason[0]['value'] = NULL;
      }
  
      // initialize 'Check Machine' field
      if(!isset($details->field_voipcall_check_machine[0]['value'])) {
        $node->field_voipcall_check_machine[0]['value'] = FALSE;
      }
  
    }
    else {
      // load the node
  //TODO: Why does the following node_load requires resetting the node cache?
      $node = node_load($details->nid, NULL, TRUE);
    }
  
    // update node fields
    $new_dest_number = $details->field_voipcall_dest_number[0]['value'];
    if(isset($new_dest_number)) $node->title = check_plain($new_dest_number);
    if(isset($details->uid)) $node->uid = $details->uid;
  
    // update voipcall-specific fields
  
    $new_call_id = $details->field_voipcall_call_id[0]['value'];
    if(isset($new_call_id))
      $node->field_voipcall_call_id[0]['value'] = $new_call_id;

    $new_dest_number = $details->field_voipcall_dest_number[0]['value'];
    if(isset($new_dest_number))
      $node->field_voipcall_dest_number[0]['value'] = $new_dest_number;

    $new_dest_name = $details->field_voipcall_dest_name[0]['value'];
    if(isset($new_dest_name))
      $node->field_voipcall_dest_name[0]['value'] = $new_dest_name;

    $new_caller_number = $details->field_voipcall_caller_number[0]['value'];
    if(isset($new_caller_number))
      $node->field_voipcall_caller_number[0]['value'] = $new_caller_number;

    $new_caller_name = $details->field_voipcall_caller_name[0]['value'];
    if(isset($new_caller_name))
      $node->field_voipcall_caller_name[0]['value'] = $new_caller_name;
  
    $new_call_status = $details->field_voipcall_status[0]['value'];
    if(isset($new_call_status))
      $node->field_voipcall_status[0]['value'] = $new_call_status;
  
    $new_answered_by = $details->field_voipcall_answered_by[0]['value'];
    if(isset($new_answered_by))
      $node->field_voipcall_answered_by[0]['value'] = $new_answered_by;
  
    $new_text_status = $details->field_voipcall_text_status[0]['value'];
    if(isset($new_text_status))
      $node->field_voipcall_text_status[0]['value'] = $new_text_status;

    $new_source = $details->field_voipcall_source[0]['value'];
    if(isset($new_source))
      $node->field_voipcall_source[0]['value'] =  $new_source;

    $new_direction = $details->field_voipcall_direction[0]['value'];
    if(isset($new_direction))
      $node->field_voipcall_direction[0]['value'] =  $new_direction;
  
    $new_start_time = $details->field_voipcall_start_time[0]['value'];
    if(isset($new_start_time))
      $node->field_voipcall_start_time[0]['value'] = $new_start_time;

    $new_end_time = $details->field_voipcall_end_time[0]['value'];
    if(isset($new_end_time))
      $node->field_voipcall_end_time[0]['value'] = $new_end_time;

    $new_duration = $details->field_voipcall_duration[0]['value'];
    if(isset($new_duration))
      $node->field_voipcall_duration[0]['value'] = $new_duration;
  
    $new_error_message = $details->field_voipcall_error_message[0]['value'];
    if(isset($new_error_message))
      $node->field_voipcall_error_message[0]['value'] = $new_error_message;
  
    $new_script = $details->field_voipcall_script[0]['value'];
    if(isset($new_script))
      $node->field_voipcall_script[0]['value'] =  serialize($new_script);
  
    $text_script = $details->field_voipcall_text_script[0]['value'];
    if(isset($new_script))
      $node->field_voipcall_text_script[0]['value'] =  serialize($text_script);
  
    $new_hangup_script_name = $details->field_voipcall_hangup_script[0]['value'];
    if(isset($new_hangup_script_name))
      $node->field_voipcall_hangup_script[0]['value'] =  $new_hangup_script_name;
  
    $new_hangup_reason = $details->field_voipcall_hangup_reason[0]['value'];
    if(isset($new_hangup_reason))
      $node->field_voipcall_hangup_reason[0]['value'] =  $new_hangup_reason;
  
    $new_check = $details->field_voipcall_check_machine[0]['value'];
    if(isset($new_check))
      $node->field_voipcall_check_machine[0]['value'] =  $new_check;
  
    // set other optional node fields
    if(isset($details->other)){
      foreach($details->other as $field => $value){
        $node->$field = $value;
      }
    }
  
    // save the node
    node_save($node);
  
    return $node;
  }
  
  /**
   * Update script variables with the current status of the given call
   *
   * NOTE: The call variables are all read-only. Any changes to their values
   *       executed within the script will not be applied to the actual
   *       call node. 
   *
   * @param VoipScript $script to be updated
   *
   * @return updated script.
   */
  private function _voipcall_update_script($script) {
  
    $call_node = $this->call_node;

    // add call variables to script variables
  
    $script->setVar('uid', $call_node->uid);
  
    $script->setVar('call_id',
      $call_node->field_voipcall_call_id[0]['value']);
  
    $script->setVar('dest_number',
      $call_node->field_voipcall_dest_number[0]['value']);
    $script->setVar('dest_name',
      $call_node->field_voipcall_dest_name[0]['value']);
    $script->setVar('caller_number',
      $call_node->field_voipcall_caller_number[0]['value']);
    $script->setVar('caller_name',
      $call_node->field_voipcall_caller_name[0]['value']);
  
    $script->setVar('call_status',
      $call_node->field_voipcall_status[0]['value']);
  
    $script->setVar('answered_by',
      $call_node->field_voipcall_answered_by[0]['value']);
  
    $script->setVar('text_status',
      $call_node->field_voipcall_text_status[0]['value']);
  
    $script->setVar('source',
      $call_node->field_voipcall_source[0]['value']);
    $script->setVar('direction',
      $call_node->field_voipcall_direction[0]['value']);
  
    $script->setVar('start_time',
      $call_node->field_voipcall_start_time[0]['value']);
    $script->setVar('end_time',
      $call_node->field_voipcall_end_time[0]['value']);
    $script->setVar('duration',
      $call_node->field_voipcall_duration[0]['value']);
  
    $script->setVar('error_message',
      $call_node->field_voipcall_error_message[0]['value']);

    $script->setVar('hangup_reason',
        $call_node->field_voipcall_hangup_reason[0]['value']);

    $script->setVar('check_machine',
        $call_node->field_voipcall_check_machine[0]['value']);

    return $script;
  }

}

