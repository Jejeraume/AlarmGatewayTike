#include "WebInterface.h"

#include "WebInterface.h"
#include <Update.h>

WebInterface::WebInterface(ConfigStore &c, AlarmClient &a)
  : config_(c), alarm_(a) {}
  

String WebInterface::esc(const String&s)const{
	String o;
	for(size_t i=0;i<s.length();++i){
		switch(s[i]){
			case'&':o+=F("&amp;");
				break;
			case'<':o+=F("&lt;");
				break;
			case'>':o+=F("&gt;");
				break;
			case'\"':o+=F("&quot;");
				break;
			case'\'':o+=F("&#39;");
				break;
			default:o+=s[i];}
		}
		return o;
	}

String WebInterface::pageHeader(const String&t)const{
	String p=F("<!doctype html><html lang='fr'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'><style>body{font-family:Arial;background:#f3f5f7;color:#20252b;margin:0;padding:18px}.box{max-width:1050px;margin:auto;background:#fff;padding:20px;border-radius:12px;box-shadow:0 2px 12px #0002}nav a{margin-right:14px}label{display:block;font-weight:600;margin-top:10px}input,select{padding:8px;border:1px solid #bbb;border-radius:6px;max-width:100%}input.txt{width:420px}button{padding:9px 13px;margin:8px 6px 0 0;border:0;border-radius:6px;cursor:pointer}.pri{background:#1769aa;color:white}.sec{background:#e9edf1}.ok{color:#087a35;font-weight:bold}.bad{color:#b3261e;font-weight:bold}table{width:100%;border-collapse:collapse;margin-top:12px}th,td{border-bottom:1px solid #ddd;padding:7px;text-align:left}.mono{font-family:monospace;background:#f5f6f7;padding:9px;border-radius:6px}.muted{color:#667;font-size:.92em}.warn{background:#fff4ce;padding:10px;border-radius:6px}</style><title>");
	p+=esc(t);
	p+=F("</title></head><body><div class='box'><nav><a href='/'>Configuration</a><a href='/events'>Evenements</a><a href='/commands'>Commandes SMS</a><a href='/backup'>Import / Export</a><a href='/update'>Mise a jour</a></nav><h1>");
	p+=esc(t);
	p+=F("</h1>");
	return p;}

String WebInterface::pageFooter()const{String p=F("<hr><div class='muted'>AlarmGateway ");p+=ALARM_GATEWAY_VERSION;p+=F(" - ");p+=BOARD_FRIENDLY_NAME;p+=F("</div></div></body></html>");return p;}

