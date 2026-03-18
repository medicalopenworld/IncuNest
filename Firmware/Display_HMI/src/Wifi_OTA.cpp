/*
  MIT License

  Copyright (c) 2022 Medical Open World, Pablo Sánchez Bergasa

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.

*/
#include <Arduino.h>
#include <string.h>

#include "esp_log.h"
#include "main.h"

static const char *TAG = "WiFi";

const char *wifiHost = "in3ator";

WebServer wifiServer(80);

WiFiClient espClient;

// Initalize the Mqtt client instance
Arduino_MQTT_Client mqttClientWIFI(espClient);

// Initialize ThingsBoard instance
ThingsBoard tb_wifi(mqttClientWIFI, MAX_MESSAGE_SIZE);
StaticJsonDocument<JSON_OBJECT_SIZE(THINGSBOARD_FIELDS_AMOUNT)> WIFI_JSON;
JsonObject addVariableToTelemetryWIFIJSON = WIFI_JSON.to<JsonObject>();

// WIFI
bool WIFI_connection_status = false;

// extern in3ator_parameters in3;
WIFIstruct Wifi_TB;
Credentials credentials;
Espressif_Updater updater_WIFI;

const OTA_Update_Callback OTAcallback(&progressCallback, &updatedCallback,
                                      CURRENT_FIRMWARE_TITLE, FWversion,
                                      &updater_WIFI, FIRMWARE_FAILURE_RETRIES,
                                      FIRMWARE_PACKET_SIZE,
                                      WAIT_FAILED_OTA_CHUNKS);

/*
   Login page
*/

const char *www_username = "in3admin";
const char *www_password = "savinglives";

/*
   wifiServer Index Page
*/

const char *serverIndex =
    "<script "
    "src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></"
    "script>"
    "<h3>Firmware Update</h3>"
    "<p>Current Version: <span id='fw_version'></span></p>"
    "<form method='POST' action='#' enctype='multipart/form-data' "
    "id='upload_form'>"
    "<input type='file' name='update'>"
    "<input type='submit' value='Update'>"
    "</form>"
    "<div id='prg'>progress: 0%</div>"
    "<script>"
    "$(document).ready(function() {"
    "  $.get('/get_fw_version', function(data) {"
    "    $('#fw_version').text(data.version);"
    "  });"
    "});"
    "</script>"
    "<script>"
    "$('form').submit(function(e){"
    "e.preventDefault();"
    "var form = $('#upload_form')[0];"
    "var data = new FormData(form);"
    " $.ajax({"
    "url: '/update',"
    "type: 'POST',"
    "data: data,"
    "contentType: false,"
    "processData:false,"
    "xhr: function() {"
    "var xhr = new window.XMLHttpRequest();"
    "xhr.upload.addEventListener('progress', function(evt) {"
    "if (evt.lengthComputable) {"
    "var per = evt.loaded / evt.total;"
    "$('#prg').html('progress: ' + Math.round(per*100) + '%');"
    "}"
    "}, false);"
    "return xhr;"
    "},"
    "success:function(d, s) {"
    "console.log('success!')"
    "},"
    "error: function (a, b, c) {"
    "}"
    "});"
    "});"
    "</script>";

char pendingSSID[64] = "";
char pendingPass[64] = "";
static uint32_t lastconnectiontrywifi = 0;

// ---------------------------------------------------------------------------
// WiFi Network Scan — state machine
//
// CRITICAL: Never call WiFi.mode() or disconnect(true) during scan flow.
// Those functions call esp_wifi_deinit() which destroys the WiFi stack.
// Calling esp_wifi_init() again on a fragmented heap causes:
//   esp_wifi_init 257 (ESP_ERR_NO_MEM), "Expected to init 4 rx buffer, actual is 0"
//
// Correct approach — keep the WiFi stack alive at all times:
//   1. setAutoReconnect(false) FIRST — stops Arduino event handler from
//      calling esp_wifi_connect() when the disconnect event fires
//   2. disconnect(false) — stops current connection, stack stays alive
//   3. Wait 300ms — IDF settles into IDLE state (no queued reconnects)
//   4. scanNetworks(true, true) — async scan, no driver conflicts
//   5. After scan: setAutoReconnect(true) + WiFi.begin() to reconnect
//
// WifiOTAHandler is fully blocked while s_scanState != SCAN_IDLE.
// ---------------------------------------------------------------------------
volatile bool g_wifiScanRequest = false;

typedef enum {
  SCAN_IDLE,         // no scan in progress
  SCAN_DISCONNECTING,// setAutoReconnect(false)+disconnect(false) called, waiting
  SCAN_RUNNING,      // scanNetworks() active, polling scanComplete()
} ScanState_t;

static ScanState_t   s_scanState        = SCAN_IDLE;
static uint32_t      s_scanStateMs      = 0;
static volatile bool s_scanInProgress   = false;
static volatile bool s_scanResultsReady = false;
static volatile int  s_scanCount        = 0;

