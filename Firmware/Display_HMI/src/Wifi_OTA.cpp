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

#include <string.h>

#include "esp_log.h"
#include "main.h"
#include "UITask.h"

#ifndef USE_IDF_FRAMEWORK
#include <Arduino.h>
#endif

static const char *TAG = "WiFi";

char wifiHost[32] = "in3ator";

#ifdef USE_IDF_FRAMEWORK
httpd_handle_t wifiServer = NULL;
Espressif_MQTT_Client mqttClientWIFI;
#else
WebServer wifiServer(80);
WiFiClient espClient;
Arduino_MQTT_Client mqttClientWIFI(espClient);
#endif

ThingsBoard tb_wifi(mqttClientWIFI, MAX_MESSAGE_SIZE);
StaticJsonDocument<JSON_OBJECT_SIZE(THINGSBOARD_FIELDS_AMOUNT)> WIFI_JSON;
JsonObject addVariableToTelemetryWIFIJSON = WIFI_JSON.to<JsonObject>();

WIFIstruct Wifi_TB;
Credentials credentials;

#ifndef USE_IDF_FRAMEWORK
Espressif_Updater updater_WIFI;

const OTA_Update_Callback OTAcallback(&progressCallback, &updatedCallback,
                                      CURRENT_FIRMWARE_TITLE, FWversion,
                                      &updater_WIFI, FIRMWARE_FAILURE_RETRIES,
                                      FIRMWARE_PACKET_SIZE,
                                      WAIT_FAILED_OTA_CHUNKS);
#endif

// Credentials staged by the UI before calling wifiInit().
// Persisted to EEPROM on the first successful GOT_IP with these credentials.
char pendingSSID[64] = "";
char pendingPass[64] = "";
static volatile bool s_persistCredentials = false;

#ifdef USE_IDF_FRAMEWORK
static volatile bool s_wifi_connected = false;

static void wifi_event_handler(void *arg, esp_event_base_t base,
                                int32_t id, void *data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *d = (wifi_event_sta_disconnected_t *)data;
        s_wifi_connected = false;
        ESP_LOGW(TAG, "STA_DISCONNECTED reason=%d", d->reason);
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = (ip_event_got_ip_t *)data;
        s_wifi_connected = true;
        ESP_LOGW(TAG, "STA_GOT_IP: " IPSTR "  [HEAP] internal=%u PSRAM=%u",
                 IP2STR(&e->ip_info.ip),
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
        if (pendingSSID[0] != '\0') {
            s_persistCredentials = true;
        }
    }
}
#endif // USE_IDF_FRAMEWORK

const char *www_username = "in3admin";
const char *www_password = "savinglives";

const char *serverIndex =
    "<script "
    "src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></"
    "script>"
    "<style>body{font-family:sans-serif;margin:20px} .section{margin:20px 0;padding:15px;border:1px solid #ccc;border-radius:8px} "
    "input[type=number]{width:120px;padding:4px;font-size:16px} "
    "button,.btn{padding:6px 16px;font-size:14px;cursor:pointer;border-radius:4px;border:1px solid #888} "
    "#freq_status{margin-left:10px;font-weight:bold}</style>"
    "<h2>IncuNest Display HMI</h2>"
    "<p>FW Version: <span id='fw_version'></span></p>"
    "<div class='section'>"
    "<h3>Display Pixel Clock</h3>"
    "<p>Current: <span id='cur_freq'>--</span> Hz (<span id='cur_freq_mhz'>--</span> MHz)</p>"
    "<label>New freq (Hz): </label>"
    "<input type='range' id='freq_slider' min='12000000' max='25000000' step='250000' oninput='updateSlider()'>"
    "<br><span id='freq_val' style='font-size:20px;font-weight:bold'>-- MHz</span>"
    "<br><br>"
    "<button onclick='setFreq()' style='padding:8px 24px;font-size:16px'>Apply (restart)</button>"
    "<span id='freq_status'></span>"
    "</div>"
    "<div class='section'>"
    "<h3>Firmware Update</h3>"
    "<form method='POST' action='#' enctype='multipart/form-data' "
    "id='upload_form'>"
    "<input type='file' name='update'>"
    "<input type='submit' value='Update'>"
    "</form>"
    "<div id='prg'>progress: 0%</div>"
    "</div>"
    "<script>"
    "function updateSlider(){"
    "  var v=$('#freq_slider').val();"
    "  $('#freq_val').text((v/1e6).toFixed(2)+' MHz');"
    "}"
    "function refreshFreq(){"
    "  $.get('/get_freq',function(d){"
    "    $('#cur_freq').text(d.freq);$('#cur_freq_mhz').text((d.freq/1e6).toFixed(2));"
    "    $('#freq_slider').val(d.freq);updateSlider();"
    "  });"
    "}"
    "function setFreq(){"
    "  var f=$('#freq_slider').val();"
    "  $.post('/set_freq',{freq:f},function(d){$('#freq_status').text(d.ok?'Restarting...':'FAIL').css('color',d.ok?'green':'red');});"
    "}"
    "$(document).ready(function(){"
    "  $.get('/get_fw_version',function(d){$('#fw_version').text(d.version);});"
    "  refreshFreq();"
    "});"
    "$('form').submit(function(e){"
    "e.preventDefault();"
    "var form=$('#upload_form')[0];"
    "var data=new FormData(form);"
    "$.ajax({"
    "url:'/update',type:'POST',data:data,contentType:false,processData:false,"
    "xhr:function(){var xhr=new window.XMLHttpRequest();"
    "xhr.upload.addEventListener('progress',function(evt){"
    "if(evt.lengthComputable){var per=evt.loaded/evt.total;"
    "$('#prg').html('progress: '+Math.round(per*100)+'%');}},false);"
    "return xhr;},"
    "success:function(d,s){console.log('success!')},"
    "error:function(a,b,c){}"
    "});"
    "});"
    "</script>";

