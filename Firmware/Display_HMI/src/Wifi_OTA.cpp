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

// WiFi Scan Stubs (Phase 1: Disable logic, keep signatures)
volatile bool g_wifiScanRequest = false;
void WifiScanRequest(void) { g_wifiScanRequest = false; }
void WifiScanHandler(void) { }
bool WifiScanIsInProgress(void) { return false; }
bool WifiScanResultsReady(void) { return false; }
int  WifiScanGetCount(void) { return 0; }
String WifiScanGetSSID(int index) { return ""; }
int  WifiScanGetRSSI(int index) { return 0; }
uint8_t* WifiScanGetBSSID(int index) { return NULL; }
int8_t   WifiScanGetChannel(int index) { return 0; }
void WifiScanClear(void) { }
void WifiDisconnect(void) { WiFi.disconnect(false); }
void WifiAbortConnection(void) { WiFi.disconnect(false); }
int  WifiGetLastDisconnectReason(void) { return 0; }

/*
   setup function
*/
void wifiInit(void) {
  // Connect to WiFi network
  ESP_LOGI(TAG, "Initializing WiFi (Fase 1: Conexión Básica)");
  Wifi_TB.lastWifiReconnectAttempt = millis();
  WiFi.setHostname(
      String(String(WIFI_NAME) + "-" + String(in3.serialNumber)).c_str());
  WiFi.mode(WIFI_STA);
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);

  String ssid;
  String pass;

  if (strlen(pendingSSID) > 0) {
    ssid = pendingSSID;
    pass = pendingPass;
    ESP_LOGI(TAG, "Connecting to pending SSID: %s", ssid.c_str());
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
}

void addAlarmTelemetriesToWIFIJSON() {
}

void addConfigTelemetriesToWIFIJSON() {
}

void addTelemetriesToWIFIJSON() {
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
        }
        addTelemetriesToWIFIJSON();
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
  WIFI_TB_OTA();
  WEB_OTA();
  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - Wifi_TB.lastWifiReconnectAttempt > WIFI_RECONNECT_INTERVAL) {
      ESP_LOGI(TAG, "Connection lost, attempting to reconnect...");
      MDNS.end();
      wifiInit();
    }
  }
}

static void OTA_WIFI_Task(void *pvParameters) {
  wifiInit();
  WIFI_TB_Init();
  for (;;) {
    WifiOTAHandler();
    vTaskDelay(pdMS_TO_TICKS(OTA_TASK_PERIOD_MS));
  }
}

void CreateOTATask() {
  xTaskCreatePinnedToCore(OTA_WIFI_Task, "OTA", 8192, NULL, OTA_TASK_PRIORITY,
                          NULL, CORE_ID_FREERTOS);
}