// When true, WifiOTAHandler will NOT call wifiInit() in its reconnect loop.
// Set after every scan so that the IDF stays idle until the user explicitly
// hits the Connect button (which calls wifiInit() → clears this flag).
static bool s_suppressReconnect = false;

#define SCAN_DISCONNECT_WAIT_MS 300  // IDF settle time after disconnect(false)
#define SCAN_TIMEOUT_MS         12000// abort if scan hangs this long

// Called from OTA task at the end of every scan path.
// Does NOT call wifiInit()/WiFi.begin() — reconnect is suppressed until the
// user explicitly requests a connection via WifiConnectButton_cb → wifiInit().
static void scanFinalize(int networkCount) {
  s_scanInProgress    = false;
  s_scanState         = SCAN_IDLE;
  s_scanCount         = (networkCount >= 0) ? networkCount : 0;
  s_scanResultsReady  = true;
  s_suppressReconnect = true;  // block auto-reconnect until user connects
  WiFi.setAutoReconnect(true); // restore flag (harmless while disconnected)
  ESP_LOGI(TAG, "WifiScanHandler: scan done (%d nets). Waiting for user to connect.", s_scanCount);
}

// Called from UI task only — no direct WiFi calls here.
void WifiScanRequest(void) {
  s_scanResultsReady = false;
  s_scanCount        = 0;
  g_wifiScanRequest  = true;
  ESP_LOGI(TAG, "WifiScanRequest: requested");
}

// Called from OTA task every loop iteration.
void WifiScanHandler(void) {
  switch (s_scanState) {

    // -----------------------------------------------------------------------
    case SCAN_IDLE:
      if (!g_wifiScanRequest) return;
      g_wifiScanRequest  = false;
      s_scanResultsReady = false;
      s_scanCount        = 0;
      s_scanInProgress   = true;
      // Reset reconnect timer so the WifiOTAHandler loop cannot fire a
      // wifiInit()/WiFi.begin() call during our 300ms settle window.
      Wifi_TB.lastWifiReconnectAttempt = millis();
      ESP_LOGI(TAG, "WifiScanHandler: wl_status=%d — disabling auto-reconnect, disconnecting",
               (int)WiFi.status());
      // Order matters: disable auto-reconnect BEFORE disconnect so the IDF
      // event handler does NOT call esp_wifi_connect() when disconnect fires.
      WiFi.setAutoReconnect(false);
      WiFi.disconnect(false);  // keep WiFi stack alive, just stop connection
      s_scanState   = SCAN_DISCONNECTING;
      s_scanStateMs = millis();
      break;

    // -----------------------------------------------------------------------
    case SCAN_DISCONNECTING:
      if (millis() - s_scanStateMs < SCAN_DISCONNECT_WAIT_MS) return;
      {
        ESP_LOGI(TAG, "WifiScanHandler: driver settled (wl_status=%d), launching scan",
                 (int)WiFi.status());
        WiFi.scanDelete();
        int16_t ret = WiFi.scanNetworks(true, true); // async, show_hidden
        ESP_LOGI(TAG, "WifiScanHandler: scanNetworks returned %d (expect -1=RUNNING)", (int)ret);
        if (ret == WIFI_SCAN_RUNNING) {
          s_scanState   = SCAN_RUNNING;
          s_scanStateMs = millis();
        } else {
          ESP_LOGE(TAG, "WifiScanHandler: scan failed to start (ret=%d)", (int)ret);
          scanFinalize(-1);
        }
      }
      break;

    // -----------------------------------------------------------------------
    case SCAN_RUNNING: {
      int n = WiFi.scanComplete();
      if (n == WIFI_SCAN_RUNNING) {
        if (millis() - s_scanStateMs > SCAN_TIMEOUT_MS) {
          ESP_LOGW(TAG, "WifiScanHandler: scan timed out after %ds", SCAN_TIMEOUT_MS / 1000);
          WiFi.scanDelete();
          scanFinalize(0);
        }
        return;
      }
      if (n < 0) {
        ESP_LOGW(TAG, "WifiScanHandler: scan error (%d)", n);
      } else {
        ESP_LOGI(TAG, "WifiScanHandler: scan complete — %d networks", n);
        for (int i = 0; i < n; i++) {
          ESP_LOGI(TAG, "  [%d] '%s' RSSI=%d", i, WiFi.SSID(i).c_str(), WiFi.RSSI(i));
        }
      }
      scanFinalize(n);
      break;
    }
  }
}

bool WifiScanIsInProgress(void) { return s_scanState != SCAN_IDLE; }

bool WifiScanResultsReady(void) { return s_scanResultsReady; }

// Called from UI task when user presses Disconnect.
// Stops the current connection and suppresses auto-reconnect until wifiInit() is called.
void WifiDisconnect(void) {
  WiFi.setAutoReconnect(false);
  WiFi.disconnect(false);
  s_suppressReconnect = true;
  ESP_LOGI(TAG, "WifiDisconnect: user-requested disconnect");
}

