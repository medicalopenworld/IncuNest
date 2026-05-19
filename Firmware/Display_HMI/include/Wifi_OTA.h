
#ifndef _WIFI_OTA_H_
#define _WIFI_OTA_H_

#include <string>

#define THINGSBOARD_ENABLE_PSRAM 0
#define THINGSBOARD_ENABLE_DYNAMIC 1

#ifdef USE_IDF_FRAMEWORK
#ifndef PROGMEM
#define PROGMEM
#endif
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "esp_ota_ops.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "lwip/inet.h"
#include <Espressif_MQTT_Client.h>
#include <ThingsBoard.h>
extern httpd_handle_t wifiServer;
#else
#include <Arduino.h>
#include <Arduino_MQTT_Client.h>
#include <ESPmDNS.h>
#include <Espressif_Updater.h>
#include <ThingsBoard.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>
#endif

#include "main.h"

#define CURRENT_FIRMWARE_TITLE "IncuNest_HMI"

#define WIFI_PUBLISH_INTERVAL 5000 // milliseconds
// Arduino-ESP32 3.x WiFi association can take >10 s on some APs; retrying
// wifiInit() before the in-flight WiFi.begin() finishes corrupts STA state
// (ESP_ERR_WIFI_CONN + HANDSHAKE_TIMEOUT) and has triggered IRQ-wdt panics.
#define WIFI_RECONNECT_INTERVAL 30000     // 30 seconds
#define THINGSBOARD_RECONNECT_DELAY 30000 // 30 seconds
#define WIFI_OTA_CHECK_INTERVAL 60000     // 1 minute

#define ENABLE_WIFI_OTA true // enable wifi OTA
#define ENABLE_GPRS_OTA true // enable GPRS OTA
#define THINGSBOARD_BUFFER_SIZE 4096
#define THINGSBOARD_FIELDS_AMOUNT 64
#define MAX_MESSAGE_SIZE 1024
#define THINGSBOARD_QOS false
#define TELEMETRIES_DECIMALS 2
#define FIRMWARE_FAILURE_RETRIES 12
#define FIRMWARE_PACKET_SIZE 4096
#define WAIT_FAILED_OTA_CHUNKS 10U * 1000U * 1000U

struct WIFIstruct {
  int provisioned = false;
  bool provision_request_sent = false;
  bool provision_request_processed = false;
  bool serverConnectionStatus = false;
  std::string device_token;
  long lastReconnectAttempt = 0;
  long lastMQTTPublish = 0;
  long lastOTACheck = 0;
};

bool WIFIIsConnectedToServer();
bool WIFIIsConnected();
void WIFI_TB_Init();
void WifiOTAHandler(void);
void WIFI_TB_OTA();
void wifiInit(void);
void CreateOTATask();

void progressCallback(const uint32_t &currentChunk,
                      const uint32_t &totalChuncks);
void updatedCallback(const bool &success);

#define CREDENTIALS_TYPE "credentialsType"
#define CREDENTIALS_VALUE "credentialsValue"
#define CLIENT_ID "clientId"
#define CLIENT_PASSWORD "password"
#define CLIENT_USERNAME "userName"
#define FW_STATE_UPDATED "UPDATED"
constexpr char ACCESS_TOKEN_CRED_TYPE[] PROGMEM = "ACCESS_TOKEN";
constexpr char MQTT_BASIC_CRED_TYPE[] PROGMEM = "MQTT_BASIC";

// Struct for client connecting after provisioning
struct Credentials {
  std::string client_id;
  std::string username;
  std::string password;
};

#endif // _WIFI_OTA_H_
