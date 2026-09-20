#pragma once
#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <ENC28J60lwIP.h>
#include "BuildConfig.h"
#include "ConfigStore.h"
#include "AlarmClient.h"
#include "AlarmTypes.h"

class WebInterface {
public:
  WebInterface(ConfigStore &config, AlarmClient &alarm, ENC28J60lwIP &eth);
  void begin();
  void loop();
  void setLastEvent(const AlarmEntry &entry, bool valid);
  void setAlarmReachable(bool v) { alarmReachable_ = v; }
  void setLastMessage(const String &s) { lastMessage_ = s; }
  void setLedState(bool v) { ledState_ = v; }

  bool consumeTestRequest();
  bool consumeSearchRequest();
  bool consumeLedToggleRequest();

private:
  ConfigStore &config_;
  AlarmClient &alarm_;
  ENC28J60lwIP &eth_;
  ESP8266WebServer server_{80};
  AlarmEntry lastEntry_;
  bool haveEntry_ = false;
  bool alarmReachable_ = false;
  bool ledState_ = false;
  String lastMessage_;
  bool testReq_ = false;
  bool searchReq_ = false;
  bool ledReq_ = false;
  String cfgUpload_;

  String esc(const String &s) const;
  String pageHeader(const String &title) const;
  String pageFooter() const;
  String mainPage() const;
  String eventsPage() const;
  String commandsPage() const;
  void redirect(const char *path = "/");
  void handleSave();
  void handleSaveCommands();
  void handleSaveEvents();
  void handleExportCfg();
  void handleImportCfg();
  void handleCfgUpload();
  void handleUpdateUpload();
};
