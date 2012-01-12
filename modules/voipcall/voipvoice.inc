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
   *   String with the gender to be used for the prompt.  Allowed values:
   *   man, woman, neutral
   */
  private $gender;
    
  /**
   * @variable $language.
   *   String with the language entonation to be used for the prompt. 
   */
  private $language;
    
  /**
   * @variable $is_local
   *   Boolean indicating whether this voice is generated on the local Drupal 
   *   server or by the remote VoIP server.
   */
  private $is_local;
   
  /**
   * Constructor
   */
  function __construct($voice_id, $gender, $language, $is_local) {
    $this->voice_id = $voice_id;
    $this->gender = $gender;
    $this->language = $language;
    $this->is_local = $is_local;
  }
   
  /**
   * Sets the default VoIP Voice
   *
   * @param $voice_id
   *   String representing the voice id
   */
  public static function setDefaultVoice($voice_id){
    //TODO: Check if voice_id is valid
    variable_set('voipcall_voice_id', $voice_id);
  }
    
  /**
   * Retrieves the default VoIP Voice of the system
   *
   * @return
   *   VoipVoice instance of default VoIP Voice.
   */
  public static function getDefaultVoice(){
    $voice_id = variable_get('voipcall_voice_id', '');
    $voices = self::getVoices();
    $voice = $voices[$voice_id];
    $voipvoice_voice = new VoipVoice($voice_id, $voice['voice'],  $voice['language'], $voice['is_local']);
    return $voipvoice_voice;
  }
    
  /**
   * Get the VoIP Voice instance associated with the given id
   *
   * @param $voice_id
   *   String representing the voice id
   *
   * @return
   *   VoipVoice instance associated with $voice_id
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
   * Get the VoIP Voices that match the specified parameters
   *
   * @param $language
   *   String representing a language code ("en", "hr", etc...)
   *
   * @param $gender
   *   String representing the gender of the voice ("man", "woman", "neutral")
   *
   * @return
   *   Array representing all available voices matching the criteria.
   */
  public static function getVoices($language = NULL, $gender=NULL, $is_local=NULL) {
    // call hook_getvoices()
    $voices = module_invoke_all('getvoices', $language , $gender, $is_local);
    ksort($voices);
    return $voices;
  }
    
  /**
   * This method defines which voice is to be used whenever the id is 'man'
   *
   * @param $voice_id
   *   String representing voice id
   *
   * @return
   *   VoipVoice instance associated with the 'man' id
   */
  public static function setDefaultManVoice($voice_id = NULL){
    variable_set('voipcall_man_voice_id', $voice_id);
  }
    
  /**
   * Defines which voice is to be used whenever the id is 'woman'
   *
   * @param $voice_id
   *   String representing voice id
   *
   * @return
   *   VoipVoice instance associated with the 'woman' id
   */
  public static function setDefaultWomanVoice($voice_id = NULL){
    variable_set('voipcall_woman_voice_id', $voice_id);
  }
    
  /**
   * Returns the VoipVoice instance associated with the 'man' id
   */
  public static function getDefaultManVoice(){
    $voice_id = variable_get('voipcall_man_voice_id', NULL);
    $voipvoice_voice = self::getVoice($voice_id);
    return $voipvoice_voice;
  }
    
  /**
   * Returns the VoipVoice instance associated with the 'woman' id
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
   * Returns numbers in digit format: '123' will become '1 2 3'
   *
   * @param $number
   *   Integer representing the number to be converted
   *
   * @return
   *   String representing the number digits
   */
  public static function getDigits($number) {
    $number_array = str_split($number);
    $number_with_space = implode(" ",$number_array);
    return $number_with_space;
  }
    
  /**
   * Returns time in date format
   *
   * @param $time
   *   Mixed representing time as timestamp or as any string format supported
   *   by strtotime(). Check http://php.net/manual/en/function.strtotime.php
   *
   * @return
   *   String representing the date
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
   * Returns a voice prompt ready to be processed by a VoIP server
   *
   * For local voices, this method returns the audio file(s) associated with
   * $text.  For non-local voices, this method returns the string to be read out
   * loud by the VoIP Server.
   *
   * @param $text
   *   String with the prompt
   *
   * @param $args
   *   An associative array of replacements to be made in the prompt afters
   *   translation. Similar to the $args parameter of t()
   *
   * @paramm $as
   *   String that determines how numbers should be pronounced. Supported values
   *   include 'number' (to read '12' as 'twelve') or 'digits' (to read '12' as
   *   'one two'). By default, numbers should be read as 'number'.
   *
   * @return
   *   String combining text and audio urls to be read and processed by VoIP
   *   servers.
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
 
/**
 * Helper functions
 */
 
/**
 * Stores the current voice of the system
 */
function voipvoice_set_current_voice($voice_tmp) {
  global $voipvoice_current_voice;
  $voipvoice_current_voice = $voice_tmp;
}

/**
 * Wrapper around VoiceVoice's getPrompt() method
 */
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
