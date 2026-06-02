
#ifndef _WIFI_OTA_H_
#define _WIFI_OTA_H_

#include <Arduino.h>

#include "main.h"

#define WIFI_PUBLISH_INTERVAL 5000        // milliseconds
#define WIFI_OTA_CHECK_INTERVAL 60000     // 1 minute in milliseconds
#define THINGSBOARD_RECONNECT_DELAY 30000 // 30 seconds
// Arduino-ESP32 3.x WiFi association can take >10 s on some APs; retrying
// before the in-flight WiFi.begin() finishes corrupts STA state
// (ESP_ERR_WIFI_CONN + HANDSHAKE_TIMEOUT). Match Display_HMI interval.
#define WIFI_RECONNECT_INTERVAL 30000     // 30 seconds

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
  long lastOTACheck = false;
  long lastReconnectAttempt = false;
  bool firstPublish = false;
  bool firstConfigPost = false;
  String device_token;
  long lastWifiReconnectAttempt = 0;
  uint8_t provision_retry_count = 0;
};

bool WIFIIsConnectedToServer();
bool WIFIIsConnected();
bool WIFICheckNewEvent();
void applyWifiCredentials(const char* ssid, const char* pass);

#endif // _WIFI_OTA_H_