// Called from UI task when user edits the SSID field or wants to start a scan.
// Harder abort than WifiDisconnect(): also resets the scan state machine and
// the reconnect timer, ensuring the IDF is fully idle before we scan.
// The 300 ms settle window is already handled by SCAN_DISCONNECTING in WifiScanHandler.
void WifiAbortConnection(void) {
  WiFi.setAutoReconnect(false);
  WiFi.disconnect(false);
  WiFi.scanDelete();
  s_suppressReconnect = true;
  s_scanState         = SCAN_IDLE;
  s_scanInProgress    = false;
  Wifi_TB.lastWifiReconnectAttempt = millis();
  ESP_LOGI(TAG, "SSID edit → connection aborted (wl_status=%d)", (int)WiFi.status());
}

int WifiScanGetCount(void) { return s_scanCount; }

String WifiScanGetSSID(int index) {
  if (index < 0 || index >= s_scanCount) return "";
  return WiFi.SSID(index);
}

int WifiScanGetRSSI(int index) {
  if (index < 0 || index >= s_scanCount) return 0;
  return WiFi.RSSI(index);
}

void WifiScanClear(void) {
  WiFi.scanDelete();
  s_scanInProgress = false;
  s_scanResultsReady = false;
  s_scanCount = 0;
}

/*
   setup function
*/
void wifiInit(void) {
  Wifi_TB.lastWifiReconnectAttempt = millis();
  s_suppressReconnect = false; // user is explicitly connecting — re-enable auto-reconnect loop

  // Guard: wifiInit() must not run while a scan is active.
  // scanFinalize() always sets s_scanState=SCAN_IDLE before calling here, so
  // this only triggers if wifiInit() is called externally (e.g. WifiConnectButton
  // in UI task) during an active scan — we abort the scan in that case.
  if (s_scanState != SCAN_IDLE) {
    ESP_LOGW(TAG, "wifiInit: called during scan (state=%d), aborting scan", (int)s_scanState);
    WiFi.scanDelete();
    s_scanInProgress   = false;
    s_scanResultsReady = true;
    s_scanCount        = 0;
    s_scanState        = SCAN_IDLE;
    WiFi.setAutoReconnect(true);
  }

  ESP_LOGI(TAG, "wifiInit: starting — current wl_status=%d", (int)WiFi.status());
  WiFi.setHostname(
      String(String(WIFI_NAME) + "-" + String(in3.serialNumber)).c_str());
  WiFi.mode(WIFI_STA);
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);

  String ssid;
  String pass;

  if (strlen(pendingSSID) > 0) {
    ssid = pendingSSID;
    pass = pendingPass;
    ESP_LOGI(TAG, "wifiInit: connecting to pending SSID='%s'", ssid.c_str());
  } else {
    ssid = EEPROM.readString(EEPROM_WIFI_SSID);
    pass = EEPROM.readString(EEPROM_WIFI_PASSWORD);
    if (ssid.length() > 0) {
      ESP_LOGI(TAG, "Connecting to SSID from EEPROM: %s", ssid.c_str());
    } else {
      ESP_LOGI(TAG, "Connecting to default SSID: %s", WIFI_SSID);
      ssid = WIFI_SSID;
      pass = WIFI_PASSWORD;
    }
  }

  ESP_LOGI(TAG, "wifiInit: calling WiFi.begin('%s')", ssid.c_str());
  WiFi.begin(ssid.c_str(), pass.c_str());
  lastconnectiontrywifi = millis();
}

void wifiDisable() { WiFi.mode(WIFI_OFF); }

void configWifiServer() {
  // Wait for connection
  ESP_LOGI(TAG, "Connected to %s IP address %s", WIFI_SSID,
           WiFi.localIP().toString().c_str());

  /*use mdns for wifiHost name resolution*/
  if (!MDNS.begin(wifiHost)) { // http://esp32.local
    ESP_LOGI(TAG, "Error setting up MDNS responder!");
  }
  ESP_LOGI(TAG, "mDNS responder started");
  /*return index page which is stored in ServerIndex */
  wifiServer.on("/", HTTP_GET, []() {
    if (!wifiServer.authenticate(www_username, www_password)) {
      return wifiServer.requestAuthentication();
    }
    wifiServer.sendHeader("Connection", "close");
    wifiServer.send(200, "text/html", serverIndex);
  });
  wifiServer.on("/serverIndex", HTTP_GET, []() {
    if (!wifiServer.authenticate(www_username, www_password)) {
      return wifiServer.requestAuthentication();
    }
    wifiServer.sendHeader("Connection", "close");
    wifiServer.send(200, "text/html", serverIndex);
  });
  wifiServer.on("/get_fw_version", HTTP_GET, []() {
    String json = "{";
    json += "\"version\":\"" + String(FWversion) + "\"";
    json += "}";
    wifiServer.sendHeader("Connection", "close");
    wifiServer.send(200, "application/json", json);
  });
  /*handling uploading firmware file */
  wifiServer.on(
      "/update", HTTP_POST,
      []() {
        if (!wifiServer.authenticate(www_username, www_password)) {
          return wifiServer.requestAuthentication();
        }
        wifiServer.sendHeader("Connection", "close");
        wifiServer.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
        ESP.restart();
      },
      []() {
        if (!wifiServer.authenticate(www_username, www_password)) {
          return; // Allow nothing if not authenticated
        }
        HTTPUpload &upload = wifiServer.upload();
        if (upload.status == UPLOAD_FILE_START) {
          // debugSerial.printf("Update: %s\n", upload.filename.c_str());
          OTA_inprogress = true;
          if (!Update.begin(
                  UPDATE_SIZE_UNKNOWN)) { // start with max available size
            Update.printError(Serial);
          }
        } else if (upload.status == UPLOAD_FILE_WRITE) {
          /* flashing firmware to ESP*/
          if (Update.write(upload.buf, upload.currentSize) !=
              upload.currentSize) {
            Update.printError(Serial);
          }
        } else if (upload.status == UPLOAD_FILE_END) {
          if (Update.end(
                  true)) { // true to set the size to the current progress
            ESP_LOGI(TAG, "Update Success: %u\nRebooting...", upload.totalSize);
          } else {
            Update.printError(Serial);
          }
        }
      });
  wifiServer.begin();
}

