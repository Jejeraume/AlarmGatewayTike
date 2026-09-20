#include "AlarmClient.h"

static const size_t HTTP_CAPTURE_MAX = 20000;

AlarmClient::AlarmClient(ConfigStore &config) : config_(config) {}

String AlarmClient::base64Encode(const String &input) {
  static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  String out; out.reserve(((input.length()+2)/3)*4);
  size_t i=0;
  while (i<input.length()) {
    uint32_t a=i<input.length()?(uint8_t)input[i++]:0;
    uint32_t b=i<input.length()?(uint8_t)input[i++]:0;
    uint32_t c=i<input.length()?(uint8_t)input[i++]:0;
    uint32_t t=(a<<16)|(b<<8)|c;
    out += table[(t>>18)&0x3F]; out += table[(t>>12)&0x3F];
    out += table[(t>>6)&0x3F]; out += table[t&0x3F];
  }
  size_t mod=input.length()%3;
  if (mod==1) { out.setCharAt(out.length()-1,'='); out.setCharAt(out.length()-2,'='); }
  else if (mod==2) out.setCharAt(out.length()-1,'=');
  return out;
}

String AlarmClient::stripTags(const String &html) {
  String out; bool tag=false;
  for (size_t i=0;i<html.length();++i) {
    char c=html[i]; if(c=='<'){tag=true;continue;} if(c=='>'){tag=false;continue;} if(!tag)out+=c;
  }
  out.replace("&nbsp;"," "); out.replace("&amp;","&"); out.replace("&lt;","<");
  out.replace("&gt;",">"); out.replace("&quot;","\""); out.trim();
  while(out.indexOf("  ")>=0) out.replace("  "," ");
  return out;
}

bool AlarmClient::httpRequest(const IPAddress &ip, const char *method, const char *path,
                              const String &body, String &response, int &status,
                              uint32_t timeoutMs, bool authenticate) {
  response=""; status=0; lastError_="";
  AlarmTcpClient client; client.setTimeout(timeoutMs);
  if (!client.connect(ip,80)) { lastError_=F("Connexion TCP impossible"); return false; }

  client.print(method); client.print(' '); client.print(path); client.print(F(" HTTP/1.1\r\nHost: "));
  client.print(ip.toString()); client.print(F("\r\nConnection: close\r\n"));
  if (authenticate) {
    String auth=String(config_.data().alarmUser)+":"+String(config_.data().alarmPassword);
    client.print(F("Authorization: Basic ")); client.print(base64Encode(auth)); client.print(F("\r\n"));
  }
  if (body.length()) {
    client.print(F("Content-Type: application/x-www-form-urlencoded\r\nContent-Length: "));
    client.print(body.length()); client.print(F("\r\n"));
  }
  client.print(F("\r\n")); if(body.length()) client.print(body);

  uint32_t t0=millis(); response.reserve(4096);
  while((client.connected()||client.available()) && millis()-t0<timeoutMs) {
    while(client.available()) { char c=(char)client.read(); if(response.length()<HTTP_CAPTURE_MAX)response+=c; }
    delay(1);
  }
  client.stop();
  if(!response.length()){lastError_=F("Aucune reponse HTTP");return false;}
  int eol=response.indexOf("\r\n");
  if(eol>0){String l=response.substring(0,eol);int p=l.indexOf(' ');if(p>=0)status=l.substring(p+1,p+4).toInt();}
  if(status==401){lastError_=F("HTTP 401 : identifiants refuses");return false;}
  if(status==403){lastError_=F("HTTP 403 : acces interdit");return false;}
  if(status<200||status>=400){lastError_=String(F("Erreur HTTP "))+status;return false;}
  return true;
}

bool AlarmClient::httpGet(const char *path,String &response,int &status,uint32_t timeoutMs){
  IPAddress ip; if(!ip.fromString(config_.data().alarmIp)){lastError_=F("Adresse centrale invalide");return false;}
  return httpRequest(ip,"GET",path,"",response,status,timeoutMs,true);
}

