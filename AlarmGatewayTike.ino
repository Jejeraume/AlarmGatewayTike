/*
 * AlarmGateway 5.0.0-dev3-V3dhcp
 * =================================
 * Base materielle/reseau reprise de la V3 ESP8266 + ENC28J60 validee.
 *
 * IMPORTANT :
 * - L'objet ENC28J60 est global, comme dans la V3.
 * - Le Wi-Fi est coupe pendant l'initialisation ENC28J60.
 * - SPI = GPIO14/12/13, CS = GPIO5, 4 MHz, MODE0, MSBFIRST.
 * - Configuration IP ENC28J60 AVANT begin(), conformement a ENC28J60lwIP.
 * - Aucune lecture de UserPhone.htm ni AlarmEvent.htm.
 * - Telephones, rappels et 40 evenements = configuration locale.
 * - SystemLog.htm = lecture periodique.
 * - RemoteCtr.htm = commandes centrale.
 */
#include <Arduino.h>
#include <SPI.h>
#include <ESP8266WiFi.h>
#include <ENC28J60lwIP.h>

#include "SoftwareSerialLocal.h"
#include "BuildConfig.h"
#include "ConfigStore.h"
#include "AlarmClient.h"
#include "AlarmTypes.h"
#include "ModemAT.h"
#include "SmsCommands.h"
#include "WebInterface.h"
#include "OtaManager.h"

#define MODEM_RX_PIN 4
#define MODEM_TX_PIN 0
#define MODEM_BAUD 115200

// -----------------------------------------------------------------------------
// Ethernet : meme forme que la V3 fonctionnelle.
// Cet objet doit rester construit globalement avant les objets applicatifs.
// -----------------------------------------------------------------------------
ENC28J60lwIP eth(ENC28J60_CS_PIN);
byte macAddress[6] = { 0x02, 0x82, 0x66, 0x10, 0x20, 0x30 };

// -----------------------------------------------------------------------------
// Application
// -----------------------------------------------------------------------------
ConfigStore config;
AlarmClient alarmClient(config);
SoftwareSerial modemSerial(MODEM_RX_PIN, MODEM_TX_PIN); 
ModemAT modem(modemSerial);
SmsCommands smsCommands(config, alarmClient);
WebInterface web(config, alarmClient, eth);
OtaManager ota;

AlarmEntry lastEntry;
bool haveLastEntry = false;
bool ledLatched = false;
uint32_t lastPollMs = 0;

static bool parseIPv4(const char *text, IPAddress &ip) {
  return ip.fromString(text ? text : "");
}

static void setLed(bool on) {
  ledLatched = on;
  digitalWrite(STATUS_LED_PIN,
               STATUS_LED_ACTIVE_LOW ? (on ? LOW : HIGH) : (on ? HIGH : LOW));
  web.setLedState(on);
}

static String buildSmsMessage(const AlarmEntry &e) {
  String m;

  // Etat en premier
  m += e.state;

  // Recherche de la correspondance Code -> Equipement
  const auto &cfg = config.data();
  const char *equipmentName = nullptr;

  for (int i = 0; i < MAX_EQUIPMENT_CODES; ++i) {
    if (cfg.equipmentCodes[i].code[0] == '\0')
      continue;

    if (e.code == cfg.equipmentCodes[i].code) {
      equipmentName = cfg.equipmentCodes[i].name;
      break;
    }
  }

  // Ajouter l'equipement uniquement si une correspondance existe
  if (equipmentName && equipmentName[0] != '\0') {
    m += F("\nEquipement : ");
    m += equipmentName;
  }

  // Code
  m += F("\nCode : ");
  m += e.code;

  // Date en dernier
  m += '\n';
  m += e.date;

  return m;
}

