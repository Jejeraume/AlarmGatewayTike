#include "ConfigStore.h"
#include <IPAddress.h>
#include "BuildConfig.h"
#include <LittleFS.h>

static constexpr uint32_t CONFIG_MAGIC = 0xA1A6A505;
static constexpr uint32_t SNAPSHOT_MAGIC = 0xA1A6C001;
static constexpr uint16_t CONFIG_VERSION = 3;

bool ConfigStore::loadBinary(const char *path, void *dst, size_t len) {
  File f = LittleFS.open(path, "r");
  if (!f || f.size() != len) return false;
  bool ok = f.read(reinterpret_cast<uint8_t*>(dst), len) == len;
  f.close();
  return ok;
}

bool ConfigStore::saveBinary(const char *path, const void *src, size_t len) {
  File f = LittleFS.open(path, "w");
  if (!f) return false;
  bool ok = f.write(reinterpret_cast<const uint8_t*>(src), len) == len;
  f.close();
  return ok;
}

void ConfigStore::defaultRules() {
  auto set = [this](int i, const char *cmd, SmsAction action, bool enabled) {
    strlcpy(rules_[i].command, cmd, sizeof(rules_[i].command));
    rules_[i].action = action;
    rules_[i].enabled = enabled;
  };
  set(0, "ARMER", SmsAction::ARM_TOTAL, true);
  set(1, "DESARMER", SmsAction::DISARM, true);
  set(2, "VERIFIER", SmsAction::STATUS, true);
  set(3, "MAISON", SmsAction::ARM_HOME, true);
  set(4, "OUVRIR PGM", SmsAction::PGM_ON, false);
  set(5, "FERMER PGM", SmsAction::PGM_OFF, false);
  set(6, "OUVRIR COMMUT.01", SmsAction::SWITCH_ON, false);
  set(7, "FERMER COMMUT.01", SmsAction::SWITCH_OFF, false);
}


void ConfigStore::defaultEvents() {
  static const char *names[40] = {
    "Delai","Perimetre","Intrusion","Urgence","Permanente","Incendie","Panic","Ouverture capteur",
    "Arme total","Systeme desarme","Systeme arme","Batterie faible","Coupure secteur","Secteur retabli",
    "Annulation alarme","Detecteur batterie faible","Detecteur batterie ok","RF perdu","Programation change",
    "Armement echoue","Teste de rapport periodique","Zone Bypass","Systeme batterie retabli","Echec de commmunication",
    "Zone Bypass annule","Communication retablie","Erreur boucle","Boucle retablie","Defaillance de la cloche",
    "bell a Restaurer","Contrainte","Recup. Entrer/Sortir","Recup. Perimetre","Recup. Vol","Recup. Panique alarme",
    "Recup. 24 Heures","Recup. Feu","Recup. Panique alarme","Recup. Temporiser","Recup. Perte dispositif RF"
  };
  for (uint8_t i=0;i<40;++i) {
    snapshot_.events[i].id=i+1;
    strlcpy(snapshot_.events[i].name,names[i],sizeof(snapshot_.events[i].name));
    snapshot_.events[i].valid=true;
    snapshot_.events[i].cms=false; snapshot_.events[i].voice=false;
    snapshot_.events[i].sms=true; snapshot_.events[i].email=false;
  }
  snapshot_.eventDataValid=true;
  snapshot_.phoneDataValid=true;
}

void ConfigStore::defaults() {
  memset(&cfg_, 0, sizeof(cfg_));
  cfg_.magic = CONFIG_MAGIC;
  cfg_.version = CONFIG_VERSION;
  strlcpy(cfg_.ethernetLocalIp, DEFAULT_ETH_LOCAL_IP, sizeof(cfg_.ethernetLocalIp));
  strlcpy(cfg_.ethernetNetmask, DEFAULT_ETH_NETMASK, sizeof(cfg_.ethernetNetmask));
  strlcpy(cfg_.searchPrefix, DEFAULT_SEARCH_PREFIX, sizeof(cfg_.searchPrefix));
  cfg_.pollSeconds = DEFAULT_POLL_SECONDS;
  strlcpy(cfg_.smsPassword, "000000", sizeof(cfg_.smsPassword));
  cfg_.setupCompleted = false;

  memset(&snapshot_, 0, sizeof(snapshot_));
  snapshot_.magic = SNAPSHOT_MAGIC;
  snapshot_.dialCount = 1;
  defaultEvents();
  defaultRules();
}

