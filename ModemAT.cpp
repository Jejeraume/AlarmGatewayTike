#include "ModemAT.h"

// -----------------------------------------------------------------------------
// Trace UART modem
// -----------------------------------------------------------------------------

void ModemAT::flushInput() {
  if (!serial_.available()) return;

  Serial.print(F("[MODEM RX] "));
  while (serial_.available()) {
    char c = (char)serial_.read();
    Serial.write(c);
  }
  Serial.println();
}

bool ModemAT::waitFor(const String &token, uint32_t timeoutMs) {
  uint32_t t0 = millis();
  lastResponse_ = "";
  bool traceStarted = false;

  while ((uint32_t)(millis() - t0) < timeoutMs) {
    while (serial_.available()) {
      char c = (char)serial_.read();

      if (!traceStarted) {
        Serial.print(F("[MODEM RX] "));
        traceStarted = true;
      }

      Serial.write(c);
      lastResponse_ += c;

      if (lastResponse_.indexOf(token) >= 0) {
        if (traceStarted) Serial.println();
        return true;
      }

      if (lastResponse_.indexOf("ERROR") >= 0) {
        if (traceStarted) Serial.println();
        return false;
      }
    }

    delay(1);
    yield();
  }

  if (traceStarted) {
    Serial.println();
  } else {
    Serial.println(F("[MODEM RX] <timeout / aucune reponse>"));
  }

  return false;
}

bool ModemAT::command(const String &cmd,
                      const String &expected,
                      uint32_t timeoutMs) {
  flushInput();

  Serial.print(F("[MODEM TX] "));
  Serial.println(cmd);

  serial_.print(cmd);
  serial_.print("\r");

  return waitFor(expected, timeoutMs);
}

// -----------------------------------------------------------------------------
// Initialisation
// -----------------------------------------------------------------------------

bool ModemAT::begin() {
  if (!command("AT")) return false;

  command("ATE0");
  command("AT+CMEE=2");

  // SMS en mode texte, mais jeu de caractères UCS2.
  // C'est le mode utilisé par l'application / le modem observé.
  if (!command("AT+CMGF=1")) return false;
  if (!command("AT+CSCS=\"UCS2\"")) return false;

  // Nouveau SMS stocké => +CMTI: "SM",index
  if (!command("AT+CNMI=2,1,0,0,0")) return false;

  return true;
}

// -----------------------------------------------------------------------------
// UCS2 <-> UTF-8
// -----------------------------------------------------------------------------

int8_t ModemAT::hexValue(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}

bool ModemAT::looksLikeUcs2Hex(const String &text) {
  if (text.length() < 4 || (text.length() % 4) != 0) return false;

  for (size_t i = 0; i < text.length(); ++i) {
    if (hexValue(text[i]) < 0) return false;
  }

  return true;
}

void ModemAT::appendUtf8(String &out, uint16_t cp) {
  if (cp <= 0x7F) {
    out += (char)cp;
  } else if (cp <= 0x07FF) {
    out += (char)(0xC0 | ((cp >> 6) & 0x1F));
    out += (char)(0x80 | (cp & 0x3F));
  } else {
    out += (char)(0xE0 | ((cp >> 12) & 0x0F));
    out += (char)(0x80 | ((cp >> 6) & 0x3F));
    out += (char)(0x80 | (cp & 0x3F));
  }
}

String ModemAT::ucs2HexToUtf8(const String &hex) {
  String in = hex;
  in.trim();

  if (!looksLikeUcs2Hex(in)) return in;

  String out;
  out.reserve(in.length() / 2);

  for (size_t i = 0; i + 3 < in.length(); i += 4) {
    int8_t h0 = hexValue(in[i]);
    int8_t h1 = hexValue(in[i + 1]);
    int8_t h2 = hexValue(in[i + 2]);
    int8_t h3 = hexValue(in[i + 3]);

    if (h0 < 0 || h1 < 0 || h2 < 0 || h3 < 0) return in;

    uint16_t cp = ((uint16_t)h0 << 12) |
                  ((uint16_t)h1 << 8)  |
                  ((uint16_t)h2 << 4)  |
                  (uint16_t)h3;

    appendUtf8(out, cp);
  }

  return out;
}