// ---------------------------------------------------------------------------
// WiFi init — call once at boot, and again when credentials change.
// Auto-reconnect is enabled: the driver handles all subsequent reconnections
// automatically; no manual reconnect loop is needed.
// ---------------------------------------------------------------------------
void wifiInit(void) {
  ESP_LOGI(TAG, "Initializing WiFi");

  snprintf(wifiHost, sizeof(wifiHost), "%s-%d", WIFI_NAME, in3.serialNumber);
  ESP_LOGI(TAG, "Setting hostname to: %s", wifiHost);

#ifdef USE_IDF_FRAMEWORK

  // Initialize TCP/IP + event loop (idempotent)
  esp_netif_init();
  esp_event_loop_create_default();
  esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
  esp_netif_set_hostname(sta_netif, wifiHost);

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&cfg);

  static bool s_eventsRegistered = false;
  if (!s_eventsRegistered) {
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                               wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                               wifi_event_handler, NULL);
    s_eventsRegistered = true;
  }

  esp_wifi_set_mode(WIFI_MODE_STA);

  // Choose credentials
  char ssid[64] = {}, pass[64] = {};
  if (pendingSSID[0] != '\0') {
    strncpy(ssid, pendingSSID, sizeof(ssid) - 1);
    strncpy(pass, pendingPass, sizeof(pass) - 1);
    ESP_LOGI(TAG, "Connecting to pending SSID: %s", ssid);
  } else {
    std::string s = EEPROM.readString(EEPROM_WIFI_SSID);
    std::string p = EEPROM.readString(EEPROM_WIFI_PASSWORD);
    if (!s.empty()) {
      strncpy(ssid, s.c_str(), sizeof(ssid) - 1);
      strncpy(pass, p.c_str(), sizeof(pass) - 1);
      ESP_LOGI(TAG, "Connecting to SSID from EEPROM: %s", ssid);
    } else {
      strncpy(ssid, WIFI_SSID, sizeof(ssid) - 1);
      strncpy(pass, WIFI_PASSWORD, sizeof(pass) - 1);
      ESP_LOGI(TAG, "Connecting to default SSID: %s", ssid);
    }
  }

  wifi_config_t wifi_cfg = {};
  strncpy((char *)wifi_cfg.sta.ssid, ssid, sizeof(wifi_cfg.sta.ssid) - 1);
  strncpy((char *)wifi_cfg.sta.password, pass, sizeof(wifi_cfg.sta.password) - 1);

  ESP_LOGW(TAG, "[HEAP] before esp_wifi_start — internal=%u PSRAM=%u",
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

  esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
  esp_wifi_start();
  esp_wifi_set_ps(WIFI_PS_NONE);
  esp_wifi_connect();

#else // Arduino framework

  // setHostname must be called before mode() in Arduino 3.x / IDF 5.x.
  WiFi.setHostname(wifiHost);
  if (WiFi.getMode() != WIFI_MODE_STA) {
    WiFi.mode(WIFI_STA);
  }

  static bool s_eventsRegistered = false;
  if (!s_eventsRegistered) {
    WiFi.onEvent([](WiFiEvent_t, WiFiEventInfo_t info) {
      ESP_LOGW(TAG, "STA_DISCONNECTED reason=%d",
               info.wifi_sta_disconnected.reason);
    }, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

    WiFi.onEvent([](WiFiEvent_t, WiFiEventInfo_t) {
      ESP_LOGW(TAG, "STA_GOT_IP: %s  [HEAP] internal=%u PSRAM=%u",
               WiFi.localIP().toString().c_str(),
               (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
               (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
      MDNS.begin(wifiHost);
      if (pendingSSID[0] != '\0') {
        s_persistCredentials = true;
      }
    }, ARDUINO_EVENT_WIFI_STA_GOT_IP);

    s_eventsRegistered = true;
  }

  WiFi.persistent(true);
  WiFi.setAutoReconnect(true);

  String ssid, pass;
  if (pendingSSID[0] != '\0') {
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

  ESP_LOGW(TAG, "[HEAP] before WiFi.begin — internal=%u PSRAM=%u",
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
  WiFi.begin(ssid.c_str(), pass.c_str());
  WiFi.setSleep(WIFI_PS_NONE);

#endif // USE_IDF_FRAMEWORK
}

// ---------------------------------------------------------------------------
// Web server — register routes and start once at task init.
// ---------------------------------------------------------------------------
void configWifiServer() {
#ifdef USE_IDF_FRAMEWORK
  ESP_LOGI(TAG, "Web server: IDF httpd not yet implemented");
  // TODO: implement esp_http_server routes (Task 6)
  (void)wifiServer;
#else
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
    String json = "{\"version\":\"" + String(FWversion) + "\"}";
    wifiServer.sendHeader("Connection", "close");
    wifiServer.send(200, "application/json", json);
  });
  wifiServer.on("/get_freq", HTTP_GET, []() {
    String json = "{\"freq\":" + String(lcd_get_freq_write()) + "}";
    wifiServer.sendHeader("Connection", "close");
    wifiServer.send(200, "application/json", json);
  });
  wifiServer.on("/set_freq", HTTP_POST, []() {
    if (!wifiServer.authenticate(www_username, www_password)) {
      return wifiServer.requestAuthentication();
    }
    uint32_t freq = wifiServer.arg("freq").toInt();
    bool ok = (freq >= DISPLAY_FREQ_MIN && freq <= DISPLAY_FREQ_MAX);
    if (ok) lcd_set_freq_write(freq);
    String json = "{\"ok\":" + String(ok ? "true" : "false") +
                  ",\"freq\":" + String(lcd_get_freq_write()) + "}";
    wifiServer.sendHeader("Connection", "close");
    wifiServer.send(200, "application/json", json);
  });
  wifiServer.on(
      "/update", HTTP_POST,
      []() {
        if (!wifiServer.authenticate(www_username, www_password)) {
          return wifiServer.requestAuthentication();
        }
        wifiServer.sendHeader("Connection", "close");
        wifiServer.send(200, "text/plain", Update.hasError() ? "FAIL" : "OK");
        ESP.restart();
      },
      []() {
        if (!wifiServer.authenticate(www_username, www_password)) return;
        HTTPUpload &upload = wifiServer.upload();
        if (upload.status == UPLOAD_FILE_START) {
          OTA_inprogress = true;
          if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
        } else if (upload.status == UPLOAD_FILE_WRITE) {
          if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
            Update.printError(Serial);
        } else if (upload.status == UPLOAD_FILE_END) {
          if (!Update.end(true)) Update.printError(Serial);
        }
      });
  wifiServer.begin();
  ESP_LOGI(TAG, "Web server started on port 80");
#endif // USE_IDF_FRAMEWORK
}

// ---------------------------------------------------------------------------
// ThingsBoard OTA callbacks
// ---------------------------------------------------------------------------
void progressCallback(const uint32_t &currentChunk, const uint32_t &totalChuncks) {
  ESP_LOGI(TAG, "OTA progress %.2f%%",
           static_cast<float>(currentChunk * 100U) / totalChuncks);
}

void updatedCallback(const bool &success) {
  if (success) {
    ESP_LOGI(TAG, "OTA done, will apply on next boot");
  } else {
    ESP_LOGI(TAG, "OTA: no new firmware");
  }
}

bool WIFIIsConnected() {
#ifdef USE_IDF_FRAMEWORK
  return s_wifi_connected;
#else
  return WiFi.status() == WL_CONNECTED;
#endif
}

bool WIFIIsConnectedToServer() {
  return Wifi_TB.serverConnectionStatus && WIFIIsConnected();
}

void WIFICheckOTA() {
  ESP_LOGI(TAG, "Checking ThingsBoard firmware update...");
  OTA_inprogress = true;
  tb_wifi.Firmware_Send_Info(CURRENT_FIRMWARE_TITLE, FWversion);
#ifndef USE_IDF_FRAMEWORK
  tb_wifi.Start_Firmware_Update(OTAcallback);
#endif
}

void WIFI_TB_Init() {
  Wifi_TB.provisioned = EEPROM.read(EEPROM_THINGSBOARD_PROVISIONED);
  ESP_LOGI(TAG, "WIFI_TB_Init provisioned=%d", Wifi_TB.provisioned);
  if (Wifi_TB.provisioned) {
    Wifi_TB.device_token = std::string(EEPROM.readString(EEPROM_THINGSBOARD_TOKEN).c_str());
    ESP_LOGI(TAG, "Token: %s", Wifi_TB.device_token.c_str());
  }
}

void WIFIProvisionResponse(const JsonObjectConst &data) {
  ESP_LOGI(TAG, "Received provision response");
  const size_t jsonSize = JSON_OBJECT_SIZE(data.size()) + 200;
  char buffer[jsonSize];
  serializeJson(data, buffer, jsonSize);

  if (strncmp(data["status"], "SUCCESS", strlen("SUCCESS")) != 0) {
    ESP_LOGE(TAG, "Provision FAIL: %s", data["errorMsg"].as<const char *>());
    return;
  }

  if (strncmp(data[CREDENTIALS_TYPE], ACCESS_TOKEN_CRED_TYPE,
              strlen(ACCESS_TOKEN_CRED_TYPE)) == 0) {
    credentials.client_id = "";
    credentials.username = data[CREDENTIALS_VALUE].as<std::string>();
    credentials.password = "";
  } else if (strncmp(data[CREDENTIALS_TYPE], MQTT_BASIC_CRED_TYPE,
                     strlen(MQTT_BASIC_CRED_TYPE)) == 0) {
    auto cv = data[CREDENTIALS_VALUE].as<JsonObjectConst>();
    credentials.client_id = cv[CLIENT_ID].as<std::string>();
    credentials.username  = cv[CLIENT_USERNAME].as<std::string>();
    credentials.password  = cv[CLIENT_PASSWORD].as<std::string>();
  } else {
    ESP_LOGW(TAG, "Unexpected credentialsType");
    return;
  }

  Wifi_TB.provisioned = true;
  Wifi_TB.device_token = credentials.username.c_str();
  EEPROM.writeString(EEPROM_THINGSBOARD_TOKEN, Wifi_TB.device_token.c_str());
  EEPROM.write(EEPROM_THINGSBOARD_PROVISIONED, Wifi_TB.provisioned);
  EEPROM.commit();
  ESP_LOGI(TAG, "Device provisioned successfully");

  if (tb_wifi.connected()) tb_wifi.disconnect();
  Wifi_TB.provision_request_processed = true;
}

void WIFITBProvision() {
  if (in3.serialNumber == 0) {
    static bool logged = false;
    if (!logged) { ESP_LOGI(TAG, "Serial 0, skipping provisioning"); logged = true; }
    return;
  }
  if (!tb_wifi.connected()) {
    ESP_LOGI(TAG, "Connecting for provision to: %s", THINGSBOARD_SERVER);
    if (!tb_wifi.connect(THINGSBOARD_SERVER, "provision", THINGSBOARD_PORT)) {
      ESP_LOGI(TAG, "Failed to connect");
      return;
    }
  }
  char deviceName[64];
  snprintf(deviceName, sizeof(deviceName), "%s-%d", WIFI_NAME, in3.serialNumber);
  const Provision_Callback provisionCallback(
      Access_Token(), &WIFIProvisionResponse, PROVISION_DEVICE_KEY,
      PROVISION_DEVICE_SECRET, deviceName);
  Wifi_TB.provision_request_sent = tb_wifi.Provision_Request(provisionCallback);
}

void addTelemetriesToWIFIJSON() {
  addVariableToTelemetryWIFIJSON["fw_version"] = FWversion;
  addVariableToTelemetryWIFIJSON["sn"] = in3.serialNumber;
}

void WIFI_TB_OTA() {
  if (!WIFIIsConnected()) {
    Wifi_TB.serverConnectionStatus = false;
    tb_wifi.loop();
    return;
  }

  if (!Wifi_TB.provisioned) {
    if (!Wifi_TB.provision_request_sent) WIFITBProvision();
    tb_wifi.loop();
    return;
  }

  if (!tb_wifi.connected()) {
    if (Wifi_TB.lastReconnectAttempt != 0 &&
        millis() - Wifi_TB.lastReconnectAttempt < THINGSBOARD_RECONNECT_DELAY) {
      tb_wifi.loop();
      return;
    }
    Wifi_TB.lastReconnectAttempt = millis();
    ESP_LOGW(TAG, "TB disconnected, reconnecting... heap=%u",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    if (!tb_wifi.connect(THINGSBOARD_SERVER, Wifi_TB.device_token.c_str())) {
      ESP_LOGI(TAG, "TB connect failed");
      tb_wifi.loop();
      return;
    }
    ESP_LOGI(TAG, "TB connected");
    Wifi_TB.serverConnectionStatus = true;
    WIFICheckOTA();
    Wifi_TB.lastOTACheck = millis();
  } else {
    if (millis() - Wifi_TB.lastMQTTPublish > WIFI_PUBLISH_INTERVAL) {
      addTelemetriesToWIFIJSON();
      bool ok = tb_wifi.sendTelemetryJson(
          addVariableToTelemetryWIFIJSON,
          JSON_STRING_SIZE(measureJson(addVariableToTelemetryWIFIJSON)));
      ESP_LOGI(TAG, "TB telemetry: %s", ok ? "OK" : "FAIL");
      WIFI_JSON.clear();
      Wifi_TB.lastMQTTPublish = millis();
    }
    if (millis() - Wifi_TB.lastOTACheck > WIFI_OTA_CHECK_INTERVAL) {
      WIFICheckOTA();
      Wifi_TB.lastOTACheck = millis();
    }
  }
  tb_wifi.loop();
}

// ---------------------------------------------------------------------------
// Main OTA/WiFi handler — called every OTA_TASK_PERIOD_MS from the OTA task.
// ---------------------------------------------------------------------------
void WifiOTAHandler(void) {
  // Persist credentials staged by the UI on the first successful connection.
  if (s_persistCredentials) {
    s_persistCredentials = false;
    EEPROM.writeString(EEPROM_WIFI_SSID, pendingSSID);
    EEPROM.writeString(EEPROM_WIFI_PASSWORD, pendingPass);
    EEPROM.commit();
    ESP_LOGI(TAG, "WiFi credentials saved to EEPROM (SSID: %s)", pendingSSID);
    pendingSSID[0] = '\0';
    pendingPass[0] = '\0';
  }

  WIFI_TB_OTA();

  if (WIFIIsConnected()) {
#ifndef USE_IDF_FRAMEWORK
    wifiServer.handleClient();
#endif
  }
}

static void OTA_WIFI_Task(void *pvParameters) {
  wifiInit();           // Sets STA mode and initializes the TCP/IP stack first.
  configWifiServer();   // Safe to start the TCP server now that the stack is up.
  WIFI_TB_Init();
  for (;;) {
    WifiOTAHandler();
    vTaskDelay(pdMS_TO_TICKS(OTA_TASK_PERIOD_MS));
  }
}

void CreateOTATask() {
  xTaskCreatePinnedToCore(OTA_WIFI_Task, "OTA", OTA_TASK_STACK_SIZE, NULL,
                          OTA_TASK_PRIORITY, NULL, CORE_ID_FREERTOS);
}
