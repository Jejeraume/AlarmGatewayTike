#include "OtaManager.h"
#include "BuildConfig.h"
#if defined(ESP32)
  #include <WiFi.h>
  #include <ArduinoOTA.h>
#else
  #include <ESP8266WiFi.h>
  #include <ArduinoOTA.h>
#endif

void OtaManager::begin(){
  if(WiFi.status()!=WL_CONNECTED)return;
  ArduinoOTA.setHostname(ALARM_GATEWAY_HOSTNAME);
  ArduinoOTA.onStart([](){Serial.println(F("[OTA] Debut"));});
  ArduinoOTA.onEnd([](){Serial.println(F("[OTA] Termine"));});
  ArduinoOTA.onError([](ota_error_t e){Serial.printf("[OTA] Erreur %u\n",(unsigned)e);});
  ArduinoOTA.begin();started_=true;Serial.println(F("[OTA] Arduino OTA actif"));
}
void OtaManager::loop(){if(!started_&&WiFi.status()==WL_CONNECTED)begin();if(started_)ArduinoOTA.handle();}
