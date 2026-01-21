
#ifndef _WIFI_OTA_H_
#define _WIFI_OTA_H_

#include <Arduino.h>
#include <string>

#define THINGSBOARD_ENABLE_PSRAM 0
#define THINGSBOARD_ENABLE_DYNAMIC 1
#include <Arduino_MQTT_Client.h>
#include <ESPmDNS.h>
#include <Espressif_Updater.h>
#include <ThingsBoard.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>

#include "main.h"

#define CURRENT_FIRMWARE_TITLE "IncuNest_HMI"
#define CURRENT_FIRMWARE_VERSION "1.0.0"

#define WIFI_PUBLISH_INTERVAL 5000    // milliseconds
#define WIFI_RECONNECT_INTERVAL 10000 // 10 seconds

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
  bool OTA_requested = false;
  bool provision_request_sent = false;
  bool provision_request_processed = false;
  bool serverConnectionStatus = false;
  bool lastServerConnectionStatus = false;
  bool lastWIFIConnectionStatus = false;
  bool lastOTAInProgress = false;
  long lastMQTTPublish = false;
  bool firstPublish = false;
  bool firstConfigPost = false;
  String device_token;
  long lastWifiReconnectAttempt = 0;
};

bool WIFIIsConnectedToServer();
bool WIFIIsConnected();
bool WIFICheckNewEvent();
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
