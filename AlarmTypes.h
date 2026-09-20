#pragma once
#include <Arduino.h>

struct AlarmEntry {
  String date;
  String code;
  String state;
  String signature;

  bool isIntrusion() const;
};

struct PhoneList {
  String phone[4];
  uint8_t dialCount = 1; // Nombre de tours d'appel configure localement (1..15)

  uint8_t countValid() const;
};

enum class SmsAction : uint8_t {
  NONE = 0,
  ARM_TOTAL,
  DISARM,
  ARM_HOME,
  STATUS,
  CANCEL_ALARM,
  PGM_ON,
  PGM_OFF,
  SWITCH_ON,
  SWITCH_OFF
};

struct SmsCommandRule {
  char command[25];
  SmsAction action;
  bool enabled;
};

struct CentralEventConfig {
  uint8_t id = 0;
  char name[48] = {0};
  bool valid = true;        // Configuration locale, modifiable depuis la page Evenements
  bool cms = false;
  bool voice = false;
  bool sms = false;
  bool email = false;
};

struct CentralSnapshot {
  uint32_t magic = 0;
  uint32_t revision = 0;
  char phones[4][20] = {{0}};
  uint8_t dialCount = 1;
  CentralEventConfig events[40];
  bool phoneDataValid = false;
  bool eventDataValid = false;
};

String normalizeFrenchPhone(const String &raw);
const char* smsActionName(SmsAction action);