bool AlarmClient::httpPost(const char *path,const String &body,String &response,int &status,uint32_t timeoutMs){
  IPAddress ip; if(!ip.fromString(config_.data().alarmIp)){lastError_=F("Adresse centrale invalide");return false;}
  return httpRequest(ip,"POST",path,body,response,status,timeoutMs,true);
}

bool AlarmClient::extractFirstRow(const String &html, AlarmEntry &entry) {
  int searchPos=0, marker=-1;
  while(true){
    int th=html.indexOf("<th",searchPos); if(th<0)break;
    int gt=html.indexOf('>',th), close=html.indexOf("</th>",gt+1); if(gt<0||close<0)break;
    String tag=html.substring(th,gt+1), val=stripTags(html.substring(gt+1,close)); val.trim();
    bool row=tag.indexOf("scope=\"row\"")>=0||tag.indexOf("scope='row'")>=0||tag.indexOf("scope=row")>=0;
    if(row&&val=="1"){marker=th;break;} searchPos=close+5;
  }
  if(marker<0)return false;
  int trStart=html.lastIndexOf("<tr",marker),trEnd=html.indexOf("</tr>",marker); if(trStart<0||trEnd<0)return false;
  String row=html.substring(trStart,trEnd+5),cells[3]; int found=0,pos=0;
  while(found<3){int td=row.indexOf("<td",pos);if(td<0)break;int gt=row.indexOf('>',td),end=row.indexOf("</td>",gt+1);if(gt<0||end<0)break;cells[found++]=stripTags(row.substring(gt+1,end));pos=end+5;}
  if(found<3)return false;
  entry.date=cells[0];entry.code=cells[1];entry.state=cells[2];entry.signature=entry.date+"|"+entry.code+"|"+entry.state;return true;
}

bool AlarmClient::getLastEvent(AlarmEntry &entry,int *httpStatus){
  String r;int s=0;if(!httpGet("/SystemLog.htm",r,s)){if(httpStatus)*httpStatus=s;return false;}
  if(!extractFirstRow(r,entry)){lastError_=F("SystemLog.htm recu mais row 1 introuvable");if(httpStatus)*httpStatus=s;return false;}
  if(httpStatus)*httpStatus=s;return true;
}

bool AlarmClient::remoteControl(uint8_t ctrl){
  if(ctrl<1||ctrl>4){lastError_=F("Commande RemoteCtr invalide");return false;}
  String body="Ctrl="+String(ctrl)+"&BypassNum=00&BypassOpt=0";String r;int s=0;return httpPost("/RemoteCtr.htm",body,r,s,3000);
}

bool AlarmClient::setZoneBypass(uint8_t zone,bool enabled){
  if(zone<1||zone>40){lastError_=F("Zone invalide");return false;}
  char z[3];snprintf(z,sizeof(z),"%02u",zone);
  String body="Ctrl=0&BypassNum="+String(z)+"&BypassOpt="+String(enabled?1:2);String r;int s=0;return httpPost("/RemoteCtr.htm",body,r,s,3000);
}

bool AlarmClient::discoverAlarm(const String &prefix,IPAddress &found,uint16_t timeoutPerHostMs){
  // prefix attendu sous la forme "192.168.0". Le scan est volontairement limité
  // au /24 choisi par l'utilisateur pour rester prédictible et portable.
  for(int host=1;host<=254;++host){
    IPAddress ip;
    if(!ip.fromString(prefix+"."+String(host)))continue;
    AlarmTcpClient c; c.setTimeout(timeoutPerHostMs);
    if(!c.connect(ip,80))continue;
    c.print(F("GET /SystemLog.htm HTTP/1.0\r\nConnection: close\r\n\r\n"));
    uint32_t t0=millis();String head;
    while(millis()-t0<250 && head.length()<300){while(c.available())head+=(char)c.read();if(head.indexOf("HTTP/")>=0)break;delay(1);}
    c.stop();
    // Une réponse 401 est aussi un excellent indicateur : le serveur existe et
    // protège la page comme la centrale. Une réponse 200 peut être testée ensuite
    // avec les identifiants via le bouton "Tester".
    if(head.indexOf("HTTP/")>=0){found=ip;return true;}
  }
  lastError_=F("Aucune centrale HTTP trouvee sur le sous-reseau");return false;
}
