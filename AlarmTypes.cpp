#include "AlarmTypes.h"

bool AlarmEntry::isIntrusion() const {
  String s = state;
  s.toLowerCase();
  return s.indexOf("intrusion") >= 0;
}

uint8_t PhoneList::countValid() const {
  uint8_t n = 0;
  for (uint8_t i = 0; i < 4; ++i) if (phone[i].length()) ++n;
  return n;
}

String normalizeFrenchPhone(const String &raw) {
  String s;
  for (size_t i = 0; i < raw.length(); ++i) {
    char c = raw[i];
    if ((c >= '0' && c <= '9') || (c == '+' && s.length() == 0)) s += c;
  }
  if (s.startsWith("00")) s = "+" + s.substring(2);
  else if (s.length() == 10 && s[0] == '0') s = "+33" + s.substring(1);
  return s;
}

const char* smsActionName(SmsAction a) {
  switch (a) {
    case SmsAction::ARM_TOTAL: return "Armer";
    case SmsAction::DISARM: return "Desarmer";
    case SmsAction::ARM_HOME: return "Maison / partiel";
    case SmsAction::STATUS: return "Verifier / statut";
    case SmsAction::CANCEL_ALARM: return "Annuler alarme";
    case SmsAction::PGM_ON: return "Ouvrir PGM";
    case SmsAction::PGM_OFF: return "Fermer PGM";
    case SmsAction::SWITCH_ON: return "Ouvrir commutateur";
    case SmsAction::SWITCH_OFF: return "Fermer commutateur";
    default: return "Aucune";
  }
}
