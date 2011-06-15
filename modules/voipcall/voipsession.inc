<?php

class VoipSession {
  
    private $session;

   /**
   * Constants
   */
   const DEFAULT_EXPIRATION = 300;
   
   function __construct($session=NULL) {
     $this->session = $session;
   }   
   /**
   * Returns Session object associated with the matching pair of numbers
   *
   * @param $origin_number
   *   String representing origin number.
   * @param $destination_number
   *   String representing destination number.
   *
   * @return
   *   Session object.
   */
    public static function getSessionByNumber($origin_number, $destination_number){
      //TODO: Check for expiration timer?
      $session = db_fetch_object(db_query("SELECT * FROM {voipsession} WHERE origin_number='%s' AND destination_number='%s'", $origin_number, $destination_number));
      if($session->expiring_at<time()){
        //Delete this session, its expired
        db_query("DELETE FROM {voipsession} WHERE sid=%d",$session->sid);
        return false;
      }
      return $session;
    }
    
   /**
   * Returns Session object associated with the matching VoIP Call
   *
   * @param $call_id
   *   Integer representing VoIP Call ID.
   *
   * @return
   *    Session object.
   */
    public static function getSessionByCallId($call_id){
      //TODO: Check for expiration timer?
      $session = db_fetch_object(db_query("SELECT * FROM {voipsession} WHERE call_id=%d", $call_id));
      return $session;
    }
    
   /**
   * Delete all expired sessions
   *
   * @return
   *   Array of call ids of deleted sessions.
   */
    public static function deleteExpiredSessions(){
      $deleted_call_ids = array();
      $time = time();
      
      //Get call ids of deleted sessions
      $res = db_query("SELECT * FROM {voipsession} WHERE expiring_at<'%s'", $time);
      while($session = db_fetch_object($res)) {
        $deleted_call_ids[] = $session->call_id;
      }
      //Delete the sessions
      db_query("DELETE FROM {voipsession} WHERE expiring_at<'%s'",$time);
      return $deleted_call_ids;
    }

    public function getCallId(){
      return $this->session->call_id;
    }
    
    public function setCallId($call_id){
      $this->session->call_id = $call_id;
    }
    
    public function getOriginNumber(){
      return $this->session->origin_number;
    }
    
    public function setOriginNumber($origin_number){
      $this->session->origin_number = $origin_number;
    }
    
    public function getDestinationNumber(){
      return $this->session->destination_number;
    }
    
    public function setDestinationNumber($destination_number){
      $this->session->destination_number = $destination_number;
    }
    
     public function setExpirationTime($expiration_time=VoipSession::DEFAULT_EXPIRATION){
      $this->session->expiration_time = $expiration_time;
    }
    
    public function resetTimer($expiration_time=VoipSession::DEFAULT_EXPIRATION){
      $this->session->expiration_time = $expiration_time;
      $this->session->expiring_at = time()+$expiration_time;
    }
    
  /**
   * Save the session data to the database
   *
   * @return
   *   The updated VoipSession instance, or NULL
   */
    public function save(){
      
      if(is_null($this->session->sid)){
        $data = new stdClass();
        $update = NULL;
        // initialize the 'Call Id' field
        if(!isset($this->session->call_id)) {
          $data->call_id = NULL;
        }
        // initialize the 'Origin number' field
        if(!isset($this->session->origin_number)) {
          $data->origin_number = NULL;
        }
        // initialize the 'Destination number' field
        if(!isset($this->session->destination_number)) {
          $data->destination_number = NULL;
        }
        // initialize the 'Expiration time' field
        if(!isset($this->session->expiration_time)) {
          $data->expiration_time = VoipSession::DEFAULT_EXPIRATION;
          $expiration_time = VoipSession::DEFAULT_EXPIRATION;
        }
        else {
          $expiration_time = $this->session->expiration_time;
        }
        // initialize the 'Expiring at' field
        $this->session->expiring_at = time()+$expiration_time;
        
      }
      else {
        // load the data
        $data = db_fetch_object(db_query("SELECT * FROM {voipsession} WHERE sid=%d", $this->session->sid));
        $update = 'sid';
      }
     
      $call_id = $this->session->call_id;
      if(isset($call_id))
        $data->call_id = $call_id;
      
      $origin_number = $this->session->origin_number;
      if(isset($origin_number))
        $data->origin_number = $origin_number; 
      
      $destination_number = $this->session->destination_number;
      if(isset($destination_number))
        $data->destination_number = $destination_number;
     
     $expiration_time = $this->session->expiration_time;
      if(isset($expiration_time))
        $data->expiration_time = $expiration_time;
      
      $data->expiring_at =  $this->session->expiring_at;
      // save the data
      drupal_write_record('voipsession', $data, $update);
      
      $this->session = $data;
    }
 }