String WebInterface::mainPage()const{
	const auto&c=config_.data();
	const auto&s=config_.snapshot();
	String p=pageHeader("Alarm Gateway");
	p+=F("<div>Wi-Fi : <b>");
	p+=(WiFi.status()==WL_CONNECTED?F("STA"):F("hors ligne"));
	p+=F("</b> / ");
	p+=(WiFi.status()==WL_CONNECTED?WiFi.localIP().toString():String("0.0.0.0"));
	p += F("<br>Ethernet : <b>");
	p += ETH.linkUp() ? F("OK") : F("hors ligne");
	p += F("</b> / ");
	p += ETH.localIP().toString();
	p+=F("</div><h2>Configuration</h2><form method='post' action='/save'><label>SSID Wi-Fi</label><input class='txt' name='ssid' value='");
	p+=esc(c.wifiSsid);
	p+=F("'><label>Mot de passe Wi-Fi</label><input class='txt' type='password' name='wpass' value='");
	p+=esc(c.wifiPassword);
	p+=F("'><label>IP Ethernet de l'ESP</label><input name='ethip' value='");
	p+=esc(c.ethernetLocalIp);
	p+=F("'><label>Masque Ethernet</label><input name='mask' value='");
	p+=esc(c.ethernetNetmask);
	p+=F("'><label>Sous-reseau de recherche (prefixe)</label><input name='prefix' value='");
	p+=esc(c.searchPrefix);
	p+=F("'><label>IP centrale</label><input name='alarmip' value='");
	p+=esc(c.alarmIp);
	p+=F("'><label>Utilisateur centrale</label><input name='user' value='");
	p+=esc(c.alarmUser);
	p+=F("'><label>Mot de passe centrale</label><input type='password' name='pass' value='");
	p+=esc(c.alarmPassword);
	p+=F("'><label>Intervalle SystemLog (secondes)</label><input type='number' min='1' max='3600' name='poll' value='");
	p+=String(c.pollSeconds);
	p+=F("'><label>Code SMS apres #PWD</label><input maxlength='8' name='smspwd' value='");
	p+=esc(c.smsPassword);
	p+=F("'><h3>Notifications</h3>");
	for(int i=0;i<4;++i){
		p+=F("<label>Telephone ");
		p+=String(i+1);
		p+=F("</label><input name='phone");
		p+=String(i+1);p+=F("' value='");
		p+=esc(s.phones[i]);
		p+=F("'>");
		}
	p+=F("<label>Nombre de rappels</label><input type='number' min='1' max='15' name='dial' value='");
	p+=String(s.dialCount);
	p += F("'><h3>Correspondance codes / equipements</h3>");
	p += F("<p class='muted'>Laisser le code vide pour une ligne inutilisee.</p>");
	p += F("<table><tr><th>Code</th><th>Nom de l'equipement</th></tr>");

	for (int i = 0; i < MAX_EQUIPMENT_CODES; ++i) {
		p += F("<tr><td><input maxlength='7' name='eqcode");
		p += String(i);
		p += F("' value='");
		p += esc(c.equipmentCodes[i].code);
		p += F("'></td><td><input class='txt' maxlength='31' name='eqname");
		p += String(i);
		p += F("' value='");
		p += esc(c.equipmentCodes[i].name);
		p += F("'></td></tr>");
	}
	p += F("</table><br>");
	p+=F("'><br><button class='pri'>Enregistrer</button></form><form method='post' action='/test' style='display:inline'><button class='sec'>Tester la centrale</button></form><form method='post' action='/search' style='display:inline'><button class='sec'>Rechercher l'IP de l'alarme</button></form><h2>Etat</h2>");
	p+=alarmReachable_?F("<div class='ok'>Centrale joignable</div>"):F("<div class='bad'>Centrale non validee</div>");
	p+=F("<div class='mono'>");
	if(haveEntry_){
		p+=F("Date : ");
		p+=esc(lastEntry_.date);
		p+=F("<br>Code : ");
		p+=esc(lastEntry_.code);
		p+=F("<br>Etat : ");
		p+=esc(lastEntry_.state);
	}
	else p+=F("Aucun etat encore memorise");
	if(lastMessage_.length()){
		p+=F("<br>Info : ");
		p+=esc(lastMessage_);
	}
	p+=F("</div>");
	return p+pageFooter();
}

String WebInterface::eventsPage()const{String p=pageHeader("Evenements");
	p+=F(
	"<p>Configuration locale des notifications.</p>"
	"<form method='post' action='/readEvents'>"
	"<button class='sec'>Lire evenements programmes</button>"
	"</form>"
	);

	if(lastMessage_.length()){
  p+=F("<div class='mono'>");
  p+=esc(lastMessage_);
  p+=F("</div>");
}

	p+=F(
  "<form method='post' action='/saveEvents'>"
  "<table>"
  "<tr>"
  "<th>#</th>"
  "<th>Evenement</th>"
  "<th>Appel</th>"
  "<th>SMS</th>"
  "<th>HomeAssistant</th>"
  "</tr>"
);
	for(int i=0;i<40;++i){
		const auto&e=config_.snapshot().events[i];
		p+=F("<tr><td>");
		p+=String(i+1);
		p+=F("</td><td>");
		p+=esc(e.name);
		p+=F("</td>");
		const char*names[]={"voice","sms","HomeAssistant"};
		const bool vals[]={e.voice,e.sms,e.HomeAssistant};
		for(int j=0;j<3;++j){
			p+=F("<td><input type='checkbox' name='");
			p+=names[j];p+=String(i+1);
			p+=F("' ");
			if(vals[j])p+=F("checked");
				p+=F("></td>");
			}
			p+=F("</tr>");
		}
	p+=F("</table><button class='pri'>Enregistrer les evenements</button></form>");
	return p+pageFooter();}
	
