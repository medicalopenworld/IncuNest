
#ifndef _WIFI_OTA_H_
#define _WIFI_OTA_H_

#include <Arduino.h>

#include "main.h"

#define WIFI_PUBLISH_INTERVAL 5000  // milliseconds

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
};

bool WIFIIsConnectedToServer();
bool WIFIIsConnected();
bool WIFICheckNewEvent();
void WIFI_TB_Init();
void WifiOTAHandler(void);
void wifiInit(void);

#endif  // _WIFI_OTA_H_