bool ConfigStore::begin() {
#if defined(ESP32)
  if (!LittleFS.begin(true)) return false;
#else
  if (!LittleFS.begin()) return false;
#endif

  if (!loadBinary("/config.bin", &cfg_, sizeof(cfg_)) ||
      cfg_.magic != CONFIG_MAGIC || cfg_.version != CONFIG_VERSION) {
    defaults();
    save();
    saveSnapshot();
    saveCommandRules();
    return true;
  }

  if (!loadBinary("/central.bin", &snapshot_, sizeof(snapshot_)) || snapshot_.magic != SNAPSHOT_MAGIC) {
    memset(&snapshot_, 0, sizeof(snapshot_));
    snapshot_.magic = SNAPSHOT_MAGIC;
    snapshot_.dialCount = 1;
    defaultEvents();
  }

  if (!loadBinary("/commands.bin", rules_, sizeof(rules_))) defaultRules();

  // Toujours terminer les chaînes fixes, même après une coupure pendant écriture.
  cfg_.wifiSsid[sizeof(cfg_.wifiSsid)-1] = 0;
  cfg_.wifiPassword[sizeof(cfg_.wifiPassword)-1] = 0;
  cfg_.alarmIp[sizeof(cfg_.alarmIp)-1] = 0;
  cfg_.alarmUser[sizeof(cfg_.alarmUser)-1] = 0;
  cfg_.alarmPassword[sizeof(cfg_.alarmPassword)-1] = 0;
  cfg_.ethernetLocalIp[sizeof(cfg_.ethernetLocalIp)-1] = 0;
  cfg_.ethernetNetmask[sizeof(cfg_.ethernetNetmask)-1] = 0;
  cfg_.searchPrefix[sizeof(cfg_.searchPrefix)-1] = 0;
  cfg_.smsPassword[sizeof(cfg_.smsPassword)-1] = 0;

  if (cfg_.pollSeconds < MIN_POLL_SECONDS || cfg_.pollSeconds > MAX_POLL_SECONDS)
    cfg_.pollSeconds = DEFAULT_POLL_SECONDS;
  return true;
}

bool ConfigStore::save() {
  cfg_.magic = CONFIG_MAGIC;
  cfg_.version = CONFIG_VERSION;
  return saveBinary("/config.bin", &cfg_, sizeof(cfg_));
}

bool ConfigStore::saveSnapshot() {
  snapshot_.magic = SNAPSHOT_MAGIC;
  ++snapshot_.revision;
  return saveBinary("/central.bin", &snapshot_, sizeof(snapshot_));
}

bool ConfigStore::saveCommandRules() {
  return saveBinary("/commands.bin", rules_, sizeof(rules_));
}


static String cfgValue(const String &line) {
  int p=line.indexOf('='); return p<0 ? String() : line.substring(p+1);
}
static bool cfgBool(const String &v) { return v=="1" || v=="true" || v=="TRUE" || v=="on"; }

bool ConfigStore::exportCfg(String &out) const {
  out.reserve(7000); out="formatVersion=1\n";
  out += "firmwareVersion=" ALARM_GATEWAY_VERSION "\n";
  out += "wifi.ssid="+String(cfg_.wifiSsid)+"\n";
  out += "wifi.password="+String(cfg_.wifiPassword)+"\n";
  out += "alarm.ip="+String(cfg_.alarmIp)+"\n";
  out += "alarm.user="+String(cfg_.alarmUser)+"\n";
  out += "alarm.password="+String(cfg_.alarmPassword)+"\n";
  out += "ethernet.ip="+String(cfg_.ethernetLocalIp)+"\n";
  out += "ethernet.netmask="+String(cfg_.ethernetNetmask)+"\n";
  out += "search.prefix="+String(cfg_.searchPrefix)+"\n";
  out += "poll.seconds="+String(cfg_.pollSeconds)+"\n";
  out += "sms.password="+String(cfg_.smsPassword)+"\n";
  for(int i=0;i<4;++i) out += "phone."+String(i+1)+"="+String(snapshot_.phones[i])+"\n";
  out += "phone.dialCount="+String(snapshot_.dialCount)+"\n";
  for(int i=0;i<40;++i){ const auto&e=snapshot_.events[i]; String k="event."+String(i+1)+".";
    out+=k+"cms="+String(e.cms?1:0)+"\n"; out+=k+"voice="+String(e.voice?1:0)+"\n";
    out+=k+"sms="+String(e.sms?1:0)+"\n"; out+=k+"email="+String(e.email?1:0)+"\n"; }
  for(size_t i=0;i<commandRuleCount();++i){ out+="command."+String(i)+".enabled="+String(rules_[i].enabled?1:0)+"\n"; out+="command."+String(i)+".text="+String(rules_[i].command)+"\n"; }
  for (int i = 0; i < MAX_EQUIPMENT_CODES; ++i) {
	if (cfg_.equipmentCodes[i].code[0] != '\0') {
		out += "equipment." + String(i) + ".code=" +
           String(cfg_.equipmentCodes[i].code) + "\n";

		out += "equipment." + String(i) + ".name=" +
           String(cfg_.equipmentCodes[i].name) + "\n";
	}
  }
  return true;
}

