#pragma once
#include <Arduino.h>

// -----------------------------------------------------------------------------
// AlarmGatewayTike - configuration materielle
// Cible : WT32-ETH01 / ESP32 + LAN8720
// -----------------------------------------------------------------------------

#define ALARM_GATEWAY_VERSION "6.0.0-dev1"
#define ALARM_GATEWAY_HOSTNAME "alarmgateway"

#if !defined(ESP32)
  #error "AlarmGatewayTike necessite un ESP32."
#endif

#define BOARD_FRIENDLY_NAME "WT32-ETH01 + LAN8720"

// -----------------------------------------------------------------------------
// Modem cellulaire
// ESP32 : utilisation d'un UART materiel
// -----------------------------------------------------------------------------

#define MODEM_UART_NUM 2
#define MODEM_BAUD 115200

// A definir suivant le cablage retenu sur le WT32-ETH01
// UART2 modem
// GPIO5 / GPIO17 reserves
// GPIO0 reserve au LAN8720 (RMII REF_CLK)
#define MODEM_RX_PIN 35
#define MODEM_TX_PIN 4

// -----------------------------------------------------------------------------
// LED d'etat
// -----------------------------------------------------------------------------

#define STATUS_LED_PIN 2
#define STATUS_LED_ACTIVE_LOW 1

// -----------------------------------------------------------------------------
// Centrale d'alarme
// -----------------------------------------------------------------------------

#define DEFAULT_POLL_SECONDS 10
#define MIN_POLL_SECONDS 1
#define MAX_POLL_SECONDS 3600

#define DEFAULT_ETH_LOCAL_IP "192.168.0.254"
#define DEFAULT_ETH_NETMASK  "255.255.255.0"
#define DEFAULT_SEARCH_PREFIX "192.168.0"