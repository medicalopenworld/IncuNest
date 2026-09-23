
#ifndef _WIFI_OTA_H_
#define _WIFI_OTA_H_

#include <Arduino.h>

#include "main.h"

#include "config/transport_policy.h"

// Los valores viven en transport_policy.h, que es la tabla única GPRS/WiFi.
// Estos alias mantienen los nombres que ya usa el código.
#define WIFI_PUBLISH_INTERVAL TX_WIFI_PUBLISH_MS
#define WIFI_OTA_CHECK_INTERVAL TX_WIFI_OTA_CHECK_MS
#ifndef THINGSBOARD_RECONNECT_DELAY // GPRS.h define el mismo alias
#define THINGSBOARD_RECONNECT_DELAY TX_THINGSBOARD_RECONNECT_MS
#endif
// Arduino-ESP32 3.x WiFi association can take >10 s on some APs; retrying
// before the in-flight WiFi.begin() finishes corrupts STA state
// (ESP_ERR_WIFI_CONN + HANDSHAKE_TIMEOUT). Match Display_HMI interval.
#define WIFI_RECONNECT_INTERVAL TX_WIFI_RECONNECT_MS

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
  long lastPpgSnapshotAttempt = false;
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