void updatedCallback(const bool &success) {
  if (success) {
    ESP_LOGI(TAG, "[GPRS] -> Done, OTA will be implemented on next boot");
    // esp_restart();
  } else {
    ESP_LOGI(TAG, "[GPRS] -> No new firmware");
    // GPRS.OTAInProgress = false;
  }
}

void WIFI_UpdatedCallback(const bool &success) {
  if (success) {
    ESP_LOGI(TAG, "Done, OTA will be implemented on next boot");
    // esp_restart();
  } else {
    ESP_LOGI(TAG, "No new firmware");
    Update.abort();
  }
}

bool WIFICheckNewEvent() {
  bool retVal = false;
  bool WifiStatus = (WiFi.status() == WL_CONNECTED);
  bool serverConnectionStatus = WIFIIsConnectedToServer();
  if (serverConnectionStatus != Wifi_TB.lastServerConnectionStatus ||
      WifiStatus != Wifi_TB.lastWIFIConnectionStatus) {
    retVal = true;
  }
  Wifi_TB.lastWIFIConnectionStatus = WifiStatus;
  Wifi_TB.lastServerConnectionStatus = serverConnectionStatus;
  return (retVal);
}

bool WIFIIsConnected() { return (WiFi.status() == WL_CONNECTED); }

bool WIFIIsConnectedToServer() {
  return (Wifi_TB.serverConnectionStatus && WIFIIsConnected());
}

void progressCallback(const uint32_t &currentChunk,
                      const uint32_t &totalChuncks) {
  char buffer[50]; // Create a buffer to hold the formatted string
  snprintf(buffer, sizeof(buffer), "Progress %.2f%%",
           static_cast<float>(currentChunk * 100U) / totalChuncks);
  ESP_LOGI(TAG, "%s", buffer);
}

void WIFICheckOTA() {
  ESP_LOGI(TAG, "Checking WIFI firwmare Update...");
  OTA_inprogress = true;
  tb_wifi.Firmware_Send_Info(CURRENT_FIRMWARE_TITLE, FWversion);
  tb_wifi.Start_Firmware_Update(OTAcallback);
}

void WIFI_TB_Init() {
  Wifi_TB.provisioned = EEPROM.read(EEPROM_THINGSBOARD_PROVISIONED);
  ESP_LOGI(TAG, "WIFI_TB_Init check provisioning: %d", Wifi_TB.provisioned);
  if (Wifi_TB.provisioned) {
    Wifi_TB.device_token = EEPROM.readString(EEPROM_THINGSBOARD_TOKEN);
    ESP_LOGI(TAG, "Provisioned with token: %s", Wifi_TB.device_token.c_str());
  }
}

void WEB_OTA() {
  if (WiFi.status() == WL_CONNECTED) {
    if (strlen(pendingSSID) > 0 && WiFi.SSID() == String(pendingSSID)) {
      ESP_LOGI(TAG, "Connection successful, persisting credentials to EEPROM");
      EEPROM.writeString(EEPROM_WIFI_SSID, pendingSSID);
      EEPROM.writeString(EEPROM_WIFI_PASSWORD, pendingPass);
      EEPROM.commit();
      pendingSSID[0] = '\0';
      pendingPass[0] = '\0';
    }
    if (!WIFI_connection_status) {
      configWifiServer();
      WIFI_connection_status = true;
    } else {
      wifiServer.handleClient();
    }
  } else {
    WIFI_connection_status = false;
  }
}

