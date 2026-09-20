#pragma once
#include <Arduino.h>
#include "ConfigStore.h"
#include "AlarmClient.h"

class SmsCommands {
public:
  SmsCommands(ConfigStore &config, AlarmClient &alarm) : config_(config), alarm_(alarm) {}

  // Parse le format existant de l'application Android : #PWD000000#DESARMER
  bool parse(const String &sms, SmsAction &action, String &normalizedCommand, String &error) const;
  bool execute(SmsAction action, String &reply);

private:
  ConfigStore &config_;
  AlarmClient &alarm_;
  String normalizeCommand(String s) const;
};
