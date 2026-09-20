#pragma once
#include <Arduino.h>
#include "AlarmTypes.h"

class ModemAT {
public:
  explicit ModemAT(Stream &serial) : serial_(serial) {}
  bool begin();
  void loop();
  bool sendSMS(const String &number, const String &message);
  bool sendSMS(const PhoneList &phones, const String &message);
  bool call(const String &number, uint32_t ringTimeMs = 20000);
  bool callWithRetries(const PhoneList &phones, uint8_t rounds, uint32_t ringTimeMs = 20000);
  String lastResponse() const { return lastResponse_; }

private:
  Stream &serial_;
  String lastResponse_;
  void flushInput();
  bool waitFor(const String &token, uint32_t timeoutMs);
  bool command(const String &cmd, const String &expected="OK", uint32_t timeoutMs=3000);
};