void WIFIProvisionResponse(const JsonObjectConst &data) {
  ESP_LOGI(TAG, "Received device provision response");
  const size_t jsonSize = JSON_OBJECT_SIZE(data.size()) + 200;
  char buffer[jsonSize];
  serializeJson(data, buffer, jsonSize);

  if (strncmp(data["status"], "SUCCESS", strlen("SUCCESS")) != 0) {
    ESP_LOGE(TAG, "Provision response FAIL: %s",
             data["errorMsg"].as<String>().c_str());
    return;
  }

  if (strncmp(data[CREDENTIALS_TYPE], ACCESS_TOKEN_CRED_TYPE,
              strlen(ACCESS_TOKEN_CRED_TYPE)) == 0) {

    credentials.client_id = "";
    credentials.username = data[CREDENTIALS_VALUE].as<std::string>();
    credentials.password = "";
    Wifi_TB.provisioned = true;
    Wifi_TB.device_token = credentials.username.c_str();
    EEPROM.writeString(EEPROM_THINGSBOARD_TOKEN, Wifi_TB.device_token);
    EEPROM.write(EEPROM_THINGSBOARD_PROVISIONED, Wifi_TB.provisioned);
    EEPROM.commit();
    ESP_LOGI(TAG, "Device provisioned successfully");
  } else if (strncmp(data[CREDENTIALS_TYPE], MQTT_BASIC_CRED_TYPE,
                     strlen(MQTT_BASIC_CRED_TYPE)) == 0) {
    auto credentials_value = data[CREDENTIALS_VALUE].as<JsonObjectConst>();
    credentials.client_id = credentials_value[CLIENT_ID].as<std::string>();
    credentials.username = credentials_value[CLIENT_USERNAME].as<std::string>();
    credentials.password = credentials_value[CLIENT_PASSWORD].as<std::string>();
    Wifi_TB.provisioned = true;
    Wifi_TB.device_token = credentials.username.c_str();
    EEPROM.writeString(EEPROM_THINGSBOARD_TOKEN, Wifi_TB.device_token);
    EEPROM.write(EEPROM_THINGSBOARD_PROVISIONED, Wifi_TB.provisioned);
    EEPROM.commit();
    ESP_LOGI(TAG, "Device provisioned successfully");
  } else {
    ESP_LOGI(TAG, "Unexpected provision credentialsType");
    return;
  }
  if (tb_wifi.connected()) {
    tb_wifi.disconnect();
  }
  Wifi_TB.provision_request_processed = true;
}

void WIFITBProvision() {
  if (in3.serialNumber == 0) {
    ESP_LOGI(TAG, "Serial number is 0, skipping provisioning");
    return;
  }
  if (!tb_wifi.connected()) {
    ESP_LOGI(TAG, "Connecting for provision to: %s", THINGSBOARD_SERVER);
    if (!tb_wifi.connect(THINGSBOARD_SERVER, "provision", THINGSBOARD_PORT)) {
      ESP_LOGI(TAG, "Failed to connect");
      return;
    }
  }
  // Connect to the ThingsBoard
  ESP_LOGI(TAG, "Sending provision request to: %s", THINGSBOARD_SERVER);

  String deviceName = String(WIFI_NAME) + "-" + String(in3.serialNumber);

  const Provision_Callback provisionCallback(
      Access_Token(), &WIFIProvisionResponse, PROVISION_DEVICE_KEY,
      PROVISION_DEVICE_SECRET, deviceName.c_str());
  Wifi_TB.provision_request_sent = tb_wifi.Provision_Request(provisionCallback);
}

void switchAlarmTelemetryWIFI(int alarm, bool value) {
  // String alarmKey;
  // switch (alarm) {
  // case HUMIDITY_ALARM:
  //   alarmKey = HUMIDITY_ALARM_KEY;
  //   break;
  // case TEMPERATURE_ALARM:
  //   alarmKey = TEMPERATURE_ALARM_KEY;
  //   break;
  // case AIR_THERMAL_CUTOUT_ALARM:
  //   alarmKey = AIR_THERMAL_CUTOUT_ALARM_KEY;
  //   break;
  // case SKIN_THERMAL_CUTOUT_ALARM:
  //   alarmKey = SKIN_THERMAL_CUTOUT_ALARM_KEY;
  //   break;
  // case AIR_SENSOR_ISSUE_ALARM:
  //   alarmKey = AIR_SENSOR_ISSUE_ALARM_KEY;
  //   break;
  // case SKIN_SENSOR_ISSUE_ALARM:
  //   alarmKey = SKIN_SENSOR_ISSUE_ALARM_KEY;
  //   break;
  // case FAN_ISSUE_ALARM:
  //   alarmKey = FAN_ISSUE_ALARM_KEY;
  //   break;
  // case HEATER_ISSUE_ALARM:
  //   alarmKey = HEATER_ISSUE_ALARM_KEY;
  //   break;
  // case POWER_SUPPLY_ALARM:
  //   alarmKey = POWER_SUPPLY_ALARM_KEY;
  //   break;
  // default:
  //   return;
  // }
  // addVariableToTelemetryWIFIJSON[alarmKey] = value;
}