String ModemAT::utf8ToUcs2Hex(const String &text) {
  static const char HEX_CHARS[] = "0123456789ABCDEF";

  String out;
  out.reserve(text.length() * 4);

  const uint8_t *p = (const uint8_t *)text.c_str();
  size_t len = text.length();
  size_t i = 0;

  while (i < len) {
    uint32_t cp;
    uint8_t c = p[i];

    if (c < 0x80) {
      cp = c;
      i += 1;
    } else if ((c & 0xE0) == 0xC0 && i + 1 < len &&
               (p[i + 1] & 0xC0) == 0x80) {
      cp = ((uint32_t)(c & 0x1F) << 6) |
           (uint32_t)(p[i + 1] & 0x3F);
      i += 2;
    } else if ((c & 0xF0) == 0xE0 && i + 2 < len &&
               (p[i + 1] & 0xC0) == 0x80 &&
               (p[i + 2] & 0xC0) == 0x80) {
      cp = ((uint32_t)(c & 0x0F) << 12) |
           ((uint32_t)(p[i + 1] & 0x3F) << 6) |
           (uint32_t)(p[i + 2] & 0x3F);
      i += 3;
    } else {
      // UCS2 ne sait pas représenter les caractères hors BMP.
      // On remplace également toute séquence UTF-8 invalide par '?'.
      cp = '?';
      i += 1;
    }

    if (cp > 0xFFFF) cp = '?';

    uint16_t u = (uint16_t)cp;
    out += HEX_CHARS[(u >> 12) & 0x0F];
    out += HEX_CHARS[(u >> 8) & 0x0F];
    out += HEX_CHARS[(u >> 4) & 0x0F];
    out += HEX_CHARS[u & 0x0F];
  }

  return out;
}

// -----------------------------------------------------------------------------
// Réception SMS
// -----------------------------------------------------------------------------

String ModemAT::extractQuotedField(const String &line, uint8_t fieldIndex) {
  int pos = 0;

  for (uint8_t i = 0; i <= fieldIndex; ++i) {
    int q1 = line.indexOf('"', pos);
    if (q1 < 0) return "";

    int q2 = line.indexOf('"', q1 + 1);
    if (q2 < 0) return "";

    if (i == fieldIndex) return line.substring(q1 + 1, q2);
    pos = q2 + 1;
  }

  return "";
}

bool ModemAT::readSMS(uint16_t index) {
  flushInput();

  Serial.print(F("[MODEM TX] AT+CMGR="));
  Serial.println(index);

  serial_.print("AT+CMGR=");
  serial_.print(index);
  serial_.print("\r");

  uint32_t t0 = millis();
  String response;
  bool traceStarted = false;

  while ((uint32_t)(millis() - t0) < 5000) {
    while (serial_.available()) {
      char c = (char)serial_.read();

      if (!traceStarted) {
        Serial.print(F("[MODEM RX] "));
        traceStarted = true;
      }

      Serial.write(c);
      response += c;

      if (response.length() > 4096) {
        if (traceStarted) Serial.println();
        lastResponse_ = response;
        return false;
      }

      if (response.indexOf("ERROR") >= 0) {
        if (traceStarted) Serial.println();
        lastResponse_ = response;
        return false;
      }

      if (response.indexOf("\r\nOK\r\n") >= 0 ||
          response.endsWith("\nOK\r\n")) {
        if (traceStarted) Serial.println();
        lastResponse_ = response;

        int headerPos = response.indexOf("+CMGR:");
        if (headerPos < 0) return false;

        int headerEnd = response.indexOf('\n', headerPos);
        if (headerEnd < 0) return false;

        String header = response.substring(headerPos, headerEnd);
        header.trim();

        // +CMGR: "REC UNREAD","<expediteur>",...
        String rawSender = extractQuotedField(header, 1);
        smsSender_ = ucs2HexToUtf8(rawSender);

        int bodyStart = headerEnd + 1;
        while (bodyStart < (int)response.length() &&
               (response[bodyStart] == '\r' || response[bodyStart] == '\n')) {
          ++bodyStart;
        }

        int bodyEnd = response.indexOf("\r\nOK", bodyStart);
        if (bodyEnd < 0) bodyEnd = response.indexOf("\nOK", bodyStart);
        if (bodyEnd < 0) return false;

        String rawBody = response.substring(bodyStart, bodyEnd);
        rawBody.trim();
        smsMessage_ = ucs2HexToUtf8(rawBody);

        Serial.print(F("[SMS DECODE] Expediteur : "));
        Serial.println(smsSender_);
        Serial.print(F("[SMS DECODE] Message    : "));
        Serial.println(smsMessage_);

        smsPending_ = smsSender_.length() && smsMessage_.length();
        return smsPending_;
      }
    }

    delay(1);
    yield();
  }

  if (traceStarted) {
    Serial.println();
  } else {
    Serial.println(F("[MODEM RX] <timeout / aucune reponse a CMGR>"));
  }

  lastResponse_ = response;
  return false;
}

