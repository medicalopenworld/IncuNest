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
#include "UITask.h"

static const char *TAG = "WiFi";

char wifiHost[32] = "in3ator";

WebServer wifiServer(80);

WiFiClient espClient;
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
    "<style>body{font-family:sans-serif;margin:20px} .section{margin:20px 0;padding:15px;border:1px solid #ccc;border-radius:8px} "
    "input[type=number]{width:120px;padding:4px;font-size:16px} "
    "button,.btn{padding:6px 16px;font-size:14px;cursor:pointer;border-radius:4px;border:1px solid #888} "
    "#freq_status{margin-left:10px;font-weight:bold}</style>"
    "<h2>IncuNest Display HMI</h2>"
    "<p>FW Version: <span id='fw_version'></span></p>"
    // --- Display Freq section ---
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
    // --- OTA section ---
    "<div class='section'>"
    "<h3>Firmware Update</h3>"
    "<form method='POST' action='#' enctype='multipart/form-data' "
    "id='upload_form'>"
    "<input type='file' name='update'>"
    "<input type='submit' value='Update'>"
    "</form>"
    "<div id='prg'>progress: 0%</div>"
    "</div>"
    // --- JS ---
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

char pendingSSID[64] = "";
char pendingPass[64] = "";
static uint32_t lastconnectiontrywifi = 0;

/*
   setup function
*/
void wifiInit(void) {
  // Connect to WiFi network
  ESP_LOGI(TAG, "Initializing WiFi");
  Wifi_TB.lastWifiReconnectAttempt = millis();
  String hostname = String(WIFI_NAME) + "-" + String(in3.serialNumber);
  // Copy hostname to wifiHost for MDNS
  strncpy(wifiHost, hostname.c_str(), sizeof(wifiHost) - 1);
  wifiHost[sizeof(wifiHost) - 1] = '\0';
  ESP_LOGI(TAG, "Setting hostname to: %s", wifiHost);

  // In Arduino 3.x (ESP-IDF 5.x), setHostname must be called BEFORE mode(WIFI_STA).
  WiFi.setHostname(hostname.c_str());
  // Skip mode change if already STA — calling mode() while connecting causes ESP_ERR_WIFI_CONN.
  if (WiFi.getMode() != WIFI_MODE_STA) {
    WiFi.mode(WIFI_STA);
  }

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
  wifiServer.on("/get_freq", HTTP_GET, []() {
    String json = "{\"freq\":" + String(lcd_get_freq_write()) + "}";
    wifiServer.sendHeader("Connection", "close");
    wifiServer.send(200, "application/json", json);
  });
  wifiServer.on("/set_freq", HTTP_POST, []() {
    if (!wifiServer.authenticate(www_username, www_password)) {
      return wifiServer.requestAuthentication();
    }
    String freqStr = wifiServer.arg("freq");
    uint32_t freq = freqStr.toInt();
    bool ok = (freq >= DISPLAY_FREQ_MIN && freq <= DISPLAY_FREQ_MAX);
    if (ok) {
      lcd_set_freq_write(freq);
    }
    String json = "{\"ok\":" + String(ok ? "true" : "false") + ",\"freq\":" + String(lcd_get_freq_write()) + "}";
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
    static bool logged = false;
    if (!logged) {
      ESP_LOGI(TAG, "Serial number is 0, skipping provisioning");
      logged = true;
    }
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
  addVariableToTelemetryWIFIJSON["fw_version"] = FWversion;
  addVariableToTelemetryWIFIJSON["sn"] = in3.serialNumber;
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
        if (Wifi_TB.lastReconnectAttempt != 0 &&
            millis() - Wifi_TB.lastReconnectAttempt < THINGSBOARD_RECONNECT_DELAY) {
          return;
        }
        ESP_LOGW(TAG, "TB disconnected. tcp_connected=%d wifi_status=%d heap=%u",
                 (int)espClient.connected(), (int)WiFi.status(),
                 (unsigned)ESP.getFreeHeap());
        ESP_LOGI(TAG, "Connecting over WIFI to: %s with token %s",
                 THINGSBOARD_SERVER, Wifi_TB.device_token.c_str());
        Wifi_TB.lastReconnectAttempt = millis();
        if (!tb_wifi.connect(THINGSBOARD_SERVER,
                             Wifi_TB.device_token.c_str())) {
          ESP_LOGI(TAG, "Failed to connect");
          return;
        } else {
          ESP_LOGI(TAG, "Connected to host");
          Wifi_TB.serverConnectionStatus = true;
          WIFICheckOTA();
          Wifi_TB.lastOTACheck = millis();
        }
      } else {
        if (millis() - Wifi_TB.lastMQTTPublish > WIFI_PUBLISH_INTERVAL) {
          addTelemetriesToWIFIJSON();
          if (tb_wifi.sendTelemetryJson(addVariableToTelemetryWIFIJSON,
                                        JSON_STRING_SIZE(measureJson(
                                            addVariableToTelemetryWIFIJSON)))) {
            ESP_LOGI(TAG, "WIFI MQTT PUBLISH TELEMETRIES SUCCESS");
          } else {
            ESP_LOGI(TAG, "WIFI MQTT PUBLISH TELEMETRIES FAIL");
          }
          WIFI_JSON.clear();
          Wifi_TB.lastMQTTPublish = millis();
        }
        if (millis() - Wifi_TB.lastOTACheck > WIFI_OTA_CHECK_INTERVAL) {
          WIFICheckOTA();
          Wifi_TB.lastOTACheck = millis();
        }
      }
    }
  } else {
    Wifi_TB.serverConnectionStatus = false;
  }
  tb_wifi.loop();
}

void WifiOTAHandler(void) {
  WIFI_TB_OTA();
  WEB_OTA();
  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - Wifi_TB.lastWifiReconnectAttempt > WIFI_RECONNECT_INTERVAL) {
      // Wifi_TB.lastWifiReconnectAttempt = millis(); // wifiInit does this
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
  xTaskCreatePinnedToCore(OTA_WIFI_Task, "OTA", OTA_TASK_STACK_SIZE, NULL, OTA_TASK_PRIORITY,
                          NULL, CORE_ID_FREERTOS);
}