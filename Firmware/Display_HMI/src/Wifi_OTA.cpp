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

#include "esp_log.h"
#include "main.h"

static const char *TAG = "Wifi_OTA";

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

const char *loginIndex =
    "<form name='loginForm'>"
    "<table width='20%' bgcolor='A09F9F' align='center'>"
    "<tr>"
    "<td colspan=2>"
    "<center><font size=4><b>ESP32 Login Page</b></font></center>"
    "<br>"
    "</td>"
    "<br>"
    "<br>"
    "</tr>"
    "<tr>"
    "<td>Username:</td>"
    "<td><input type='text' size=25 name='userid'><br></td>"
    "</tr>"
    "<br>"
    "<br>"
    "<tr>"
    "<td>WIFI_PASSWORD:</td>"
    "<td><input type='WIFI_PASSWORD' size=25 name='pwd'><br></td>"
    "<br>"
    "<br>"
    "</tr>"
    "<tr>"
    "<td><input type='submit' onclick='check(this.form)' value='Login'></td>"
    "</tr>"
    "</table>"
    "</form>"
    "<script>"
    "function check(form)"
    "{"
    "if(form.userid.value=='in3admin' && form.pwd.value=='savinglives')"
    "{"
    "window.open('/serverIndex')"
    "}"
    "else"
    "{"
    " alert('Error WIFI_PASSWORD or Username')/*displays error message*/"
    "}"
    "}"
    "</script>";

/*
   wifiServer Index Page
*/

const char *serverIndex =
    "<script "
    "src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></"
    "script>"
    "<form method='POST' action='#' enctype='multipart/form-data' "
    "id='upload_form'>"
    "<input type='file' name='update'>"
    "<input type='submit' value='Update'>"
    "</form>"
    "<div id='prg'>progress: 0%</div>"
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

int serialNumber = 0;
/*
   setup function
*/
void wifiInit(void) {
  // Connect to WiFi network
  ESP_LOGI(TAG, "Initializing WiFi");
  WiFi.setHostname(
      String(String(WIFI_NAME) + "-" + String(serialNumber)).c_str());
  WiFi.mode(WIFI_STA);
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
  ESP_LOGI(TAG, "Connecting to SSID: %s", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
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
    wifiServer.sendHeader("Connection", "close");
    wifiServer.send(200, "text/html", loginIndex);
  });
  wifiServer.on("/serverIndex", HTTP_GET, []() {
    wifiServer.sendHeader("Connection", "close");
    wifiServer.send(200, "text/html", serverIndex);
  });
  /*handling uploading firmware file */
  wifiServer.on(
      "/update", HTTP_POST,
      []() {
        wifiServer.sendHeader("Connection", "close");
        wifiServer.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
        ESP.restart();
      },
      []() {
        HTTPUpload &upload = wifiServer.upload();
        if (upload.status == UPLOAD_FILE_START) {
          // debugSerial.printf("Update: %s\n", upload.filename.c_str());
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
  tb_wifi.Firmware_Send_Info(CURRENT_FIRMWARE_TITLE, FWversion);
  tb_wifi.Start_Firmware_Update(OTAcallback);
}

void WIFI_TB_Init() {
  // Wifi_TB.provisioned = EEPROM.read(EEPROM_THINGSBOARD_PROVISIONED);
  Wifi_TB.provisioned = false;
  ESP_LOGI(TAG, "WIFI_TB_Init check provisioning: %d", Wifi_TB.provisioned);
  if (Wifi_TB.provisioned) {
    // Wifi_TB.device_token = EEPROM.readString(EEPROM_THINGSBOARD_TOKEN);
    ESP_LOGI(TAG, "Provisioned with token: %s", Wifi_TB.device_token.c_str());
  }
}

void WEB_OTA() {
  if (WiFi.status() == WL_CONNECTED) {
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
    ESP_LOGI(TAG, "Provision response contains the error: %s",
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
    // EEPROM.writeString(EEPROM_THINGSBOARD_TOKEN, Wifi_TB.device_token);
    // EEPROM.write(EEPROM_THINGSBOARD_PROVISIONED, Wifi_TB.provisioned);
    // EEPROM.commit();
    ESP_LOGI(TAG, "Device provisioned successfully");
  } else if (strncmp(data[CREDENTIALS_TYPE], MQTT_BASIC_CRED_TYPE,
                     strlen(MQTT_BASIC_CRED_TYPE)) == 0) {
    auto credentials_value = data[CREDENTIALS_VALUE].as<JsonObjectConst>();
    credentials.client_id = credentials_value[CLIENT_ID].as<std::string>();
    credentials.username = credentials_value[CLIENT_USERNAME].as<std::string>();
    credentials.password = credentials_value[CLIENT_PASSWORD].as<std::string>();
    Wifi_TB.provisioned = true;
    Wifi_TB.device_token = credentials.username.c_str();
    // EEPROM.writeString(EEPROM_THINGSBOARD_TOKEN, Wifi_TB.device_token);
    // EEPROM.write(EEPROM_THINGSBOARD_PROVISIONED, Wifi_TB.provisioned);
    // EEPROM.commit();
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
  if (!tb_wifi.connected()) {
    ESP_LOGI(TAG, "Connecting for provision to: %s", THINGSBOARD_SERVER);
    if (!tb_wifi.connect(THINGSBOARD_SERVER, "provision", THINGSBOARD_PORT)) {
      ESP_LOGI(TAG, "Failed to connect");
      return;
    }
  }
  // Connect to the ThingsBoard
  ESP_LOGI(TAG, "Sending provision request to: %s", THINGSBOARD_SERVER);

  String deviceName = String(WIFI_NAME) + "-" + String(serialNumber);

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
          WIFICheckOTA();
        }
      }
      if (tb_wifi.connected() &&
          millis() - Wifi_TB.lastMQTTPublish > WIFI_PUBLISH_INTERVAL) {
        if (!Wifi_TB.firstPublish) {
          Wifi_TB.firstPublish = true;
          addConfigTelemetriesToWIFIJSON();
          if (tb_wifi.sendTelemetryJson(addVariableToTelemetryWIFIJSON,
                                        JSON_STRING_SIZE(measureJson(
                                            addVariableToTelemetryWIFIJSON)))) {
            ESP_LOGI(TAG, "WIFI MQTT PUBLISH CONFIG SUCCESS");
          } else {
            ESP_LOGI(TAG, "WIFI MQTT PUBLISH CONFIG FAIL");
          }
          WIFI_JSON.clear();
        }
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
    }
  } else {
    Wifi_TB.serverConnectionStatus = false;
  }
  tb_wifi.loop();
}

void WifiOTAHandler(void) {
  WIFI_TB_OTA();
  WEB_OTA();
  if (WiFi.status() != 0xff && WiFi.status() != WL_CONNECTED &&
      millis() > 60000) // If no connection in 1 minute, disable WIFI
  {
    wifiDisable();
    ESP_LOGI(TAG, "WIFI DISABLED");
  }
}