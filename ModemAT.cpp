#include "ModemAT.h"

void ModemAT::flushInput(){while(serial_.available())serial_.read();}
bool ModemAT::waitFor(const String &token,uint32_t timeoutMs){uint32_t t=millis();lastResponse_="";while(millis()-t<timeoutMs){while(serial_.available()){char c=(char)serial_.read();lastResponse_+=c;if(lastResponse_.indexOf(token)>=0)return true;if(lastResponse_.indexOf("ERROR")>=0)return false;}delay(1);yield();}return false;}
bool ModemAT::command(const String &cmd,const String &expected,uint32_t timeoutMs){flushInput();serial_.print(cmd);serial_.print("\r");return waitFor(expected,timeoutMs);}

bool ModemAT::begin(){
  if(!command("AT"))return false;
  command("ATE0");
  command("AT+CMEE=2");
  // Mode texte + notification d'un nouveau SMS par +CMTI.
  command("AT+CMGF=1");
  command("AT+CNMI=2,1,0,0,0");
  return true;
}

String ModemAT::extractQuotedField(const String &line,uint8_t fieldIndex){
  int pos=0;
  for(uint8_t i=0;i<=fieldIndex;++i){
    int q1=line.indexOf('"',pos); if(q1<0)return "";
    int q2=line.indexOf('"',q1+1); if(q2<0)return "";
    if(i==fieldIndex)return line.substring(q1+1,q2);
    pos=q2+1;
  }
  return "";
}

bool ModemAT::readSMS(uint16_t index){
  flushInput();
  serial_.print("AT+CMGR=");
  serial_.print(index);
  serial_.print("\r");

  uint32_t t=millis();
  String r;
  bool gotHeader=false;
  while(millis()-t<5000){
    while(serial_.available()){
      char c=(char)serial_.read();
      r+=c;
      if(r.indexOf("ERROR")>=0){lastResponse_=r;return false;}
      if(r.indexOf("\r\nOK\r\n")>=0 || r.endsWith("\nOK\r\n")){
        lastResponse_=r;
        int h=r.indexOf("+CMGR:");
        if(h<0)return false;
        int he=r.indexOf('\n',h);
        if(he<0)return false;
        String header=r.substring(h,he); header.trim();
        smsSender_=extractQuotedField(header,1); // +CMGR: "REC UNREAD","numero",...
        int bodyStart=he+1;
        while(bodyStart<(int)r.length() && (r[bodyStart]=='\r' || r[bodyStart]=='\n'))bodyStart++;
        int bodyEnd=r.indexOf("\r\nOK",bodyStart);
        if(bodyEnd<0)bodyEnd=r.indexOf("\nOK",bodyStart);
        if(bodyEnd<0)return false;
        smsMessage_=r.substring(bodyStart,bodyEnd);
        smsMessage_.trim();
        smsPending_=smsSender_.length() && smsMessage_.length();
        return smsPending_;
      }
      if(r.indexOf("+CMGR:")>=0)gotHeader=true;
    }
    delay(1);yield();
  }
  lastResponse_=r;
  (void)gotHeader;
  return false;
}

void ModemAT::loop(){
  // Ne consomme les URC que lorsque le modem est au repos.
  while(serial_.available()){
    char c=(char)serial_.read();
    urcBuffer_+=c;
    if(urcBuffer_.length()>256)urcBuffer_.remove(0,urcBuffer_.length()-128);

    int p=urcBuffer_.indexOf("+CMTI:");
    if(p>=0){
      int e=urcBuffer_.indexOf('\n',p);
      if(e>=0){
        String line=urcBuffer_.substring(p,e); line.trim();
        urcBuffer_.remove(0,e+1);
        int comma=line.lastIndexOf(',');
        if(comma>=0){
          int idx=line.substring(comma+1).toInt();
          if(idx>0){
            if(readSMS((uint16_t)idx)){
              // On efface uniquement après lecture réussie.
              String cmd=String("AT+CMGD=")+idx;
              command(cmd);
            }
          }
        }
      }
    }
  }
}

bool ModemAT::takeReceivedSMS(String &sender,String &message){
  if(!smsPending_)return false;
  sender=smsSender_;
  message=smsMessage_;
  smsPending_=false;
  smsSender_="";
  smsMessage_="";
  return true;
}

bool ModemAT::sendSMS(const String &number,const String &message){if(!number.length())return false;if(!command("AT+CMGF=1"))return false;flushInput();serial_.print("AT+CMGS=\"");serial_.print(number);serial_.print("\"\r");if(!waitFor(">",5000))return false;serial_.print(message);serial_.write(0x1A);return waitFor("+CMGS:",15000)&&waitFor("OK",5000);}
bool ModemAT::sendSMS(const PhoneList &phones,const String &message){bool any=false,all=true;for(uint8_t i=0;i<4;++i){if(!phones.phone[i].length())continue;any=true;if(!sendSMS(phones.phone[i],message))all=false;delay(400);}return any&&all;}
bool ModemAT::call(const String &number,uint32_t ringTimeMs){if(!number.length())return false;flushInput();serial_.print("ATD");serial_.print(number);serial_.print(";\r");if(!waitFor("OK",5000))return false;uint32_t t=millis();while(millis()-t<ringTimeMs){while(serial_.available())serial_.read();delay(10);yield();}command("AT+CHUP");return true;}
bool ModemAT::callWithRetries(const PhoneList &phones,uint8_t rounds,uint32_t ringTimeMs){if(rounds<1)rounds=1;if(rounds>15)rounds=15;for(uint8_t r=0;r<rounds;++r){for(uint8_t i=0;i<4;++i){if(!phones.phone[i].length())continue;if(call(phones.phone[i],ringTimeMs))return true;delay(500);}}return false;}
