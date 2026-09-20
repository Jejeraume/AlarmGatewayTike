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
  bool callWithRetries(const PhoneList &phones, uint8_t rounds,
                       uint32_t ringTimeMs = 20000);

  String lastResponse() const { return lastResponse_; }

  // Retourne un SMS reçu si un +CMTI a été traité depuis le dernier appel.
  // sender et message sont renvoyés décodés en texte UTF-8 Arduino String.
  bool takeReceivedSMS(String &sender, String &message);

private:
  Stream &serial_;
  String lastResponse_;
  String urcBuffer_;
  String smsSender_;
  String smsMessage_;
  bool smsPending_ = false;

  void flushInput();
  bool waitFor(const String &token, uint32_t timeoutMs);
  bool command(const String &cmd,
               const String &expected = "OK",
               uint32_t timeoutMs = 3000);

  bool readSMS(uint16_t index);

  static String extractQuotedField(const String &line, uint8_t fieldIndex);

  // Conversion utilisée lorsque AT+CSCS="UCS2" est actif.
  static String utf8ToUcs2Hex(const String &text);
  static String ucs2HexToUtf8(const String &hex);
  static bool looksLikeUcs2Hex(const String &text);
  static int8_t hexValue(char c);
  static void appendUtf8(String &out, uint16_t codepoint);
};