void ModemAT::loop() {
  // Ne consomme les URC que lorsque le modem est au repos.
  bool traced = false;

  while (serial_.available()) {
    char c = (char)serial_.read();

    if (!traced) {
      Serial.print(F("[MODEM RX] "));
      traced = true;
    }

    Serial.write(c);
    urcBuffer_ += c;

    if (urcBuffer_.length() > 256) {
      urcBuffer_.remove(0, urcBuffer_.length() - 128);
    }

    int p = urcBuffer_.indexOf("+CMTI:");
    if (p < 0) continue;

    int e = urcBuffer_.indexOf('\n', p);
    if (e < 0) continue;

    String line = urcBuffer_.substring(p, e);
    line.trim();
    urcBuffer_.remove(0, e + 1);

    int comma = line.lastIndexOf(',');
    if (comma < 0) continue;

    int index = line.substring(comma + 1).toInt();
    if (index <= 0) continue;

    if (traced) {
      Serial.println();
      traced = false;
    }

    if (readSMS((uint16_t)index)) {
      // Effacement uniquement après lecture et décodage réussis.
      command(String("AT+CMGD=") + index);
	  // Laisser le modem terminer complètement le traitement SMS
		// avant qu'une éventuelle réponse soit envoyée.
		delay(300);
		yield();
		// Le SMS sera traité par la boucle principale.
		// Ne plus manipuler l'UART modem pendant ce passage.
		break;
    }
  }

  if (traced) Serial.println();
}

bool ModemAT::takeReceivedSMS(String &sender, String &message) {
  if (!smsPending_) return false;

  sender = smsSender_;
  message = smsMessage_;

  smsPending_ = false;
  smsSender_ = "";
  smsMessage_ = "";

  return true;
}

// -----------------------------------------------------------------------------
// Envoi SMS
// -----------------------------------------------------------------------------

bool ModemAT::sendSMS(const String &number, const String &message) {
  if (!number.length()) return false;

  // Passage en alphabet GSM pour l'émission
  if (!command("AT+CSCS=\"GSM\"")) return false;

  flushInput();

  Serial.print(F("[MODEM TX] AT+CMGS=\""));
  Serial.print(number);
  Serial.println(F("\""));

  serial_.print("AT+CMGS=\"");
  serial_.print(number);
  serial_.print("\"");
  serial_.write('\r');

  if (!waitFor(">", 5000)) {
    // Retour en UCS2 pour la réception
    command("AT+CSCS=\"UCS2\"");
    return false;
  }

  Serial.print(F("[MODEM TX] SMS texte : "));
  Serial.println(message);

for (size_t i = 0; i < message.length(); i++) {
  serial_.write((uint8_t)message[i]);
  delay(2);
  yield();
}

delay(100);
serial_.write(0x1A);

  bool ok = waitFor("+CMGS:", 15000);

  // Remettre immédiatement le modem en UCS2 pour les SMS entrants
  command("AT+CSCS=\"UCS2\"");

  return ok;
}

bool ModemAT::sendSMS(const PhoneList &phones, const String &message) {
  bool any = false;
  bool all = true;

  for (uint8_t i = 0; i < 4; ++i) {
    if (!phones.phone[i].length()) continue;

    any = true;
    if (!sendSMS(phones.phone[i], message)) all = false;
    delay(400);
  }

  return any && all;
}

// -----------------------------------------------------------------------------
// Appels - comportement existant conservé
// -----------------------------------------------------------------------------

bool ModemAT::call(const String &number, uint32_t ringTimeMs) {
  if (!number.length()) return false;

  flushInput();

  Serial.print(F("[MODEM TX] ATD"));
  Serial.print(number);
  Serial.println(';');

  serial_.print("ATD");
  serial_.print(number);
  serial_.print(";\r");

  if (!waitFor("OK", 5000)) return false;

  uint32_t t0 = millis();
  bool traced = false;

  while ((uint32_t)(millis() - t0) < ringTimeMs) {
    while (serial_.available()) {
      char c = (char)serial_.read();

      if (!traced) {
        Serial.print(F("[MODEM RX] "));
        traced = true;
      }

      Serial.write(c);
    }

    delay(10);
    yield();
  }

  if (traced) Serial.println();

  command("AT+CHUP");
  return true;
}

bool ModemAT::callWithRetries(const PhoneList &phones,
                              uint8_t rounds,
                              uint32_t ringTimeMs) {
  if (rounds < 1) rounds = 1;
  if (rounds > 15) rounds = 15;

  for (uint8_t r = 0; r < rounds; ++r) {
    for (uint8_t i = 0; i < 4; ++i) {
      if (!phones.phone[i].length()) continue;

      if (call(phones.phone[i], ringTimeMs)) return true;
      delay(500);
    }
  }

  return false;
}

bool ModemAT::purgeSMS() {
    return command("AT+CMGD=1,4");
}