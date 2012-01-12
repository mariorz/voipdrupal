<?php 
/**
 * @file
 * Implementation of VoIP Voice class
 */

  class VoipVoice {
  
    /**
    * @variable $voice_id
    *   String with the type of voice to be used for the prompt. 
    */
    private $voice_id;
    /**
    * @variable $gender
    *   String with the type of gender to be used for the prompt.  Supports following formats: man, woman, neutral
    */
    private $gender;
    
    
    /**
    * @variable $language.
    *   String with the language entonation to be used for the prompt. 
    */
    private $language;
    
    /**
    * @variable $voice_id
    *  Boolean indicating if this voice is generated on local server or not.
    */
    private $is_local;
   
   function __construct($voice_id, $gender, $language, $is_local) {
     $this->voice_id = $voice_id;
     $this->gender = $gender;
     $this->language = $language;
     $this->is_local = $is_local;
   }
   
   /**
   * Sets default VoIP Voice
   *
   * @param $voice_id
   *   String representing voice id
   *
   * @return
   *   
   */
    public static function setDefaultVoice($voice_id){
      //TODO: Check if voice_id is valid
      variable_set('voipcall_voice_id', $voice_id);
    }
    
   /**
   * Get the default VoIP Voice
   *
   *
   * @return
   *   VoipVoice class of default VoIP Voice.
   */
    public static function getDefaultVoice(){
       $voice_id = variable_get('voipcall_voice_id', '');
       $voices = self::getVoices();
       $voice = $voices[$voice_id];
       $voipvoice_voice = new VoipVoice($voice_id, $voice['voice'],  $voice['language'], $voice['is_local']);
       return $voipvoice_voice;
    }
    
   /**
   * Get the VoIP Voice
   *
   * @param $voice_id
   *   String representing voice id
   *
   * @return
   *   VoipVoice class loaded from voice_id.
   */
    public static function getVoice($voice_id = NULL){
      if ($voice_id == 'man') {
        $voice_id = variable_get('voipcall_man_voice_id', '');
      }
      else if ($voice_id == 'woman') {
        $voice_id = variable_get('voipcall_woman_voice_id', '');
      }
      else if (empty($voice_id)) {
        $voice_id = variable_get('voipcall_voice_id', '');
      }
      
      $voices = self::getVoices();
      $voice = $voices[$voice_id];
      $voipvoice_voice = new VoipVoice($voice_id, $voice['voice'],  $voice['language'], $voice['is_local']);
      return $voipvoice_voice;
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
   *   Array representing all available voices matching the criteria.
   */
    public static function getVoices($language = NULL, $gender=NULL, $is_local=NULL) {
      //call a hook_getvoices()
      $voices = module_invoke_all('getvoices', $language , $gender, $is_local);
      ksort($voices);
      return $voices;
    }
    
   /**
   * This method defines which voice to be used whenever the id is 'man'
   *
   * @param $voice_id
   *   String representing voice id
   *
   * @return
   *   
   */
    public static function setDefaultManVoice($voice_id = NULL){
      variable_set('voipcall_man_voice_id', $voice_id);
    }
    
   /**
   * This method defines which voice to be used whenever the id is 'woman'
   *
   * @param $voice_id
   *   String representing voice id
   *
   * @return
   *   
   */
    public static function setDefaultWomanVoice($voice_id = NULL){
      variable_set('voipcall_woman_voice_id', $voice_id);
    }
    
      /**
   * This method returns VoipVoice class when id is 'man'
   *
   *
   * @return
   *    VoipVoice class loaded from default man voice id.
   */
    public static function getDefaultManVoice(){
      $voice_id = variable_get('voipcall_man_voice_id', NULL);
      $voipvoice_voice = self::getVoice($voice_id);
      return $voipvoice_voice;
    }
    
   /**
   * This method returns VoipVoice class when id is 'woman'
   *
   *
   * @return
   *   VoipVoice class loaded from default woman voice id.
   */
    public static function getDefaultWomanVoice(){
      $voice_id = variable_get('voipcall_woman_voice_id', self::getDefaultVoice()->getVoiceId());
      $voipvoice_voice = self::getVoice($voice_id);
      return $voipvoice_voice;
    }
    

    function getVoiceId(){
      return $this->voice_id;
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
    
   /**
   * This method returns:
   *   - for local voices, this method should return the audio file(s) associated with the $text
   *   - for non-local voices, this method returns the $text in VoipPrompt format.
   *
   * @param $text
   *   String representing the prompt
   *
   * @param $param
   *   String representing:
   *    - for local voices, an associative array of replacements to make after translation. Based on the first character of the key, the value is 
   *    escaped and/or themed. See format_string() for details.
   *    - for non-local voices, string that determines how numbers should be pronounced. Supported values include 'number' (to read '12' as 'twelve') 
   *      or 'digits' (to read '12' as 'one two'). By default, numbers should be read as 'number'
   *
   * @return
   *   - for local voices, this method should return the audio file(s) url associated with the $text
   *   - for non-local voices, this method returns the $text in VoipPrompt format.
   */
    function getPrompt($text, $params = NULL) { 
      if ($this->isLocal()) {
        if(module_exists('voipvoice')) {
          //Use voipvoice function to get locally recorder voice
          return voipvoice_get_prompt($text, $params, $this->getVoiceId());
        }
        else {
          //Log error and return plain text
          watchdog('voipcall', 'getPrompt() called with local voice, but voipvoice.module is not installed.', NULL, WATCHDOG_ERROR);
          return $text;
        }
        
      }
      else {
        return new VoipPrompt($text, $this->getGender(), $this->getLanguage(), $params); 
      }
    }
 }
 
 /* Helper functions */
 
function voipvoice_set_current_voice($voice_tmp) {
  global $voipvoice_current_voice;
  $voipvoice_current_voice = $voice_tmp;
}

function v($text, $params = NULL, $voice_id=NULL) {
 if ($voice_id) {
   $voipvoice_current_voice = VoipVoice::getVoice($voice_id);
 }
 else {
   //Use voice set in voipvoice_set_current_voice()
   global $voipvoice_current_voice;
   if(!$voipvoice_current_voice) {
    //initialize with default
    $voipvoice_current_voice = VoipVoice::getDefaultVoice();
   }
 }

 $prompt = $voipvoice_current_voice->getPrompt($text, $params);
 return $prompt;
}