String WebInterface::commandsPage()const{
	String p=pageHeader("Table d'equivalence SMS");
	p+=F("<p>Format recu : <code>#PWD000000#DESARMER</code>.</p><form method='post' action='/saveCommands'><table><tr><th>Actif</th><th>Commande recue</th><th>Action</th></tr>");
	const auto*r=config_.commandRules();
	for(size_t i=0;i<ConfigStore::commandRuleCount();++i){
		p+=F("<tr><td><input type='checkbox' name='en");
		p+=String(i);
		p+=F("' ");
		if(r[i].enabled)p+=F("checked");
		p+=F("></td><td><input name='cmd");
		p+=String(i);
		p+=F("' value='");
		p+=esc(r[i].command);
		p+=F("'></td><td>");
		p+=smsActionName(r[i].action);
		p+=F("</td></tr>");
	}
	p+=F("</table><button class='pri'>Enregistrer la table</button></form>");
	return p+pageFooter();
}

void WebInterface::redirect(const char*path){
	server_.sendHeader("Location",path,true);
	server_.send(303,"text/plain","");
}

void WebInterface::handleSave(){
	auto&c=config_.data();
	auto&s=config_.snapshot();
	if(server_.hasArg("ssid"))strlcpy(c.wifiSsid,server_.arg("ssid").c_str(),sizeof(c.wifiSsid));
	if(server_.hasArg("wpass"))strlcpy(c.wifiPassword,server_.arg("wpass").c_str(),sizeof(c.wifiPassword));
	if(server_.hasArg("alarmip"))strlcpy(c.alarmIp,server_.arg("alarmip").c_str(),sizeof(c.alarmIp));
	if(server_.hasArg("user"))strlcpy(c.alarmUser,server_.arg("user").c_str(),sizeof(c.alarmUser));
	if(server_.hasArg("pass"))strlcpy(c.alarmPassword,server_.arg("pass").c_str(),sizeof(c.alarmPassword));
	if(server_.hasArg("ethip"))strlcpy(c.ethernetLocalIp,server_.arg("ethip").c_str(),sizeof(c.ethernetLocalIp));
	if(server_.hasArg("mask"))strlcpy(c.ethernetNetmask,server_.arg("mask").c_str(),sizeof(c.ethernetNetmask));
	if(server_.hasArg("prefix"))strlcpy(c.searchPrefix,server_.arg("prefix").c_str(),sizeof(c.searchPrefix));
	if(server_.hasArg("smspwd"))strlcpy(c.smsPassword,server_.arg("smspwd").c_str(),sizeof(c.smsPassword));
	if(server_.hasArg("poll")){int n=server_.arg("poll").toInt();
	if(n>=MIN_POLL_SECONDS&&n<=MAX_POLL_SECONDS)c.pollSeconds=n;
	}
	for(int i=0;i<4;++i){
		String k="phone"+String(i+1);
		if(server_.hasArg(k))strlcpy(s.phones[i],server_.arg(k).c_str(),sizeof(s.phones[i]));
	}
	if(server_.hasArg("dial")){int n=server_.arg("dial").toInt();if(n>=1&&n<=15)s.dialCount=n;}
	for (int i = 0; i < MAX_EQUIPMENT_CODES; ++i) {
		String codeKey = "eqcode" + String(i);
		String nameKey = "eqname" + String(i);
		if (server_.hasArg(codeKey)) {
			String value = server_.arg(codeKey);
			value.trim();
			strlcpy(c.equipmentCodes[i].code, value.c_str(), sizeof(c.equipmentCodes[i].code));
		}

		if (server_.hasArg(nameKey)) {
			String value = server_.arg(nameKey);
			value.trim();
			strlcpy(c.equipmentCodes[i].name, value.c_str(), sizeof(c.equipmentCodes[i].name));
		}
	}
	s.phoneDataValid=true;c.setupCompleted=strlen(c.alarmIp)>0;
	config_.save();
	config_.saveSnapshot();
	lastMessage_=F("Configuration enregistree. Redemarrer si le Wi-Fi/Ethernet a change.");
	redirect();
}
void WebInterface::handleSaveEvents(){
	auto&s=config_.snapshot();
	for(int i=0;i<40;++i){
		int n=i+1;
		s.events[i].voice=server_.hasArg("voice"+String(n));
		s.events[i].sms=server_.hasArg("sms"+String(n));
		s.events[i].HomeAssistant=server_.hasArg("HomeAssistant"+String(n));
		s.events[i].valid=true;
		}
	s.eventDataValid=true;
	config_.saveSnapshot();
	lastMessage_=F("Configuration des evenements enregistree");redirect("/events");
	}