void addAlarmTelemetriesToWIFIJSON() {
  // int alarmReported = false;
  // for (int i = NO_ALARMS + 1; i < NUM_ALARMS; i++) {
  //   if (in3.alarmToReport[i]) {
  //     switchAlarmTelemetryWIFI(i, true);
  //     alarmReported = true;
  //     in3.previousAlarmReport = true;
  //   }
  // }
  // if (!alarmReported) {
  //   if (in3.previousAlarmReport) {
  //     in3.previousAlarmReport = false;
  //     for (int i = NO_ALARMS + 1; i < NUM_ALARMS; i++) {
  //       switchAlarmTelemetryWIFI(i, false);
  //     }
  //   }
  // }
}

void addConfigTelemetriesToWIFIJSON() {
  // addAlarmTelemetriesToWIFIJSON();
  // addVariableToTelemetryWIFIJSON[SN_KEY] = in3.serialNumber;
  // addVariableToTelemetryWIFIJSON[SYSTEM_RESET_REASON] = in3.resetReason;
  // addVariableToTelemetryWIFIJSON[HW_NUM_KEY] = HW_NUM;
  // addVariableToTelemetryWIFIJSON[HW_REV_KEY] = String(HW_REVISION);
  // addVariableToTelemetryWIFIJSON[FW_VERSION_KEY] = FWversion;

  // addVariableToTelemetryWIFIJSON[SYS_CURR_STANDBY_TEST_KEY] =
  //     roundSignificantDigits(in3.system_current_standby_test,
  //                            TELEMETRIES_DECIMALS);
  // addVariableToTelemetryWIFIJSON[HEATER_CURR_TEST_KEY] =
  //     roundSignificantDigits(in3.heater_current_test, TELEMETRIES_DECIMALS);
  // addVariableToTelemetryWIFIJSON[FAN_CURR_TEST_KEY] =
  //     roundSignificantDigits(in3.fan_current_test, TELEMETRIES_DECIMALS);
  // addVariableToTelemetryWIFIJSON[PHOTOTHERAPY_CURR_KEY] =
  //     roundSignificantDigits(in3.phototherapy_current_test,
  //                            TELEMETRIES_DECIMALS);
  // addVariableToTelemetryWIFIJSON[PHOTOTHERAPY_PWM_KEY] =
  //     roundSignificantDigits(in3.phototherapy_intensity,
  //     TELEMETRIES_DECIMALS);
  // addVariableToTelemetryWIFIJSON[HUMIDIFIER_CURR_KEY] =
  //     roundSignificantDigits(in3.humidifier_current_test,
  //     TELEMETRIES_DECIMALS);
  // addVariableToTelemetryWIFIJSON[DISPLAY_CURR_TEST_KEY] =
  //     roundSignificantDigits(in3.display_current_test, TELEMETRIES_DECIMALS);
  // addVariableToTelemetryWIFIJSON[BUZZER_CURR_TEST_KEY] =
  //     roundSignificantDigits(in3.buzzer_current_test, TELEMETRIES_DECIMALS);
  // addVariableToTelemetryWIFIJSON[HW_TEST_KEY] = in3.HW_test_error_code;

  // addVariableToTelemetryWIFIJSON[UI_LANGUAGE_KEY] = in3.language;
  // addVariableToTelemetryWIFIJSON[CALIBRATED_SENSOR_KEY] =
  // !in3.calibrationError;
  // addVariableToTelemetryWIFIJSON[GPRS_CONNECTIVITY_KEY] = false;
  // addVariableToTelemetryWIFIJSON[WIFI_CONNECTIVITY_KEY] = true;
}

