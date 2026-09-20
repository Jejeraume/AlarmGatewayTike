# Changelog

## 5.0.0-dev3
- Suppression complete de la synchronisation UserPhone.htm / AlarmEvent.htm.
- Telephones (4) et nombre de rappels configurables manuellement.
- 40 evenements configurables manuellement (CMS/Appel/SMS/Email).
- Export/import complet en fichier texte `.cfg`, incluant user/mot de passe centrale et Wi-Fi.
- Import transactionnel : validation avant remplacement de la configuration.
- Correction collision globale `alarm` -> `alarmClient`.
- Correction Web OTA ESP32/ESP8266 (`UPDATE_SIZE_UNKNOWN` / `ESP.getFreeSketchSpace()`).
- Suppression des `yield()` redondants apres `delay(1)` dans AlarmClient.
