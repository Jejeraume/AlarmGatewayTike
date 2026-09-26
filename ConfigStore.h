#pragma once
#include <Arduino.h>
#include "AlarmTypes.h"

#define MAX_EQUIPMENT_CODES 16

struct EquipmentCode {
  char code[8];
  char name[32];
};

struct StoredConfig {
  uint32_t magic;
  uint16_t version;

  char wifiSsid[33];
  char wifiPassword[65];

  char alarmIp[16];
  char alarmUser[33];
  char alarmPassword[49];

  char ethernetLocalIp[16];
  char ethernetNetmask[16];
  char searchPrefix[16]; // ex. "192.168.0"

  uint16_t pollSeconds;
  char smsPassword[9];   // les applis observées utilisent 6 chiffres, marge incluse
  
  char homeAssistantIp[16];
  uint16_t homeAssistantPort;
  char homeAssistantToken[256];
  
  EquipmentCode equipmentCodes[MAX_EQUIPMENT_CODES];
  
  bool setupCompleted;
};

class ConfigStore {
public:
  bool begin();
  bool save();
  bool saveSnapshot();
  bool saveCommandRules();
  bool exportCfg(String &out) const;
  bool importCfg(const String &text, String &error);

  StoredConfig& data() { return cfg_; }
  const StoredConfig& data() const { return cfg_; }
  CentralSnapshot& snapshot() { return snapshot_; }
  const CentralSnapshot& snapshot() const { return snapshot_; }
  SmsCommandRule* commandRules() { return rules_; }
  const SmsCommandRule* commandRules() const { return rules_; }
  static constexpr size_t commandRuleCount() { return 8; }

private:
  StoredConfig cfg_{};
  CentralSnapshot snapshot_{};
  SmsCommandRule rules_[8]{};

  void defaults();
  void defaultRules();
  void defaultEvents();
  bool loadBinary(const char *path, void *dst, size_t len);
  bool saveBinary(const char *path, const void *src, size_t len);
};