static bool initEthernetV3() {
  // Initialisation ENC28J60.
  //
  // 1 - Tentative d'obtention d'une adresse par DHCP.
  // 2 - Si aucune adresse apres 10 secondes :
  //       IP ESP = 192.168.0.1
  //       masque = 255.255.255.0
  //
  // Le serveur DHCP de secours sera ajoute ensuite.

  WiFi.mode(WIFI_OFF);
  WiFi.forceSleepBegin();
  delay(1);

  SPI.begin(); // ESP8266 : SCK=14, MISO=12, MOSI=13
  SPI.setBitOrder(MSBFIRST);
  SPI.setDataMode(SPI_MODE0);
  SPI.setFrequency(4000000);

  eth.setDefault();

  Serial.println(F("[ETH] Initialisation ENC28J60..."));

  if (!eth.begin(macAddress)) {
    Serial.println(F("[ETH] ERREUR : ENC28J60 non detecte"));
    return false;
  }

  // --------------------------------------------------
  // Attente d'une adresse DHCP
  // --------------------------------------------------

  Serial.print(F("[ETH] Attente DHCP"));

  uint32_t start = millis();

  while (millis() - start < 10000UL) {

    if (eth.localIP() != IPAddress(0, 0, 0, 0))
      break;

    Serial.print('.');
    delay(500);
    yield();
  }

  Serial.println();

  // --------------------------------------------------
  // Aucun DHCP -> IP statique de secours
  // --------------------------------------------------

  if (eth.localIP() == IPAddress(0, 0, 0, 0)) {

    Serial.println(F("[ETH] Aucun serveur DHCP detecte"));
    Serial.println(F("[ETH] Passage en mode autonome"));

    IPAddress ip(192, 168, 0, 1);
    IPAddress gateway(192, 168, 0, 1);
    IPAddress netmask(255, 255, 255, 0);
    IPAddress dns(192, 168, 0, 1);

    if (!eth.config(ip, gateway, netmask, dns)) {
      Serial.println(F("[ETH] ERREUR configuration IP statique"));
      return false;
    }

    delay(100);

    Serial.print(F("[ETH] IP secours ESP : "));
    Serial.println(eth.localIP());

    Serial.print(F("[ETH] Masque : "));
    Serial.println(eth.subnetMask());

    return true;
  }

  // --------------------------------------------------
  // DHCP obtenu normalement
  // --------------------------------------------------

  Serial.println(F("[ETH] DHCP obtenu"));

  Serial.print(F("[ETH] IP ESP : "));
  Serial.println(eth.localIP());

  Serial.print(F("[ETH] Masque : "));
  Serial.println(eth.subnetMask());

  Serial.print(F("[ETH] Gateway : "));
  Serial.println(eth.gatewayIP());

  return true;
}

/*
static bool initEthernetV3() {
  // Initialisation ENC28J60 reprise STRICTEMENT de la V3 fonctionnelle.
  // Sur l'ESP8266 temporaire, l'interface Ethernet reste en DHCP.
  // Les champs ethernet.ip / ethernet.netmask sont conserves dans la
  // configuration pour la future cible WT32-ETH01 mais ne sont pas appliques ici.

  WiFi.mode(WIFI_OFF);
  WiFi.forceSleepBegin();
  delay(1);

  SPI.begin(); // ESP8266 : SCK=14, MISO=12, MOSI=13
  SPI.setBitOrder(MSBFIRST);
  SPI.setDataMode(SPI_MODE0);
  SPI.setFrequency(4000000);

  eth.setDefault();

  Serial.println(F("[ETH] Initialisation ENC28J60 / V3 DHCP..."));
  if (!eth.begin(macAddress)) {
    Serial.println(F("[ETH] ERREUR : ENC28J60 non detecte"));
    return false;
  }

  Serial.print(F("[ETH] Attente DHCP"));
  while (!eth.connected()) {
    Serial.print('.');
    delay(500);
  }
  Serial.println();

  Serial.print(F("[ETH] IP ESP : "));
  Serial.println(eth.localIP());
  Serial.print(F("[ETH] Masque : "));
  Serial.println(eth.subnetMask());
  Serial.print(F("[ETH] Gateway : "));
  Serial.println(eth.gatewayIP());
  return true;
}
*/

static void startWiFiAfterEthernet() {
  // L'ENC28J60 est deja initialise. On peut maintenant reveiller le Wi-Fi.
  WiFi.forceSleepWake();
  delay(1);

  const auto &c = config.data();
  if (!strlen(c.wifiSsid)) {
    Serial.println(F("[WIFI] Aucun SSID configure"));
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(c.wifiSsid, c.wifiPassword);

  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 12000UL) {
    delay(100);
    yield();
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print(F("[WIFI] Connecte : "));
    Serial.println(WiFi.localIP());
  } else {
    Serial.println(F("[WIFI] Connexion impossible - Ethernet reste actif"));
  }
}

static bool testAlarmNow(bool verbose) {
  AlarmEntry e;
  int st = 0;
  if (!alarmClient.getLastEvent(e, &st)) {
    web.setAlarmReachable(false);
    web.setLastMessage(alarmClient.lastError());
    return false;
  }
  web.setAlarmReachable(true);
  web.setLastEvent(e, true);
  if (verbose)
    web.setLastMessage(F("Test reussi : communication et identifiants valides"));
  return true;
}

