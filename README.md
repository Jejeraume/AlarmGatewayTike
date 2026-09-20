# AlarmGateway 5.0.0-dev3-V3dhcp

Cette branche repart de la **V3 ESP8266 + ENC28J60** qui avait ete validee sur le materiel.

## Ce qui est conserve de la V3

- `ENC28J60lwIP eth(GPIO5)` est un objet global.
- Wi-Fi coupe pendant l'initialisation ENC28J60.
- SPI materiel ESP8266 : SCK=14, MISO=12, MOSI=13.
- SPI 4 MHz, MODE0, MSBFIRST.
- `eth.setDefault()` puis `eth.begin(mac)`.
- Serveur HTTP ESP8266/lwIP.
- Lecture de `SystemLog.htm` et parsing de la ligne 1.

Sur l'ESP8266 temporaire, l'ENC28J60 utilise le **DHCP exactement comme la V3 fonctionnelle**. Les champs Ethernet restent presents dans la configuration pour la future cible WT32-ETH01, mais ne sont pas appliques sur l'ESP8266.

## Modifications dev3 conservees

- Suppression totale du bouton et de la logique **Recuperer les parametres sur la centrale**.
- Aucun appel a `getPhoneNumbers()`.
- Aucun appel a `synchronizeCentral()`.
- Aucun acces a `UserPhone.htm` ou `AlarmEvent.htm`.
- `SystemLog.htm` est la seule lecture periodique de la centrale.
- Commandes `RemoteCtr.htm` : armement total, maison, desarmement, annulation et bypass.
- Test de connexion / identifiants.
- Recherche initiale optionnelle de la centrale.
- 4 numeros de telephone saisis localement.
- Nombre de rappels saisi localement.
- 40 evenements configures localement (CMS / appel / SMS / email).
- Table des commandes SMS locale.
- Export/import `AlarmGateway.cfg`.
- Le `.cfg` contient en clair : Wi-Fi, IP/utilisateur/mot de passe centrale, mot de passe SMS, telephones, rappels, 40 evenements et commandes SMS.
- Web OTA et Arduino OTA Wi-Fi.

## Ordre de demarrage

1. Chargement LittleFS.
2. Wi-Fi OFF.
3. SPI + ENC28J60 selon la V3.
4. Demarrage serveur Web.
5. Activation du Wi-Fi, si un SSID est configure.
6. Activation OTA.
7. Initialisation modem.

## Fichiers

- `AlarmGateway_v5.ino` : orchestration + initialisation Ethernet V3.
- `ConfigStore.*` : configuration locale LittleFS + import/export `.cfg`.
- `AlarmClient.*` : `SystemLog.htm`, `RemoteCtr.htm`, test/recherche.
- `WebInterface.*` : configuration Web, evenements, commandes SMS, import/export, Web OTA.
- `ModemAT.*` : SMS/appels.
- `SmsCommands.*` : commandes Android `#PWD...`.
- `OtaManager.*` : Arduino OTA Wi-Fi.
- `AlarmTypes.*` : structures communes.

Cible de cette reconstruction : **ESP-12E / ESP8266 + ENC28J60**. La branche WT32-ETH01 pourra etre reappliquee apres validation de cette base.
