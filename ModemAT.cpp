#include "ModemAT.h"

void ModemAT::flushInput(){while(serial_.available())serial_.read();}
bool ModemAT::waitFor(const String &token,uint32_t timeoutMs){uint32_t t=millis();lastResponse_="";while(millis()-t<timeoutMs){while(serial_.available()){char c=(char)serial_.read();lastResponse_+=c;if(lastResponse_.indexOf(token)>=0)return true;if(lastResponse_.indexOf("ERROR")>=0)return false;}delay(1);yield();}return false;}
bool ModemAT::command(const String &cmd,const String &expected,uint32_t timeoutMs){flushInput();serial_.print(cmd);serial_.print("\r");return waitFor(expected,timeoutMs);}
bool ModemAT::begin(){if(!command("AT"))return false;command("ATE0");command("AT+CMEE=2");return true;}
void ModemAT::loop(){/* Réception SMS ajoutée après validation du modem final et de ses URC. */}
bool ModemAT::sendSMS(const String &number,const String &message){if(!number.length())return false;if(!command("AT+CMGF=1"))return false;flushInput();serial_.print("AT+CMGS=\"");serial_.print(number);serial_.print("\"\r");if(!waitFor(">",5000))return false;serial_.print(message);serial_.write(0x1A);return waitFor("+CMGS:",15000)&&waitFor("OK",5000);}
bool ModemAT::sendSMS(const PhoneList &phones,const String &message){bool any=false,all=true;for(uint8_t i=0;i<4;++i){if(!phones.phone[i].length())continue;any=true;if(!sendSMS(phones.phone[i],message))all=false;delay(400);}return any&&all;}
bool ModemAT::call(const String &number,uint32_t ringTimeMs){if(!number.length())return false;flushInput();serial_.print("ATD");serial_.print(number);serial_.print(";\r");if(!waitFor("OK",5000))return false;uint32_t t=millis();while(millis()-t<ringTimeMs){while(serial_.available())Serial.write(serial_.read());delay(10);yield();}command("AT+CHUP");return true;}
bool ModemAT::callWithRetries(const PhoneList &phones,uint8_t rounds,uint32_t ringTimeMs){if(rounds<1)rounds=1;if(rounds>15)rounds=15;for(uint8_t r=0;r<rounds;++r){for(uint8_t i=0;i<4;++i){if(!phones.phone[i].length())continue;if(call(phones.phone[i],ringTimeMs))return true;delay(500);}}return false;}