void WebInterface::handleReadEvents() {
  AlarmProgrammedEvent events[40];
  size_t count = 0;

  if (!alarm_.readProgrammedEvents(events, 40, count)) {
    lastMessage_ = String(F("Erreur lecture evenements : ")) + alarm_.lastError();
    redirect("/events");
    return;
  }

  auto &snapshot = config_.snapshot();

  for (size_t i = 0; i < count; ++i) {
    int index = events[i].code - 1;

    if (index < 0 || index >= 40)
      continue;

    strlcpy(
      snapshot.events[index].name,
      events[i].name.c_str(),
      sizeof(snapshot.events[index].name)
    );

    snapshot.events[index].valid = true;
  }

  snapshot.eventDataValid = true;
  config_.saveSnapshot();

  lastMessage_ =
    String(count) + F(" evenements lus depuis la centrale");

  redirect("/events");
}

void WebInterface::handleSaveCommands(){
	auto*r=config_.commandRules();
	for(size_t i=0;i<ConfigStore::commandRuleCount();++i){
		String k="cmd"+String(i);
		if(server_.hasArg(k))strlcpy(r[i].command,server_.arg(k).c_str(),sizeof(r[i].command));
		r[i].enabled=server_.hasArg("en"+String(i));
		}
	config_.saveCommandRules();
	lastMessage_=F("Table SMS enregistree");
	redirect("/commands");
}

void WebInterface::handleExportCfg(){
	String data;
	config_.exportCfg(data);
	server_.sendHeader("Content-Disposition","attachment; filename=AlarmGateway.cfg");
	server_.send(200,"application/octet-stream",data);
}

void WebInterface::handleCfgUpload(){
	HTTPUpload&up=server_.upload();
	if(up.status==UPLOAD_FILE_START){
		cfgUpload_="";
		cfgUpload_.reserve(8000);
	}
	else if(up.status==UPLOAD_FILE_WRITE){
		if(cfgUpload_.length()+up.currentSize<=16000)for(size_t i=0;i<up.currentSize;++i)cfgUpload_+=(char)up.buf[i];}
		else if(up.status==UPLOAD_FILE_ABORTED){
			cfgUpload_="";
		}
	}
void WebInterface::handleImportCfg(){
	String err;
	if(cfgUpload_.length()&&config_.importCfg(cfgUpload_,err)){
		lastMessage_=F("Configuration .cfg importee. Redemarrage recommande.");
		server_.send(200,"text/html; charset=utf-8",pageHeader("Import termine")+F("<p class='ok'>Configuration importee avec succes.</p><p><a href='/'>Retour</a></p>")+pageFooter());
	}
	else{
		if(!err.length())err=F("Fichier vide ou trop volumineux");server_.send(400,"text/html; charset=utf-8",pageHeader("Erreur import")+String("<p class='bad'>")+esc(err)+F("</p><p>La configuration existante a ete conservee.</p><p><a href='/backup'>Retour</a></p>")+pageFooter());
	}
	cfgUpload_="";
}

