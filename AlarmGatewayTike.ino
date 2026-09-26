/*
 * AlarmGatewayTike 6.0.0-dev1
 * ===========================
 * Cible : WT32-ETH01 / ESP32 + LAN8720
 *
 * - Ethernet natif ESP32 / LAN8720
 * - Wi-Fi optionnel
 * - Modem A7670E sur UART2 materiel
 * - WebUpdate gere par WebInterface
 * - Configuration locale LittleFS
 * - SystemLog.htm = lecture periodique
 * - RemoteCtr.htm = commandes centrale
 */

#include <Arduino.h>
#include <WiFi.h>
#include <ETH.h>

#include "BuildConfig.h"
#include "ConfigStore.h"
#include "AlarmClient.h"
#include "AlarmTypes.h"
#include "ModemAT.h"
#include "SmsCommands.h"
#include "WebInterface.h"

// -----------------------------------------------------------------------------
// Application
// -----------------------------------------------------------------------------

ConfigStore config;
AlarmClient alarmClient(config);

HardwareSerial modemSerial(MODEM_UART_NUM);
ModemAT modem(modemSerial);

SmsCommands smsCommands(config, alarmClient);
WebInterface web(config, alarmClient);

AlarmEntry lastEntry;

bool haveLastEntry = false;

uint32_t lastPollMs = 0;

// -----------------------------------------------------------------------------
// Message SMS
// -----------------------------------------------------------------------------

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

// -----------------------------------------------------------------------------
// Ethernet WT32-ETH01 / LAN8720
// -----------------------------------------------------------------------------

static bool initEthernet() {
  Serial.println(F("[ETH] Initialisation LAN8720..."));

  /*
   * WT32-ETH01 :
   *
   * PHY       : LAN8720
   * PHY addr  : 1
   * MDC       : GPIO23
   * MDIO      : GPIO18
   * POWER     : GPIO16
   * REF_CLK   : GPIO0 en entree
   */

  bool ok = ETH.begin(
    ETH_PHY_LAN8720,
    1,
    23,
    18,
    16,
    ETH_CLOCK_GPIO0_IN
  );

  if (!ok) {
    Serial.println(F("[ETH] ERREUR initialisation LAN8720"));
    return false;
  }

  ETH.setHostname(ALARM_GATEWAY_HOSTNAME);

  Serial.print(F("[ETH] Attente liaison"));

  uint32_t start = millis();

  while (!ETH.linkUp() && millis() - start < 10000UL) {
    Serial.print('.');
    delay(250);
  }

  Serial.println();

  if (!ETH.linkUp()) {
    Serial.println(F("[ETH] Aucun lien Ethernet"));
    return false;
  }

  // --------------------------------------------------
  // Attente DHCP
  // --------------------------------------------------

  Serial.print(F("[ETH] Attente DHCP"));

  start = millis();

  while (ETH.localIP() == IPAddress(0, 0, 0, 0) &&
         millis() - start < 10000UL) {
    Serial.print('.');
    delay(250);
  }

  Serial.println();

  if (ETH.localIP() == IPAddress(0, 0, 0, 0)) {
    Serial.println(F("[ETH] DHCP non obtenu"));
    return false;
  }

  Serial.println(F("[ETH] DHCP obtenu"));

  Serial.print(F("[ETH] IP      : "));
  Serial.println(ETH.localIP());

  Serial.print(F("[ETH] Masque  : "));
  Serial.println(ETH.subnetMask());

  Serial.print(F("[ETH] Gateway : "));
  Serial.println(ETH.gatewayIP());

  return true;
}

// -----------------------------------------------------------------------------
// Wi-Fi optionnel
// -----------------------------------------------------------------------------

static void startWiFi() {
  const auto &c = config.data();

  if (!strlen(c.wifiSsid)) {
    Serial.println(F("[WIFI] Aucun SSID configure"));
    return;
  }

  Serial.print(F("[WIFI] Connexion a "));
  Serial.println(c.wifiSsid);

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);

  WiFi.begin(c.wifiSsid, c.wifiPassword);

  uint32_t t0 = millis();

  while (WiFi.status() != WL_CONNECTED &&
         millis() - t0 < 12000UL) {
    delay(100);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print(F("[WIFI] Connecte : "));
    Serial.println(WiFi.localIP());
  } else {
    Serial.println(
      F("[WIFI] Connexion impossible - Ethernet reste actif")
    );
  }
}

// -----------------------------------------------------------------------------
// Test centrale
// -----------------------------------------------------------------------------

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

  if (verbose) {
    web.setLastMessage(
      F("Test reussi : communication et identifiants valides")
    );
  }

  return true;
}

// -----------------------------------------------------------------------------
// Notification Home Assistant
// -----------------------------------------------------------------------------