static void notifyEvent(const AlarmEntry &e) {
  const auto &snap = config.snapshot();
  const CentralEventConfig *eventCfg = nullptr;

  String state = e.state;
  state.trim();
  state.toLowerCase();

  for (uint8_t i = 0; i < 40; ++i) {
    String n = snap.events[i].name;
    n.trim();
    n.toLowerCase();
    if (n == state) {
      eventCfg = &snap.events[i];
      break;
    }
  }

  bool sendSms = eventCfg ? eventCfg->sms : true;
  bool makeCall = eventCfg ? eventCfg->voice : e.isIntrusion();
  if (!sendSms && !makeCall) return;

  PhoneList phones;
  for (uint8_t i = 0; i < 4; ++i) phones.phone[i] = snap.phones[i];
  phones.dialCount = snap.dialCount;

  if (phones.countValid() == 0) {
    web.setLastMessage(F("Evenement detecte mais aucun telephone configure"));
    return;
  }

  if (sendSms && !modem.sendSMS(phones, buildSmsMessage(e)))
    web.setLastMessage(F("Au moins un SMS a echoue"));

  if (makeCall && !modem.callWithRetries(phones, phones.dialCount, 20000))
    web.setLastMessage(F("Appels echoues"));
}


static bool isConfiguredPhone(const String &sender) {
  String n = normalizeFrenchPhone(sender);
  const auto &snap = config.snapshot();
  for (uint8_t i = 0; i < 4; ++i) {
    if (!snap.phones[i][0]) continue;
    if (normalizeFrenchPhone(String(snap.phones[i])) == n) return true;
  }
  return false;
}

static void handleIncomingSMS() {
  String sender, text;
  if (!modem.takeReceivedSMS(sender, text)) return;

  Serial.print(F("[SMS RX] "));
  Serial.print(sender);
  Serial.print(F(" : "));
  Serial.println(text);

  if (!isConfiguredPhone(sender)) {
    Serial.println(F("[SMS RX] Expediteur non autorise"));
    return;
  }

  SmsAction action;
  String commandName, error, reply;
  if (!smsCommands.parse(text, action, commandName, error)) {
    Serial.print(F("[SMS RX] Ignore : "));
    Serial.println(error);
    return;
  }

  bool ok = smsCommands.execute(action, reply);
  if (!reply.length()) reply = ok ? F("Commande executee") : F("Commande en echec");
  modem.sendSMS(sender, reply);
}

static void pollAlarm() {
  AlarmEntry cur;
  if (!alarmClient.getLastEvent(cur)) {
    web.setAlarmReachable(false);
    web.setLastMessage(alarmClient.lastError());
    return;
  }

  web.setAlarmReachable(true);
  web.setLastEvent(cur, true);

  if (!haveLastEntry) {
    lastEntry = cur;
    haveLastEntry = true;
    Serial.print(F("[ALARME] Etat initial : "));
    Serial.println(cur.signature);
    return;
  }

  if (cur.signature == lastEntry.signature) return;

  Serial.print(F("[ALARME] Changement : "));
  Serial.println(cur.signature);
  lastEntry = cur;
  setLed(true);
  notifyEvent(cur);
}

static void handleWebActions() {
  if (web.consumeLedToggleRequest()) setLed(!ledLatched);
  if (web.consumeTestRequest()) testAlarmNow(true);

  if (web.consumeSearchRequest()) {
    IPAddress found;
    if (alarmClient.discoverAlarm(config.data().searchPrefix, found)) {
      strlcpy(config.data().alarmIp, found.toString().c_str(),
              sizeof(config.data().alarmIp));
      config.data().setupCompleted = true;
      config.save();
      web.setLastMessage(String(F("Serveur HTTP trouve : ")) +
                         found.toString() + F(". Tester les identifiants."));
    } else {
      web.setLastMessage(alarmClient.lastError());
    }
  }
}