void WebInterface::handleUpdateUpload() {
	HTTPUpload &up = server_.upload();
	if (up.status == UPLOAD_FILE_START) {
		Serial.printf("[WEB UPDATE] %s\n", up.filename.c_str());
		if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
			Update.printError(Serial);
		}
	} 
	else if (up.status == UPLOAD_FILE_WRITE) {
		if (Update.write(up.buf, up.currentSize) != up.currentSize) {
			Update.printError(Serial);
		}
	} 	
	else if (up.status == UPLOAD_FILE_END) {
		if (!Update.end(true)) {
			Update.printError(Serial);
		} 
		else {
			Serial.printf("[WEB UPDATE] Termine : %u octets\n", up.totalSize);
		}
	} 
	else if (up.status == UPLOAD_FILE_ABORTED) {
    Update.abort();
    Serial.println(F("[WEB UPDATE] Upload annule"));
	}
}


void WebInterface::begin(){
	server_.on("/",HTTP_GET,[this](){
		server_.send(200,"text/html; charset=utf-8",mainPage());});
	server_.on("/events",HTTP_GET,[this](){
		server_.send(200,"text/html; charset=utf-8",eventsPage());});
	server_.on("/commands",HTTP_GET,[this](){
		server_.send(200,"text/html; charset=utf-8",commandsPage());});
	server_.on("/backup",HTTP_GET,[this](){
		String p=pageHeader("Import / Export .cfg");p+=F("<p class='warn'><b>Attention :</b> le fichier .cfg contient les mots de passe Wi-Fi, centrale et SMS en clair.</p><p><a href='/export.cfg'><button class='pri'>Exporter la configuration</button></a></p><form method='post' action='/import' enctype='multipart/form-data'><input type='file' name='config' accept='.cfg'><button class='sec'>Importer le fichier .cfg</button></form>");server_.send(200,"text/html; charset=utf-8",p+pageFooter());});
	server_.on("/export.cfg",HTTP_GET,[this](){
		handleExportCfg();});
	server_.on("/import",HTTP_POST,[this](){
		handleImportCfg();},[this](){handleCfgUpload();});server_.on("/save",HTTP_POST,[this](){handleSave();});
	server_.on("/saveEvents",HTTP_POST,[this](){
		handleSaveEvents();});
//Ajout de la route
	server_.on("/readEvents", HTTP_POST, [this]() {
		handleReadEvents();});	
	server_.on("/saveCommands",HTTP_POST,[this](){
		handleSaveCommands();});server_.on("/test",HTTP_POST,[this](){testReq_=true;redirect();});
	server_.on("/search",HTTP_POST,[this](){
		searchReq_=true;redirect();});
	server_.on("/update",HTTP_GET,[this](){
		String p=pageHeader("Mise a jour OTA");p+=F("<form method='post' action='/update' enctype='multipart/form-data'><input type='file' name='firmware' accept='.bin'><button class='pri'>Mettre a jour</button></form><p class='muted'>La configuration LittleFS est conservee.</p>");server_.send(200,"text/html; charset=utf-8",p+pageFooter());});
	server_.on("/update",HTTP_POST,[this](){
		bool ok=!Update.hasError();server_.send(200,"text/plain",ok?"Mise a jour OK - redemarrage":"Erreur mise a jour");if(ok){delay(500);ESP.restart();}},[this](){handleUpdateUpload();});
	server_.onNotFound([this](){
		server_.send(404,"text/plain","404");});
	server_.begin();}
	
void WebInterface::loop(){
		server_.handleClient();
}
void WebInterface::setLastEvent(const AlarmEntry&e,bool v){
	lastEntry_=e;haveEntry_=v;
}

bool WebInterface::consumeTestRequest(){
	bool v=testReq_;
	testReq_=false;
	return v;
}

bool WebInterface::consumeSearchRequest(){
	bool v=searchReq_;
	searchReq_=false;
	return v;
}

