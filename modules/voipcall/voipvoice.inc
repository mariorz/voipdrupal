<?php 
/*
TODO: 
asDate($number) -> convert to date
asDigits($number) -> convert as digits (1 2 3 ...)
asNumber($number) -> return $number
*/
/**
 * @file
 * Implementation of VoIP Voice class
 */

  class VoipVoice {
  
    private $voice_code;
    private $gender;
    private $language;
    private $is_local;
   
   function __construct($voice_code, $gender, $language, $is_local) {
     $this->voice_code = $voice_code;
     $this->gender = $gender;
     $this->language = $language;
     $this->is_local = $is_local;
   }
   
   /**
   * Sets default VoIP Voice
   *
   * @param $voice_code
   *   Integer representing voice code
   *
   * @return
   *   
   */
    public static function setDefaultVoice($voice_code){
      //TODO: Check if voice_code is valid
      variable_set('voipcall_voice_id', $voice_code);
    }
    
   /**
   * Get the default VoIP Voice
   *
   *
   * @return
   *   interger representing voice code of default VoIP Voice.
   */
    public static function getDefaultVoice(){
       $voice_code = variable_get('voipcall_voice_id', '');
       return $voice_code;
    }
    
   /**
   * Get the VoIP Voice
   *
   * @param $voice_code
   *   Integer representing voice code
   *
   * @return
   *   interger representing voice code of default VoIP Voice.
   */
    public static function getVoice($voice_code = NULL){
      if ($voice_code == 'man') {
        return variable_get('voipcall_man_voice_id', '');
      }
      else if ($voice_code == 'woman') {
        return variable_get('voipcall_woman_voice_id', '');
      }
      else if (empty($voice_code)) {
        return self::getDefaultVoice();
      }
      else {
        return $voice_code;
      }
    }
    
   /**
   * Get the VoIP Voices matching some parameters
   *
   * @param $language
   *   String representing language code ("en", "hr", etc...)
   *
   * @param $gender
   *   String representing gender of the voice ("man", "woman", "neutral")
   *
   * @return
   *   interger representing voice code of default VoIP Voice.
   */
    public static function getVoices($language = NULL, $gender=NULL, $is_local=NULL) {
      //call a hook_getvoices()
      $voices = module_invoke_all('getvoices', $language , $gender, $is_local);
      sort($voices);
      return $voices;
    }
    
   /**
   * This method defines which voice to be used whenever the id is 'man'
   *
   * @param $voice_code
   *   String representing voice code
   *
   * @return
   *   
   */
    public static function setDefaultManVoice($voice_code = NULL){
      variable_set('voipcall_man_voice_id', $voice_code);
    }
    
   /**
   * This method defines which voice to be used whenever the id is 'woman'
   *
   * @param $voice_code
   *   String representing voice code
   *
   * @return
   *   
   */
    public static function setDefaultWomanVoice($voice_code = NULL){
      variable_set('voipcall_woman_voice_id', $voice_code);
    }
    
      /**
   * This method defines which voice to be used whenever the id is 'man'
   *
   * @param $voice_code
   *   String representing voice code
   *
   * @return
   *   
   */
    public static function getDefaultManVoice(){
      return variable_get('voipcall_man_voice_id', self::getDefaultVoice());
    }
    
   /**
   * This method returns default woman voice
   *
   * @param $voice_code
   *   String representing voice code
   *
   * @return
   *   
   */
    public static function getDefaultWomanVoice(){
      return variable_get('voipcall_woman_voice_id', self::getDefaultVoice());
    }
    

    function getVoiceCode(){
      return $this->voice_code;
    }
    
    function getGender(){
      return $this->gender;
    }
    
    function getLanguage(){
      return $this->language;
    }
    
    function isLocal() {
      return $this->is_local;
    }
    
   /**
   * This method returns numbers in number format: 123 will become 1 2 3
   *
   * @param $number
   *   Integer representing number
   *
   * @return
   *   Integer representing number
   */
    public static function getDigits($number) {
      $number_array = str_split($number);
      $number_with_space = implode(" ",$number_array);
      return $number_with_space;
    }
    
    /**
   * This method returns time in date format
   *
   * @param $time
   *   Mixed representing time in timestamp or any string format supported in strtotime(): http://php.net/manual/en/function.strtotime.php
   *
   * @return
   *   String representing date
   */
    public static function getDate($time) {
      $timestamp = strtotime($time);  //Will convert string to timestamp
      if (!$timestamp) {
        //If the $time param was timestamp, then strtotime() will return FALSE.
        $timestamp = $time;
      }
      return date("F j, Y",$timestamp); 
    }
    
    function getPrompt($text, $params = NULL) {
      /*
      *** the implementation of v() will call this method for the current voice
*** for local voices, this method should return the audio file(s) associated with the $text
*** for non-local voices, this method should just return the $text itself (or its translation)
      */
      if ($this->isLocal()) {
        return v($text, $params['args'], $this->getVoiceCode());
      }
      else {
        return new VoipPrompt($text, $this->getVoiceCode()); 
      }
    }
 }