static bool sendHomeAssistantEvent(const AlarmEntry &e) {
  const auto &cfg = config.data();

  if (!cfg.homeAssistantIp[0] || !cfg.homeAssistantToken[0] || cfg.homeAssistantPort == 0) {
    Serial.println(F("[HA] Configuration incomplete"));
    return false;
  }

  WiFiClient client;

  Serial.print(F("[HA] Connexion a "));
  Serial.print(cfg.homeAssistantIp);
  Serial.print(':');
  Serial.println(cfg.homeAssistantPort);

  if (!client.connect(cfg.homeAssistantIp, cfg.homeAssistantPort)) {
    Serial.println(F("[HA] Connexion impossible"));
    return false;
  }

  String json;
  json.reserve(256);

  json += F("{\"code\":\"");
  json += e.code;
  json += F("\",\"event\":\"");
  json += e.state;
  json += F("\",\"date\":\"");
  json += e.date;
  json += F("\"}");

  client.print(F("POST /api/events/alarmgateway_event HTTP/1.1\r\n"));

  client.print(F("Host: "));
  client.print(cfg.homeAssistantIp);
  client.print(F("\r\n"));

  client.print(F("Authorization: Bearer "));
  client.print(cfg.homeAssistantToken);
  client.print(F("\r\n"));

  client.print(F("Content-Type: application/json\r\n"));

  client.print(F("Content-Length: "));
  client.print(json.length());
  client.print(F("\r\n"));

  client.print(F("Connection: close\r\n\r\n"));

  client.print(json);

  uint32_t start = millis();

  while (!client.available() &&
         client.connected() &&
         millis() - start < 3000) {
    delay(10);
  }

  if (!client.available()) {
    Serial.println(F("[HA] Pas de reponse"));
    client.stop();
    return false;
  }

  String statusLine =
    client.readStringUntil('\n');

  statusLine.trim();

  Serial.print(F("[HA] "));
  Serial.println(statusLine);

  bool ok =
    statusLine.indexOf(" 200 ") >= 0;

  client.stop();

  return ok;
}

// -----------------------------------------------------------------------------
// Notification evenement
// -----------------------------------------------------------------------------

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

  bool sendSms =
    eventCfg ? eventCfg->sms : true;

  bool makeCall =
    eventCfg ? eventCfg->voice : e.isIntrusion();

  bool sendHA =
    eventCfg ? eventCfg->HomeAssistant : false;

  // Aucune notification demandee
  if (!sendSms && !makeCall && !sendHA)
    return;

  // ---------------------------------------------------------------------------
  // SMS / appels
  // ---------------------------------------------------------------------------

  if (sendSms || makeCall) {
    PhoneList phones;

    for (uint8_t i = 0; i < 4; ++i)
      phones.phone[i] = snap.phones[i];

    phones.dialCount = snap.dialCount;

    if (phones.countValid() == 0) {
      web.setLastMessage(
        F("Evenement detecte mais aucun telephone configure")
      );
    }
    else {
      if (sendSms &&
          !modem.sendSMS(
            phones,
            buildSmsMessage(e)
          )) {

        web.setLastMessage(
          F("Au moins un SMS a echoue")
        );
      }

      if (makeCall &&
          !modem.callWithRetries(
            phones,
            phones.dialCount,
            20000
          )) {

        web.setLastMessage(
          F("Appels echoues")
        );
      }
    }
  }

  // ---------------------------------------------------------------------------
  // Home Assistant
  // ---------------------------------------------------------------------------

  if (sendHA) {
    Serial.println(
      F("[HA] Envoi notification")
    );

    if (sendHomeAssistantEvent(e)) {
      Serial.println(
        F("[HA] Notification envoyee")
      );
    }
    else {
      Serial.println(
        F("[HA] Echec notification")
      );
    }
  }
}

// -----------------------------------------------------------------------------
// Verification expediteur SMS
// -----------------------------------------------------------------------------

static bool isConfiguredPhone(const String &sender) {
  String n = normalizeFrenchPhone(sender);

  const auto &snap = config.snapshot();

  for (uint8_t i = 0; i < 4; ++i) {
    if (!snap.phones[i][0])
      continue;

    if (normalizeFrenchPhone(
          String(snap.phones[i])) == n) {
      return true;
    }
  }

  return false;
}

// -----------------------------------------------------------------------------
// Reception SMS
// -----------------------------------------------------------------------------