void addTelemetriesToWIFIJSON() {
  // addAlarmTelemetriesToWIFIJSON();
  // addVariableToTelemetryWIFIJSON[SKIN_CAPACITANCE_KEY] =
  //     in3.skinSensorCapacitance;
  // addVariableToTelemetryWIFIJSON[SKIN_TEMPERATURE_KEY] =
  // roundSignificantDigits(
  //     in3.temperature[SKIN_SENSOR], TELEMETRIES_DECIMALS);
  // addVariableToTelemetryWIFIJSON[AIR_TEMPERATURE_KEY] =
  // roundSignificantDigits(
  //     in3.temperature[ROOM_DIGITAL_TEMP_SENSOR], TELEMETRIES_DECIMALS);
  // if (in3.temperature[AMBIENT_DIGITAL_TEMP_SENSOR] &&
  //     in3.humidity[AMBIENT_DIGITAL_HUM_SENSOR]) {
  //   addVariableToTelemetryWIFIJSON[AMBIENT_TEMPERATURE_KEY] =
  //       roundSignificantDigits(in3.temperature[AMBIENT_DIGITAL_TEMP_SENSOR],
  //                              TELEMETRIES_DECIMALS);
  //   addVariableToTelemetryWIFIJSON[HUMIDITY_AMBIENT_KEY] =
  //       roundSignificantDigits(in3.humidity[AMBIENT_DIGITAL_HUM_SENSOR],
  //                              TELEMETRIES_DECIMALS);
  // }
  // addVariableToTelemetryWIFIJSON[PHOTOTHERAPY_ACTIVE_KEY] = in3.phototherapy;
  // addVariableToTelemetryWIFIJSON[HUMIDITY_ROOM_KEY] = roundSignificantDigits(
  //     in3.humidity[ROOM_DIGITAL_HUM_SENSOR], TELEMETRIES_DECIMALS);
  // addVariableToTelemetryWIFIJSON[SYSTEM_CURRENT_KEY] =
  //     roundSignificantDigits(in3.system_current, TELEMETRIES_DECIMALS);
  // addVariableToTelemetryWIFIJSON[SYSTEM_VOLTAGE_KEY] =
  //     roundSignificantDigits(in3.system_voltage, TELEMETRIES_DECIMALS);
  // addVariableToTelemetryWIFIJSON[V5_CURRENT_KEY] =
  //     roundSignificantDigits(in3.USB_current, TELEMETRIES_DECIMALS);
  // addVariableToTelemetryWIFIJSON[V5_VOLTAGE_KEY] =
  //     roundSignificantDigits(in3.USB_voltage, TELEMETRIES_DECIMALS);
  // addVariableToTelemetryWIFIJSON[BAT_CURRENT_KEY] =
  //     roundSignificantDigits(in3.BATTERY_current, TELEMETRIES_DECIMALS);
  // addVariableToTelemetryWIFIJSON[BAT_VOLTAGE_KEY] =
  //     roundSignificantDigits(in3.BATTERY_voltage, TELEMETRIES_DECIMALS);

  // if (in3.temperatureControl || in3.humidityControl) {
  //   addVariableToTelemetryWIFIJSON[FAN_CURRENT_KEY] =
  //       roundSignificantDigits(in3.fan_current, TELEMETRIES_DECIMALS);
  //   addVariableToTelemetryWIFIJSON[CONTROL_ACTIVE_TIME_KEY] =
  //       roundSignificantDigits(in3.control_active_time,
  //       TELEMETRIES_DECIMALS);
  //   addVariableToTelemetryWIFIJSON[FAN_ACTIVE_TIME_KEY] =
  //       roundSignificantDigits(in3.fan_active_time, TELEMETRIES_DECIMALS);
  //   if (in3.temperatureControl) {
  //     addVariableToTelemetryWIFIJSON[HEATER_CURRENT_KEY] =
  //         roundSignificantDigits(in3.heater_current, TELEMETRIES_DECIMALS);
  //     addVariableToTelemetryWIFIJSON[DESIRED_TEMPERATURE_KEY] =
  //         in3.desiredControlTemperature;
  //     addVariableToTelemetryWIFIJSON[HEATER_ACTIVE_TIME_KEY] =
  //         roundSignificantDigits(in3.heater_active_time,
  //         TELEMETRIES_DECIMALS);
  //   }
  //   if (in3.humidityControl) {
  //     addVariableToTelemetryWIFIJSON[DESIRED_HUMIDITY_ROOM_KEY] =
  //         in3.desiredControlHumidity;
  //   }
  //   if (!Wifi_TB.firstConfigPost) {
  //     Wifi_TB.firstConfigPost = true;
  //     addVariableToTelemetryWIFIJSON[CONTROL_ACTIVE_KEY] = true;
  //     if (in3.temperatureControl) {
  //       if (in3.controlMode == CONTROL_AIR) {
  //         addVariableToTelemetryWIFIJSON[CONTROL_MODE_KEY] = "AIR";
  //       } else {
  //         addVariableToTelemetryWIFIJSON[CONTROL_MODE_KEY] = "SKIN";
  //       }
  //     }
  //   }
  // } else {
  //   Wifi_TB.firstConfigPost = false;
  //   addVariableToTelemetryWIFIJSON[CONTROL_ACTIVE_KEY] = false;
  //   addVariableToTelemetryWIFIJSON[STANBY_TIME_KEY] =
  //       roundSignificantDigits(in3.standby_time, TELEMETRIES_DECIMALS);
  // }
  // if (in3.humidityControl) {
  //   addVariableToTelemetryWIFIJSON[HUMIDIFIER_CURRENT_KEY] =
  //       roundSignificantDigits(in3.humidifier_current, TELEMETRIES_DECIMALS);
  //   addVariableToTelemetryWIFIJSON[HUMIDIFIER_VOLTAGE_KEY] =
  //       roundSignificantDigits(in3.humidifier_voltage, TELEMETRIES_DECIMALS);
  //   addVariableToTelemetryWIFIJSON[HUMIDIFIER_ACTIVE_TIME_KEY] =
  //       roundSignificantDigits(in3.humidifier_active_time,
  //                              TELEMETRIES_DECIMALS);
  // }
  // if (in3.phototherapy) {
  //   addVariableToTelemetryWIFIJSON[PHOTOTHERAPY_CURRENT_KEY] =
  //       roundSignificantDigits(in3.phototherapy_current,
  //       TELEMETRIES_DECIMALS);
  //   addVariableToTelemetryWIFIJSON[PHOTHERAPY_ACTIVE_TIME_KEY] =
  //       roundSignificantDigits(in3.phototherapy_active_time,
  //                              TELEMETRIES_DECIMALS);
  // }
}

