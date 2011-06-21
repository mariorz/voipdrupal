<?php

/*
 * Returns a voip script with sms group
 *
 * @return FALSE in case of failure, or the script array in case of success
 */
function _voipscriptsamples_sms_group(){
  $script = new VoipScript('voipscript_sms_group');
  $script->addSet('request_result', '^voipscriptsamples_sms_group_parse_request(%inbound_text_contents, %caller_number)');
  $script->addSendText("%request_result");
  $script->addHangup();
  return $script;
}
function voipscriptsamples_sms_group_parse_request($msg, $from) {
  $request = explode(" ", $msg);
  if (count($request)>1) {
    $group_id = $request[1];
    switch (strtoupper($request[0])) {
      case "S":
        $group = variable_get("voip_sms_group_".$group_id, array());
        $group[(string)$from] = $from;
        variable_set("voip_sms_group_".$group_id, $group);
        $ret = t("You are now subscribed to group ").$group_id;
      break;
      case "B":
        //Broadcast the message
        unset($request[0]);
        unset($request[1]);
        $text = implode(" ", $request);
        if(empty($text)) {
          return t("Message can't be empty");
        }
        $group = variable_get("voip_sms_group_".$group_id, array());
        foreach ($group as $number) {
          if ($number != $from) {
            $call = new VoipCall();
            $call->setCurrentChannel('text');
            $call->setCurrentNetwork('sms');
            $call->setDestNumber($number);
            $call->setCallerNumber(variable_get('voipcall_cid_number', NULL));
            voip_text($text, $call);
          }
        }
         $ret = t("Your message has been broadcasted to group ").$group_id;
      break;
      case "U":
        $group = variable_get("voip_sms_group_".$group_id, array());
        unset($group[(string)$from]);
        variable_set("voip_sms_group_".$group_id, $group);
        $ret = t("You have been unsubscribed from group ").$group_id;
      break;
    }
  }
  else {
    $ret = t("Invalid request");
  }
  
  return $ret;
}