static void handleIncomingSMS() {
  String sender;
  String text;

  if (!modem.takeReceivedSMS(sender, text))
    return;

  Serial.print(F("[SMS RX] "));
  Serial.print(sender);
  Serial.print(F(" : "));
  Serial.println(text);

  if (!isConfiguredPhone(sender)) {
    Serial.println(
      F("[SMS RX] Expediteur non autorise")
    );
    return;
  }

  SmsAction action;

  String commandName;
  String error;
  String reply;

  if (!smsCommands.parse(
        text,
        action,
        commandName,
        error)) {

    Serial.print(F("[SMS RX] Ignore : "));
    Serial.println(error);

    return;
  }

  bool ok =
    smsCommands.execute(action, reply);

  if (!reply.length()) {
    reply = ok
      ? F("Commande executee")
      : F("Commande en echec");
  }

  modem.sendSMS(sender, reply);
}

// -----------------------------------------------------------------------------
// Lecture etat centrale
// -----------------------------------------------------------------------------

static void pollAlarm() {
  AlarmEntry cur;

  if (!alarmClient.getLastEvent(cur)) {
    web.setAlarmReachable(false);
    web.setLastMessage(
      alarmClient.lastError()
    );
    return;
  }

  web.setAlarmReachable(true);
  web.setLastEvent(cur, true);

  if (!haveLastEntry) {
    lastEntry = cur;
    haveLastEntry = true;

    Serial.print(
      F("[ALARME] Etat initial : ")
    );

    Serial.println(cur.signature);

    return;
  }

  if (cur.signature == lastEntry.signature)
    return;

  Serial.print(
    F("[ALARME] Changement : ")
  );

  Serial.println(cur.signature);

  lastEntry = cur;

  notifyEvent(cur);
}

// -----------------------------------------------------------------------------
// Actions interface Web
// -----------------------------------------------------------------------------

static void handleWebActions() {

  if (web.consumeTestRequest())
    testAlarmNow(true);

  if (web.consumeSearchRequest()) {
    IPAddress found;

    if (alarmClient.discoverAlarm(
          config.data().searchPrefix,
          found)) {

      strlcpy(
        config.data().alarmIp,
        found.toString().c_str(),
        sizeof(config.data().alarmIp)
      );

      config.data().setupCompleted = true;

      config.save();

      web.setLastMessage(
        String(F("Serveur HTTP trouve : ")) +
        found.toString() +
        F(". Tester les identifiants.")
      );

    } else {

      web.setLastMessage(
        alarmClient.lastError()
      );
    }
  }
}

// -----------------------------------------------------------------------------
// Console serie
// -----------------------------------------------------------------------------

static void handleSerialCommands() {
  if (!Serial.available())
    return;

  String cmd =
    Serial.readStringUntil('\n');

  cmd.trim();

  if (cmd.length() == 0)
    return;

  Serial.print(F("[CONSOLE] Commande : "));
  Serial.println(cmd);

  // --------------------------------------------------
  // HELP
  // --------------------------------------------------

  if (cmd.equalsIgnoreCase("HELP")) {
    Serial.println();
    Serial.println(F("=== COMMANDES DE MAINTENANCE ==="));

    Serial.println(F("HELP      : affiche cette aide"));

    Serial.println(F("AT        : teste la communication avec le modem"));

    Serial.println(F("STATUS    : affiche quelques informations modem"));

    Serial.println(F("PURGESMS  : supprime tous les SMS stockes"));

	Serial.println(F("TESTHA    : teste la notification Home Assistant"));
	Serial.println(F("TESTSMS   : envoie un SMS de test"));
	Serial.println(F("TESTCALL  : appelle le premier numero configure"));
    Serial.println();

    return;
  }

  // --------------------------------------------------
  // AT
  // --------------------------------------------------

  if (cmd.equalsIgnoreCase("AT")) {
    Serial.println(F("[MODEM] Test AT..."));

    modemSerial.print("AT\r");

    uint32_t t0 = millis();

    while (millis() - t0 < 2000) {
      while (modemSerial.available()) {
        Serial.write(
          modemSerial.read()
        );
      }

      delay(1);
    }

    Serial.println();

    return;
  }

  // --------------------------------------------------
  // STATUS
  // --------------------------------------------------

  if (cmd.equalsIgnoreCase("STATUS")) {
    Serial.println(
      F("[MODEM] Etat du modem")
    );

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
          Serial.write(
            modemSerial.read()
          );
        }

        delay(1);
      }

      Serial.println();
    }

    return;
  }

  // --------------------------------------------------
  // PURGESMS
  // --------------------------------------------------

  if (cmd.equalsIgnoreCase("PURGESMS")) {
    Serial.println(
      F("[SMS] Purge de la banque SMS...")
    );

    if (modem.purgeSMS()) {
      Serial.println(
        F("[SMS] Banque SMS purgee avec succes")
      );
    } else {
      Serial.println(
        F("[SMS] ERREUR pendant la purge")
      );
    }

    return;
  }


	// --------------------------------------------------
	// TESTHA
	// --------------------------------------------------

	if (cmd.equalsIgnoreCase("TESTHA")) {
		Serial.println(F("[TEST] Home Assistant"));
		AlarmEntry e;
		e.code = "TEST";
		e.state = "Test AlarmGateway";
		e.date = "Test manuel";
		if (sendHomeAssistantEvent(e))
			Serial.println(F("[TEST] Home Assistant OK"));
		else
			Serial.println(F("[TEST] Home Assistant ECHEC"));
		return;
	}


	// --------------------------------------------------
	// TESTSMS
	// --------------------------------------------------

	if (cmd.equalsIgnoreCase("TESTSMS")) {
		Serial.println(F("[TEST] SMS"));
		const auto &snap = config.snapshot();
		PhoneList phones;
		for (uint8_t i = 0; i < 4; ++i) phones.phone[i] = "";

	// Premier numero configure uniquement
		for (uint8_t i = 0; i < 4; ++i) {
			if (snap.phones[i][0]) {
				phones.phone[0] = snap.phones[i];
			break;
			}
		}

		if (phones.countValid() == 0) {
			Serial.println(F("[TEST] Aucun telephone configure"));
			return;
		}

		if (modem.sendSMS(phones, "Test SMS AlarmGateway")) {
			Serial.println(F("[TEST] SMS OK"));
		}
		else {
			Serial.println(F("[TEST] SMS ECHEC"));
		}
	return;
	}


	// --------------------------------------------------
	// TESTCALL
	// --------------------------------------------------

	if (cmd.equalsIgnoreCase("TESTCALL")) {
		Serial.println(F("[TEST] Appel"));
		const auto &snap = config.snapshot();
		String number;
		// Premier numero configure
		for (uint8_t i = 0; i < 4; ++i) {
			if (snap.phones[i][0]) {
				number = snap.phones[i];
				break;
			}
		}
		if (!number.length()) {
			Serial.println(F("[TEST] Aucun telephone configure"));
			return;
		}
		Serial.print(F("[TEST] Appel vers "));
		Serial.println(number);
		if (modem.call(number, 20000)) {
			Serial.println(F("[TEST] Appel termine"));
		}
		else {
			Serial.println(F("[TEST] Appel ECHEC"));
		}
		return;
	}
  Serial.print(F("[CONSOLE] Commande inconnue : "));
  Serial.println(cmd);
  Serial.println( F("Tapez HELP pour afficher les commandes."));
}

