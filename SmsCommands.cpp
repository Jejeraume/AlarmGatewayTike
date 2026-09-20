#include "SmsCommands.h"

String SmsCommands::normalizeCommand(String s) const {s.trim();s.toUpperCase();while(s.indexOf("  ")>=0)s.replace("  "," ");return s;}

bool SmsCommands::parse(const String &sms,SmsAction &action,String &cmd,String &error) const {
  action=SmsAction::NONE;cmd="";error="";String s=sms;s.trim();
  if(!s.startsWith("#PWD")){error=F("Prefixe #PWD absent");return false;}
  int sep=s.indexOf('#',4);if(sep<0){error=F("Separateur commande absent");return false;}
  String pin=s.substring(4,sep);if(pin!=String(config_.data().smsPassword)){error=F("Code PWD incorrect");return false;}
  cmd=normalizeCommand(s.substring(sep+1));
  const SmsCommandRule *rules=config_.commandRules();
  for(size_t i=0;i<ConfigStore::commandRuleCount();++i){if(!rules[i].enabled)continue;if(cmd==normalizeCommand(String(rules[i].command))){action=rules[i].action;return true;}}
  error=F("Commande inconnue ou desactivee");return false;
}

bool SmsCommands::execute(SmsAction a,String &reply){
  bool ok=false;reply="";
  switch(a){
    case SmsAction::ARM_TOTAL: ok=alarm_.remoteControl(1); break;
    case SmsAction::ARM_HOME: ok=alarm_.remoteControl(2); break;
    case SmsAction::DISARM: ok=alarm_.remoteControl(3); break;
    case SmsAction::CANCEL_ALARM: ok=alarm_.remoteControl(4); break;
    case SmsAction::STATUS:{AlarmEntry e;ok=alarm_.getLastEvent(e);if(ok)reply=e.date+" - "+e.state+" (code "+e.code+")";break;}
    case SmsAction::PGM_ON: case SmsAction::PGM_OFF: case SmsAction::SWITCH_ON: case SmsAction::SWITCH_OFF:
      reply=F("Commande domotique reservee : page domotique non mappee");return false;
    default: reply=F("Aucune action");return false;
  }
  if(!ok)reply=alarm_.lastError();else if(!reply.length())reply=F("Commande executee");return ok;
}