static void handleSerialCommands() {
  if (!Serial.available()) return;

  String cmd = Serial.readStringUntil('\n');
  cmd.trim();

  if (cmd.length() == 0) return;

  Serial.print(F("[CONSOLE] Commande : "));
  Serial.println(cmd);

  // --------------------------------------------------
  // Aide
  // --------------------------------------------------
  if (cmd.equalsIgnoreCase("HELP")) {
    Serial.println();
    Serial.println(F("=== COMMANDES DE MAINTENANCE ==="));
    Serial.println(F("HELP      : affiche cette aide"));
    Serial.println(F("AT        : teste la communication avec le modem"));
    Serial.println(F("STATUS    : affiche quelques informations modem"));
    Serial.println(F("PURGESMS  : supprime tous les SMS stockes"));
    Serial.println();
    return;
  }

  // --------------------------------------------------
  // Test modem
  // --------------------------------------------------
  if (cmd.equalsIgnoreCase("AT")) {
    Serial.println(F("[MODEM] Test AT..."));

    modemSerial.print("AT\r");

    uint32_t t0 = millis();

    while (millis() - t0 < 2000) {
      while (modemSerial.available()) {
        Serial.write(modemSerial.read());
      }
      yield();
    }

    Serial.println();
    return;
  }

  // --------------------------------------------------
  // Etat modem
  // --------------------------------------------------
  if (cmd.equalsIgnoreCase("STATUS")) {
    Serial.println(F("[MODEM] Etat du modem"));

    const char *commands[] = {
      "AT+CPIN?",
      "AT+CREG?",
      "AT+CSQ",
      "AT+CPMS?"
    };

    for (const char *at : commands) {
      Serial.print(F("> "));
      Serial.println(at);

      modemSerial.print(at);
      modemSerial.print('\r');

      uint32_t t0 = millis();

      while (millis() - t0 < 1000) {
        while (modemSerial.available()) {
          Serial.write(modemSerial.read());
        }
        yield();
      }

      Serial.println();
    }

    return;
  }

  // --------------------------------------------------
  // Purge SMS
  // --------------------------------------------------
  if (cmd.equalsIgnoreCase("PURGESMS")) {
    Serial.println(F("[SMS] Purge de la banque SMS..."));

    if (modem.purgeSMS()) {
      Serial.println(F("[SMS] Banque SMS purgee avec succes"));
    } else {
      Serial.println(F("[SMS] ERREUR pendant la purge"));
    }

    return;
  }

  // --------------------------------------------------
  // Commande inconnue
  // --------------------------------------------------
  Serial.print(F("[CONSOLE] Commande inconnue : "));
  Serial.println(cmd);
  Serial.println(F("Tapez HELP pour afficher les commandes."));
}

void setup() {
  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, STATUS_LED_ACTIVE_LOW ? HIGH : LOW);

  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.print(F("AlarmGateway "));
  Serial.println(ALARM_GATEWAY_VERSION);
  Serial.println(F("ESP-12E + ENC28J60 - base V3"));

  if (!config.begin()) {
    Serial.println(F("[FS] LittleFS impossible"));
  }

  // Ethernet AVANT toute activation Wi-Fi, exactement dans l'esprit V3.
  bool ethOk = initEthernetV3();
  if (!ethOk) {
    Serial.println(F("[ETH] Initialisation echouee"));
  }

  // Le serveur HTTP est attache a lwIP et reste accessible sur Ethernet.
  web.begin();

  // Le Wi-Fi n'est active qu'une fois l'ENC28J60 entierement initialise.
  startWiFiAfterEthernet();
  ota.begin();

  //modemSerial.begin(MODEM_BAUD);
  modemSerial.begin(
    MODEM_BAUD,
    SWSERIAL_8N1,
    MODEM_RX_PIN,
    MODEM_TX_PIN,
    false,
    512,
    0
  );
  

  delay(200);
  if (modem.begin()) Serial.println(F("[MODEM] AT OK"));
  else Serial.println(F("[MODEM] non repondant"));
}

void loop() {
  web.loop();
  ota.loop();
  modem.loop();
  handleIncomingSMS();
  handleWebActions();
  handleSerialCommands();
  uint32_t now = millis();
  uint32_t interval = (uint32_t)config.data().pollSeconds * 1000UL;
  if (lastPollMs == 0 || (uint32_t)(now - lastPollMs) >= interval) {
    lastPollMs = now;
    if (strlen(config.data().alarmIp)) pollAlarm();
  }

  delay(1);
  yield();
}

/*
static void handleSerialCommands() {
  if (!Serial.available()) return;

  String cmd = Serial.readStringUntil('\n');
  cmd.trim();

  if (cmd.equalsIgnoreCase("PURGESMS")) {
    Serial.println(F("[SMS] Purge de la banque SMS..."));

    if (modem.purgeSMS()) {
      Serial.println(F("[SMS] Banque SMS purgee avec succes"));
    } else {
      Serial.println(F("[SMS] ERREUR pendant la purge"));
    }
  }
}
*/