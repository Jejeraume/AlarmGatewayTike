#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include "ConfigStore.h"
#include "AlarmTypes.h"

using AlarmTcpClient = WiFiClient;

struct AlarmProgrammedEvent {
  uint8_t code;
  String name;
};

class AlarmClient {
public:
  explicit AlarmClient(ConfigStore &config);

  bool getLastEvent(AlarmEntry &entry, int *httpStatus = nullptr);
  bool remoteControl(uint8_t ctrl);
  bool setZoneBypass(uint8_t zone, bool enabled);
  bool readProgrammedEvents(AlarmProgrammedEvent *events, size_t maxEvents, size_t &eventCount);

  // Détection première installation : cherche un serveur HTTP sur prefix.1..254
  // et vérifie qu'il ressemble à la centrale (index/SystemLog ou HTTP 401).
  bool discoverAlarm(const String &prefix, IPAddress &found, uint16_t timeoutPerHostMs = 45);

  String lastError() const { return lastError_; }

private:
  ConfigStore &config_;
  String lastError_;

  String base64Encode(const String &input);
  String stripTags(const String &html);
  bool httpRequest(const IPAddress &ip, const char *method, const char *path,
                   const String &body, String &response, int &status,
                   uint32_t timeoutMs = 2500, bool authenticate = true);
  bool httpGet(const char *path, String &response, int &status, uint32_t timeoutMs = 2500);
  bool httpPost(const char *path, const String &body, String &response, int &status, uint32_t timeoutMs = 2500);
  bool extractFirstRow(const String &html, AlarmEntry &entry);
};