bool ConfigStore::importCfg(const String &text, String &error) {
  StoredConfig nc=cfg_; CentralSnapshot ns=snapshot_; SmsCommandRule nr[8]; memcpy(nr,rules_,sizeof(nr));
  bool versionSeen=false; int pos=0;
  while(pos < (int)text.length()) {
    int end=text.indexOf('\n',pos); if(end<0) end=text.length(); String line=text.substring(pos,end); pos=end+1; line.trim();
    if(!line.length() || line[0]=='#' || line[0]==';') continue;
    int eq=line.indexOf('='); if(eq<1) continue; String key=line.substring(0,eq), val=cfgValue(line); key.trim(); val.trim();
    if(key=="formatVersion"){ if(val!="1"){error="Version .cfg non supportee";return false;} versionSeen=true; }
    else if(key=="wifi.ssid") strlcpy(nc.wifiSsid,val.c_str(),sizeof(nc.wifiSsid));
    else if(key=="wifi.password") strlcpy(nc.wifiPassword,val.c_str(),sizeof(nc.wifiPassword));
    else if(key=="alarm.ip") strlcpy(nc.alarmIp,val.c_str(),sizeof(nc.alarmIp));
    else if(key=="alarm.user") strlcpy(nc.alarmUser,val.c_str(),sizeof(nc.alarmUser));
    else if(key=="alarm.password") strlcpy(nc.alarmPassword,val.c_str(),sizeof(nc.alarmPassword));
    else if(key=="ethernet.ip") strlcpy(nc.ethernetLocalIp,val.c_str(),sizeof(nc.ethernetLocalIp));
    else if(key=="ethernet.netmask") strlcpy(nc.ethernetNetmask,val.c_str(),sizeof(nc.ethernetNetmask));
    else if(key=="search.prefix") strlcpy(nc.searchPrefix,val.c_str(),sizeof(nc.searchPrefix));
    else if(key=="poll.seconds"){int n=val.toInt();if(n<MIN_POLL_SECONDS||n>MAX_POLL_SECONDS){error="poll.seconds invalide";return false;}nc.pollSeconds=n;}
    else if(key=="sms.password") strlcpy(nc.smsPassword,val.c_str(),sizeof(nc.smsPassword));
    else if(key.startsWith("phone.")){String sub=key.substring(6);if(sub=="dialCount"){int n=val.toInt();if(n<1||n>15){error="phone.dialCount invalide";return false;}ns.dialCount=n;}else{int n=sub.toInt();if(n>=1&&n<=4)strlcpy(ns.phones[n-1],val.c_str(),sizeof(ns.phones[n-1]));}}
    else if(key.startsWith("event.")){int d1=key.indexOf('.',6);if(d1>6){int n=key.substring(6,d1).toInt();String f=key.substring(d1+1);if(n>=1&&n<=40){auto&e=ns.events[n-1];if(f=="cms")e.cms=cfgBool(val);else if(f=="voice")e.voice=cfgBool(val);else if(f=="sms")e.sms=cfgBool(val);else if(f=="email")e.email=cfgBool(val);e.valid=true;}}}
    else if(key.startsWith("command.")){int d1=key.indexOf('.',8);if(d1>8){int n=key.substring(8,d1).toInt();String f=key.substring(d1+1);if(n>=0&&n<8){if(f=="enabled")nr[n].enabled=cfgBool(val);else if(f=="text")strlcpy(nr[n].command,val.c_str(),sizeof(nr[n].command));}}}
	else if (key.startsWith("equipment.")) {
		int d1 = key.indexOf('.', 10);

		if (d1 > 10) {
			int n = key.substring(10, d1).toInt();
			String field = key.substring(d1 + 1);

			if (n >= 0 && n < MAX_EQUIPMENT_CODES) {
				if (field == "code") 
				{
					strlcpy(nc.equipmentCodes[n].code,val.c_str(),sizeof(nc.equipmentCodes[n].code));
				}
			else if (field == "name") 
				{
					strlcpy(nc.equipmentCodes[n].name,val.c_str(),sizeof(nc.equipmentCodes[n].name));
				}
		}
	}
}
  }
  if(!versionSeen){error="formatVersion absent";return false;}
  IPAddress tmp; if(strlen(nc.alarmIp) && !tmp.fromString(nc.alarmIp)){error="alarm.ip invalide";return false;}
  if(!tmp.fromString(nc.ethernetLocalIp)){error="ethernet.ip invalide";return false;}
  nc.magic=CONFIG_MAGIC; nc.version=CONFIG_VERSION; nc.setupCompleted=strlen(nc.alarmIp)>0; ns.magic=SNAPSHOT_MAGIC; ns.phoneDataValid=true; ns.eventDataValid=true;
  cfg_=nc; snapshot_=ns; memcpy(rules_,nr,sizeof(rules_));
  if(!save() || !saveSnapshot() || !saveCommandRules()){error="Ecriture LittleFS impossible";return false;}
  error=""; return true;
}