// -----------------------------------------------------------------------------
// SETUP
// -----------------------------------------------------------------------------

void setup() {


  // --------------------------------------------------
  // Console
  // --------------------------------------------------

  Serial.begin(115200);

  delay(300);

  Serial.println();

  Serial.print(F("AlarmGatewayTike "));
  Serial.println(
    ALARM_GATEWAY_VERSION
  );

  Serial.println(
    F("WT32-ETH01 / ESP32 + LAN8720")
  );

  // --------------------------------------------------
  // Configuration
  // --------------------------------------------------

  if (!config.begin()) {
    Serial.println(
      F("[FS] LittleFS impossible")
    );
  }

  // --------------------------------------------------
  // Ethernet
  // --------------------------------------------------

  if (!initEthernet()) {
    Serial.println(
      F("[ETH] Initialisation echouee")
    );
  }

  // --------------------------------------------------
  // Interface Web
  // --------------------------------------------------

  web.begin();

  // --------------------------------------------------
  // Wi-Fi optionnel
  // --------------------------------------------------

  startWiFi();

  // --------------------------------------------------
  // UART modem
  // --------------------------------------------------

  Serial.print(
    F("[MODEM] UART")
  );

  Serial.print(
    MODEM_UART_NUM
  );

  Serial.print(
    F(" RX=GPIO")
  );

  Serial.print(
    MODEM_RX_PIN
  );

  Serial.print(
    F(" TX=GPIO")
  );

  Serial.println(
    MODEM_TX_PIN
  );

  modemSerial.begin(
    MODEM_BAUD,
    SERIAL_8N1,
    MODEM_RX_PIN,
    MODEM_TX_PIN
  );

  delay(200);

  if (modem.begin()) {
    Serial.println(
      F("[MODEM] AT OK")
    );
  } else {
    Serial.println(
      F("[MODEM] non repondant")
    );
  }

}

// -----------------------------------------------------------------------------
// LOOP
// -----------------------------------------------------------------------------

void loop() {
  web.loop();

  modem.loop();

  handleIncomingSMS();

  handleWebActions();

  handleSerialCommands();

  uint32_t now = millis();

  uint32_t interval =
    (uint32_t)config.data().pollSeconds *
    1000UL;

  if (lastPollMs == 0 ||
      (uint32_t)(now - lastPollMs) >= interval) {

    lastPollMs = now;

    if (strlen(config.data().alarmIp))
      pollAlarm();
  }

  delay(1);
}