void WIFI_TB_OTA() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!Wifi_TB.provisioned) {
      if (!Wifi_TB.provision_request_sent) {
        WIFITBProvision();
      }
      tb_wifi.loop();
    } else {
      if (!tb_wifi.connected()) {
        // Connect to the ThingsBoard
        ESP_LOGI(TAG, "Connecting over WIFI to: %s with token %s",
                 THINGSBOARD_SERVER, Wifi_TB.device_token.c_str());
        if (!tb_wifi.connect(THINGSBOARD_SERVER,
                             Wifi_TB.device_token.c_str())) {
          ESP_LOGI(TAG, "Failed to connect");
          return;
        } else {
          ESP_LOGI(TAG, "Connected to host");
          Wifi_TB.serverConnectionStatus = true;
          if (ENABLE_WIFI_OTA && !Wifi_TB.OTA_requested) {
            ESP_LOGI(TAG, "Requesting OTA");
            WIFICheckOTA();
            Wifi_TB.OTA_requested = true;
          }
        }
      }
      if (tb_wifi.connected() &&
          millis() - Wifi_TB.lastMQTTPublish > WIFI_PUBLISH_INTERVAL) {
        if (!Wifi_TB.firstPublish) {
          Wifi_TB.firstPublish = true;
          // addConfigTelemetriesToWIFIJSON();
          // if (tb_wifi.sendTelemetryJson(addVariableToTelemetryWIFIJSON,
          //                               JSON_STRING_SIZE(measureJson(
          //                                   addVariableToTelemetryWIFIJSON))))
          //                                   {
          //   ESP_LOGI(TAG, "WIFI MQTT PUBLISH CONFIG SUCCESS");
          // } else {
          //   ESP_LOGI(TAG, "WIFI MQTT PUBLISH CONFIG FAIL");
          // }
          // WIFI_JSON.clear();
        }
        addTelemetriesToWIFIJSON();
        // if (tb_wifi.sendTelemetryJson(addVariableToTelemetryWIFIJSON,
        //                               JSON_STRING_SIZE(measureJson(
        //                                   addVariableToTelemetryWIFIJSON))))
        //                                   {
        //   ESP_LOGI(TAG, "WIFI MQTT PUBLISH TELEMETRIES SUCCESS");
        // } else {
        //   ESP_LOGI(TAG, "WIFI MQTT PUBLISH TELEMETRIES FAIL");
        // }
        // WIFI_JSON.clear();
        Wifi_TB.lastMQTTPublish = millis();
      }
    }
  } else {
    Wifi_TB.serverConnectionStatus = false;
  }
  tb_wifi.loop();
}

void WifiOTAHandler(void) {
  WifiScanHandler();

  // Don't run MQTT/OTA/reconnect while ANY scan state is active
  // (disconnecting, starting, or running) to avoid driver conflicts.
  if (s_scanState != SCAN_IDLE) return;

  WIFI_TB_OTA();
  WEB_OTA();
  if (WiFi.status() != WL_CONNECTED) {
    // s_suppressReconnect is set after every scan — cleared only by wifiInit()
    // (which is called from WifiConnectButton_cb when the user explicitly connects).
    // This prevents the reconnect loop from firing a WiFi.begin() between scans,
    // which would put the IDF in CONNECTING state and cause subsequent scans to fail.
    if (!s_suppressReconnect &&
        millis() - Wifi_TB.lastWifiReconnectAttempt > WIFI_RECONNECT_INTERVAL) {
      ESP_LOGI(TAG, "WifiOTAHandler: reconnecting (wl_status=%d)...", (int)WiFi.status());
      MDNS.end();
      wifiInit();
    }
  }
}

static void OTA_WIFI_Task(void *pvParameters) {
  wifiInit();
  WIFI_TB_Init();
  uint32_t lastStatusLogMs = 0;
  for (;;) {
    // Periodic WiFi status log (every 3 seconds) for serial debugging
    if (millis() - lastStatusLogMs >= 3000) {
      lastStatusLogMs = millis();
      ESP_LOGI(TAG, "[STATUS] wl=%d connected=%d scanState=%d scanReady=%d scanCount=%d suppressReconnect=%d",
               (int)WiFi.status(), (int)(WiFi.status() == WL_CONNECTED),
               (int)s_scanState, (int)s_scanResultsReady, (int)s_scanCount,
               (int)s_suppressReconnect);
    }
    WifiOTAHandler();
    vTaskDelay(pdMS_TO_TICKS(OTA_TASK_PERIOD_MS));
  }
}

void CreateOTATask() {
  xTaskCreatePinnedToCore(OTA_WIFI_Task, "OTA", 8192, NULL, OTA_TASK_PRIORITY,
                          NULL, CORE_ID_FREERTOS);
}