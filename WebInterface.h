#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <ETH.h>
#include <WebServer.h>

#include "BuildConfig.h"
#include "ConfigStore.h"
#include "AlarmClient.h"
#include "AlarmTypes.h"

class WebInterface {
public:
  WebInterface(ConfigStore &config, AlarmClient &alarm);

  void begin();
  void loop();

  void setLastEvent(const AlarmEntry &entry, bool valid);
  void setAlarmReachable(bool v) { alarmReachable_ = v; }
  void setLastMessage(const String &s) { lastMessage_ = s; }

  bool consumeTestRequest();
  bool consumeSearchRequest();

private:
  ConfigStore &config_;
  AlarmClient &alarm_;

  WebServer server_{80};

  AlarmEntry lastEntry_;
  bool haveEntry_ = false;
  bool alarmReachable_ = false;

  String lastMessage_;

  bool testReq_ = false;
  bool searchReq_ = false;

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
  void handleReadEvents();
  void handleExportCfg();
  void handleImportCfg();
  void handleCfgUpload();
  
  void handleUpdateUpload();
};