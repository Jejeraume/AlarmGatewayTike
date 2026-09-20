#pragma once
#include <Arduino.h>

#define ALARM_GATEWAY_VERSION "5.0.0-dev3-V3sms1"
#define ALARM_GATEWAY_HOSTNAME "alarmgateway"

#if !defined(ESP8266)
  #error "Cette reconstruction V3base est destinee a l'ESP-12E / ESP8266 + ENC28J60."
#endif

#define BOARD_FRIENDLY_NAME "ESP-12E + ENC28J60 (base V3)"
#define ENC28J60_CS_PIN 5

#define MODEM_RX_PIN 4   // GPIO4
#define MODEM_TX_PIN 0   // GPIO0
#define MODEM_BAUD 115200

#ifndef LED_BUILTIN
  #define LED_BUILTIN 2
#endif
#define STATUS_LED_PIN LED_BUILTIN
#define STATUS_LED_ACTIVE_LOW 1

#define DEFAULT_POLL_SECONDS 10
#define MIN_POLL_SECONDS 1
#define MAX_POLL_SECONDS 3600

#define DEFAULT_ETH_LOCAL_IP "192.168.0.254"
#define DEFAULT_ETH_NETMASK  "255.255.255.0"
#define DEFAULT_SEARCH_PREFIX "192.168.0"
