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
#include "lwip/dns.h"

#include "GPRS.h"
#include "main.h"

extern GPRSstruct GPRS;
static const char *TAG __attribute__((unused)) = "WiFi";
char wifiHost[32];

WebServer wifiServer(80);

WiFiClient espClient;

// Initalize the Mqtt client instance
Arduino_MQTT_Client mqttClientWIFI(espClient);

// Initialize ThingsBoard instance
// ThingsBoardSized<THINGSBOARD_BUFFER_SIZE, THINGSBOARD_FIELDS_AMOUNT>
// tb_wifi(espClient);
ThingsBoard tb_wifi(mqttClientWIFI, MAX_MESSAGE_SIZE);
StaticJsonDocument<JSON_OBJECT_SIZE(THINGSBOARD_FIELDS_AMOUNT)> WIFI_JSON;
JsonObject addVariableToTelemetryWIFIJSON = WIFI_JSON.to<JsonObject>();

// WIFI
bool WIFI_connection_status = false;

extern in3ator_parameters in3;
extern bool WIFI_EN;

WIFIstruct Wifi_TB;
Credentials wifi_credentials;
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
    "<h3>Firmware Update</h3>"
    "<p>Current Version: <span id='fw_version'></span></p>"
    "<p>CCID: <span id='ccid'></span></p>"
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
    "  $.get('/get_ccid', function(data) {"
    "    $('#ccid').text(data.ccid);"
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

const char *configIndex =
    "<script "
    "src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></"
    "script>"
    "<h3>System Configuration</h3>"
    "<label>Serial Number:</label><br>"
    "<input type='number' name='serial' id='serial'><br><br>"
    "<label>Fan Supply PWM (0-255):</label><br>"
    "<input type='number' name='fan_supply_pwm' id='fan_supply_pwm'><br><br>"
    "<label>Fan Control PWM (0-255):</label><br>"
    "<input type='number' name='fan_ctl_pwm' id='fan_ctl_pwm'><br><br>"
    "<label>Heater Max Power (Amps):</label><br>"
    "<input type='number' step='0.1' name='heater_amps' id='heater_amps'><br><br>"
    "<label>Air Temp Max (C):</label><br>"
    "<input type='number' step='0.1' name='air_tmax' id='air_tmax'><br><br>"
    "<label>Skin Temp Max (C):</label><br>"
    "<input type='number' step='0.1' name='skin_tmax' id='skin_tmax'><br><br>"
    "<button type='button' onclick='saveSection([\"serial\",\"fan_supply_pwm\",\"fan_ctl_pwm\",\"heater_amps\",\"air_tmax\",\"skin_tmax\"])'>Save System Configuration</button>"
    "<br><br>"
    "<h3>GPRS Reporting Periods (seconds)</h3>"
    "<label>Actuating Period:</label><br>"
    "<input type='number' name='gprs_act' id='gprs_act'><br><br>"
    "<label>Phototherapy Period:</label><br>"
    "<input type='number' name='gprs_photo' id='gprs_photo'><br><br>"
    "<label>Standby Period:</label><br>"
    "<input type='number' name='gprs_stby' id='gprs_stby'><br><br>"
    "<button type='button' onclick='saveSection([\"gprs_act\",\"gprs_photo\",\"gprs_stby\"])'>Save GPRS Reporting</button>"
    "<br><br>"
    "<h3>Calibration</h3>"
    "<label>Reference Skin Temp (Current Actual):</label><br>"
    "<input type='number' step='0.01' name='reference_temp' id='reference_temp'><br><br>"
    "<label>Fine Tune Skin Temperature Offset:</label><br>"
    "<input type='number' step='0.01' name='fine_tune' id='fine_tune' readonly><br><br>"
    "<button type='button' onclick='saveSection([\"reference_temp\"])'>Save Calibration</button>"
    "<br><br>"
    "<div id='msg'></div>"
    "<script>"
    "$(document).ready(function() {"
    "  $.get('/get_config', function(data) {"
    "    $('#serial').val(data.serial);"
    "    $('#fan_supply_pwm').val(data.fan_supply_pwm);"
    "    $('#fan_ctl_pwm').val(data.fan_ctl_pwm);"
    "    $('#heater_amps').val(data.heater_amps);"
    "    $('#air_tmax').val(data.air_tmax);"
    "    $('#skin_tmax').val(data.skin_tmax);"
    "    $('#gprs_act').val(data.gprs_act);"
    "    $('#gprs_photo').val(data.gprs_photo);"
    "    $('#gprs_stby').val(data.gprs_stby);"
    "    $('#reference_temp').val(data.skin_temp_val);"
    "    $('#fine_tune').val(data.fine_tune);"
    "  });"
    "});"
    "function saveSection(fields) {"
    "  var data = {};"
    "  fields.forEach(function(f) { data[f] = $('#'+f).val(); });"
    "  $.post('/config', data, function(resp) {"
    "    $('#msg').text(resp);"
    "  });"
    "}"
    "</script>";

/*
   setup function
*/
extern char pendingSSID[64];
extern char pendingPass[64];

static uint32_t lastconnectiontrywifi = 0;

void wifiInit(void) {
  // Connect to WiFi network
  ESP_LOGI(TAG, "Initializing WiFi");
  Wifi_TB.lastWifiReconnectAttempt = millis();

  String hostname = String(WIFI_NAME) + "-" + String(in3.serialNumber);

  // Copy hostname to wifiHost for MDNS
  strncpy(wifiHost, hostname.c_str(), sizeof(wifiHost) - 1);
  wifiHost[sizeof(wifiHost) - 1] = '\0';
  ESP_LOGI(TAG, "Setting hostname to: %s", wifiHost);

  WiFi.setHostname(hostname.c_str());
  WiFi.mode(WIFI_STA);

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
  logI("Connected to " + WiFi.SSID() + " IP address " +
       WiFi.localIP().toString());

  // Pin a fallback DNS so subsequent lookups survive lwIP reinit after reconnects
  ip_addr_t dns_fallback;
  IP4_ADDR(&dns_fallback.u_addr.ip4, 8, 8, 8, 8);
  dns_fallback.type = IPADDR_TYPE_V4;
  dns_setserver(1, &dns_fallback);

  /*use mdns for wifiHost name resolution*/
  if (!MDNS.begin(wifiHost)) { // http://esp32.local
    logI("Error setting up MDNS responder!");
  }
  logI("mDNS responder started");
  /*return index page which is stored in ServerIndex */
  wifiServer.on("/", HTTP_GET, []() {
    wifiServer.sendHeader("Connection", "close");
    wifiServer.send(200, "text/html", loginIndex);
  });
  wifiServer.on("/serverIndex", HTTP_GET, []() {
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
  wifiServer.on("/get_ccid", HTTP_GET, []() {
    String json = "{";
    json += "\"ccid\":\"" + GPRS.CCID + "\"";
    json += "}";
    wifiServer.sendHeader("Connection", "close");
    wifiServer.send(200, "application/json", json);
  });
  /* config page */
  wifiServer.on("/config", HTTP_GET, []() {
    wifiServer.sendHeader("Connection", "close");
    wifiServer.send(200, "text/html", configIndex);
  });
  wifiServer.on("/get_config", HTTP_GET, []() {
    String json = "{";
    json += "\"serial\":" + String(in3.serialNumber) + ",";
    json += "\"fan_supply_pwm\":" + String(in3.fanPWM) + ",";
    json += "\"fan_ctl_pwm\":" + String(in3.fanCtlPWM) + ",";
    json += "\"heater_amps\":" + String(in3.heaterMaxPowerAmps) + ",";
    json += "\"air_tmax\":" + String(in3.airTemperatureSetMax) + ",";
    json += "\"skin_tmax\":" + String(in3.skinTemperatureSetMax) + ",";
    json += "\"gprs_act\":" + String(in3.actuating_gprs_period) + ",";
    json += "\"gprs_photo\":" + String(in3.phototherapy_gprs_period) + ",";
    json += "\"gprs_stby\":" + String(in3.standby_gprs_period) + ",";
    // Return current displayed temperature so user can see what it is or use it
    // as base
    json += "\"skin_temp_val\":" + String(in3.temperature[SKIN_SENSOR]) + ",";
    json += "\"fine_tune\":" + String(in3.fineTuneSkinTemperature);
    json += "}";
    wifiServer.sendHeader("Connection", "close");
    wifiServer.send(200, "application/json", json);
  });
  wifiServer.on("/config", HTTP_POST, []() {
    extern float maxDesiredTemp[2];
    if (wifiServer.hasArg("serial")) {
      in3.serialNumber = wifiServer.arg("serial").toInt();
      EEPROM.writeInt(EEPROM_SERIAL_NUMBER, in3.serialNumber);
    }
    if (wifiServer.hasArg("fan_supply_pwm")) {
      in3.fanPWM = wifiServer.arg("fan_supply_pwm").toInt();
      EEPROM.writeInt(EEPROM_FAN_PWM, in3.fanPWM);
    }
    if (wifiServer.hasArg("fan_ctl_pwm")) {
      in3.fanCtlPWM = wifiServer.arg("fan_ctl_pwm").toInt();
      EEPROM.writeInt(EEPROM_FAN_CTL_PWM, in3.fanCtlPWM);
      ledcWrite(FAN_CTL_PWM_CHANNEL, in3.fanCtlPWM);
    }
    if (wifiServer.hasArg("heater_amps")) {
      in3.heaterMaxPowerAmps = wifiServer.arg("heater_amps").toFloat();
      EEPROM.writeFloat(EEPROM_HEATER_MAX_AMPS, in3.heaterMaxPowerAmps);
    }
    if (wifiServer.hasArg("air_tmax")) {
      in3.airTemperatureSetMax = wifiServer.arg("air_tmax").toFloat();
      maxDesiredTemp[CONTROL_AIR] = in3.airTemperatureSetMax;
      EEPROM.writeFloat(EEPROM_AIR_TEMP_MAX, in3.airTemperatureSetMax);
    }
    if (wifiServer.hasArg("skin_tmax")) {
      in3.skinTemperatureSetMax = wifiServer.arg("skin_tmax").toFloat();
      maxDesiredTemp[CONTROL_SKIN] = in3.skinTemperatureSetMax;
      EEPROM.writeFloat(EEPROM_SKIN_TEMP_MAX, in3.skinTemperatureSetMax);
    }
    if (wifiServer.hasArg("gprs_act")) {
      in3.actuating_gprs_period = wifiServer.arg("gprs_act").toInt();
      EEPROM.writeInt(EEPROM_GPRS_ACT_PERIOD, in3.actuating_gprs_period);
    }
    if (wifiServer.hasArg("gprs_photo")) {
      in3.phototherapy_gprs_period = wifiServer.arg("gprs_photo").toInt();
      EEPROM.writeInt(EEPROM_GPRS_PHOTO_PERIOD, in3.phototherapy_gprs_period);
    }
    if (wifiServer.hasArg("gprs_stby")) {
      in3.standby_gprs_period = wifiServer.arg("gprs_stby").toInt();
      EEPROM.writeInt(EEPROM_GPRS_STBY_PERIOD, in3.standby_gprs_period);
    }
    if (wifiServer.hasArg("reference_temp")) {
      double referenceTemp = wifiServer.arg("reference_temp").toDouble();
      in3.fineTuneSkinTemperature =
          in3.fineTuneSkinTemperature +
          (referenceTemp - in3.temperature[SKIN_SENSOR]);
      EEPROM.writeFloat(EEPROM_FINE_TUNE_TEMP_SKIN,
                        in3.fineTuneSkinTemperature);
    }
    EEPROM.commit();
    wifiServer.sendHeader("Connection", "close");
    wifiServer.send(200, "text/plain", "Saved. Settings applied immediately.");
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
            logI("Update Success: " + String(upload.totalSize) + " bytes");
          } else {
            Update.printError(Serial);
          }
        }
      });
  wifiServer.begin();
}

void WIFI_UpdatedCallback(const bool &success) {
  if (success) {
    logI("[WIFI] -> Done, OTA will be implemented on next boot");
    // esp_restart();
  } else {
    logI("[WIFI] -> No new firmware");
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

void WIFICheckOTA() {
  logI("[WIFI] -> Checking WIFI firwmare Update...");
  tb_wifi.Firmware_Send_Info(CURRENT_FIRMWARE_TITLE, FWversion);
  tb_wifi.Start_Firmware_Update(OTAcallback);
}

void WIFI_TB_Init() {
  Wifi_TB.provisioned = EEPROM.read(EEPROM_THINGSBOARD_PROVISIONED);
  if (Wifi_TB.provisioned == 0xff) { // default EEPROM value
    Wifi_TB.provisioned = false;
  }
  logI("[WIFI] -> WIFI_TB_Init check provisioning: " +
       String(Wifi_TB.provisioned));
  if (Wifi_TB.provisioned) {
    Wifi_TB.device_token = EEPROM.readString(EEPROM_THINGSBOARD_TOKEN);
    logI("[WIFI] -> Provisioned with token: " + String(Wifi_TB.device_token));
  }
}

void WIFIProvisionResponse(const JsonObjectConst &data) {
  logI("[WIFI] -> Received device provision response");
  const size_t jsonSize = JSON_OBJECT_SIZE(data.size()) + 200;
  char buffer[jsonSize];
  serializeJson(data, buffer, jsonSize);

  if (strncmp(data["status"], "SUCCESS", strlen("SUCCESS")) != 0) {
    logI("[WIFI] -> Provision response contains the error: " +
         data["errorMsg"].as<String>());
    return;
  }

  if (strncmp(data[CREDENTIALS_TYPE], ACCESS_TOKEN_CRED_TYPE,
              strlen(ACCESS_TOKEN_CRED_TYPE)) == 0) {

    wifi_credentials.client_id = "";
    wifi_credentials.username = data[CREDENTIALS_VALUE].as<std::string>();
    wifi_credentials.password = "";
    Wifi_TB.provisioned = true;
    Wifi_TB.device_token = wifi_credentials.username.c_str();
    EEPROM.writeString(EEPROM_THINGSBOARD_TOKEN, Wifi_TB.device_token);
    EEPROM.write(EEPROM_THINGSBOARD_PROVISIONED, Wifi_TB.provisioned);
    EEPROM.commit();
    logI("[WIFI] -> Device provisioned successfully");
  } else if (strncmp(data[CREDENTIALS_TYPE], MQTT_BASIC_CRED_TYPE,
                     strlen(MQTT_BASIC_CRED_TYPE)) == 0) {
    auto credentials_value = data[CREDENTIALS_VALUE].as<JsonObjectConst>();
    wifi_credentials.client_id = credentials_value[CLIENT_ID].as<std::string>();
    wifi_credentials.username =
        credentials_value[CLIENT_USERNAME].as<std::string>();
    wifi_credentials.password =
        credentials_value[CLIENT_PASSWORD].as<std::string>();
    Wifi_TB.provisioned = true;
    Wifi_TB.device_token = wifi_credentials.username.c_str();
    EEPROM.writeString(EEPROM_THINGSBOARD_TOKEN, Wifi_TB.device_token);
    EEPROM.write(EEPROM_THINGSBOARD_PROVISIONED, Wifi_TB.provisioned);
    EEPROM.commit();
    logI("[WIFI] -> Device provisioned successfully");
  } else {
    logI("[WIFI] -> Unexpected provision credentialsType");
    return;
  }
  if (tb_wifi.connected()) {
    tb_wifi.disconnect();
  }
  Wifi_TB.provision_request_processed = true;
}

void WIFITBProvision() {
  if (!tb_wifi.connected()) {
    logI("[WIFI] -> Connecting for provision to: " +
         String(THINGSBOARD_SERVER));
    if (!tb_wifi.connect(THINGSBOARD_SERVER, "provision", THINGSBOARD_PORT)) {
      logI("[WIFI] -> Failed to connect");
      return;
    }
  }
  // Connect to the ThingsBoard
  logI("[WIFI] -> Sending provision request to: " + String(THINGSBOARD_SERVER));

  const Provision_Callback provisionCallback(
      Access_Token(), &WIFIProvisionResponse, PROVISION_DEVICE_KEY,
      PROVISION_DEVICE_SECRET, GPRS.CCID.c_str());
  Wifi_TB.provision_request_sent = tb_wifi.Provision_Request(provisionCallback);
}

void switchAlarmTelemetryWIFI(int alarm, bool value) {
  String alarmKey;
  switch (alarm) {
  case HUMIDITY_ALARM:
    alarmKey = HUMIDITY_ALARM_KEY;
    break;
  case TEMPERATURE_ALARM:
    alarmKey = TEMPERATURE_ALARM_KEY;
    break;
  case AIR_THERMAL_CUTOUT_ALARM:
    alarmKey = AIR_THERMAL_CUTOUT_ALARM_KEY;
    break;
  case SKIN_THERMAL_CUTOUT_ALARM:
    alarmKey = SKIN_THERMAL_CUTOUT_ALARM_KEY;
    break;
  case AIR_SENSOR_ISSUE_ALARM:
    alarmKey = AIR_SENSOR_ISSUE_ALARM_KEY;
    break;
  case SKIN_SENSOR_ISSUE_ALARM:
    alarmKey = SKIN_SENSOR_ISSUE_ALARM_KEY;
    break;
  case FAN_ISSUE_ALARM:
    alarmKey = FAN_ISSUE_ALARM_KEY;
    break;
  case HEATER_ISSUE_ALARM:
    alarmKey = HEATER_ISSUE_ALARM_KEY;
    break;
  case POWER_SUPPLY_ALARM:
    alarmKey = POWER_SUPPLY_ALARM_KEY;
    break;
  default:
    return;
  }
  addVariableToTelemetryWIFIJSON[alarmKey] = value;
}

void addAlarmTelemetriesToWIFIJSON() {
  int alarmReported = false;
  for (int i = NO_ALARMS + 1; i < NUM_ALARMS; i++) {
    if (in3.alarmToReport[i]) {
      switchAlarmTelemetryWIFI(i, true);
      alarmReported = true;
      in3.previousAlarmReport = true;
    }
  }
  if (!alarmReported) {
    if (in3.previousAlarmReport) {
      in3.previousAlarmReport = false;
      for (int i = NO_ALARMS + 1; i < NUM_ALARMS; i++) {
        switchAlarmTelemetryWIFI(i, false);
      }
    }
  }
}

void addConfigTelemetriesToWIFIJSON() {
  addAlarmTelemetriesToWIFIJSON();
  addVariableToTelemetryWIFIJSON[SN_KEY] = in3.serialNumber;
  addVariableToTelemetryWIFIJSON[SYSTEM_RESET_REASON] = in3.resetReason;
  addVariableToTelemetryWIFIJSON[HW_NUM_KEY] = HW_NUM;
  addVariableToTelemetryWIFIJSON[HW_REV_KEY] = String(HW_REVISION);
  addVariableToTelemetryWIFIJSON[FW_VERSION_KEY] = FWversion;
  addVariableToTelemetryWIFIJSON[CCID_KEY] = GPRS.CCID.c_str();

  addVariableToTelemetryWIFIJSON[SYS_CURR_STANDBY_TEST_KEY] =
      roundSignificantDigits(in3.system_current_standby_test,
                             TELEMETRIES_DECIMALS);
  addVariableToTelemetryWIFIJSON[HEATER_CURR_TEST_KEY] =
      roundSignificantDigits(in3.heater_current_test, TELEMETRIES_DECIMALS);
  addVariableToTelemetryWIFIJSON[FAN_CURR_TEST_KEY] =
      roundSignificantDigits(in3.fan_current_test, TELEMETRIES_DECIMALS);
  addVariableToTelemetryWIFIJSON[PHOTOTHERAPY_CURR_KEY] =
      roundSignificantDigits(in3.phototherapy_current_test,
                             TELEMETRIES_DECIMALS);
  addVariableToTelemetryWIFIJSON[PHOTOTHERAPY_PWM_KEY] =
      roundSignificantDigits(in3.phototherapy_intensity, TELEMETRIES_DECIMALS);
  addVariableToTelemetryWIFIJSON[HUMIDIFIER_CURR_KEY] =
      roundSignificantDigits(in3.humidifier_current_test, TELEMETRIES_DECIMALS);
  addVariableToTelemetryWIFIJSON[DISPLAY_CURR_TEST_KEY] =
      roundSignificantDigits(in3.display_current_test, TELEMETRIES_DECIMALS);
  addVariableToTelemetryWIFIJSON[BUZZER_CURR_TEST_KEY] =
      roundSignificantDigits(in3.buzzer_current_test, TELEMETRIES_DECIMALS);
  addVariableToTelemetryWIFIJSON[HW_TEST_KEY] = in3.HW_test_error_code;

  addVariableToTelemetryWIFIJSON[UI_LANGUAGE_KEY] = in3.language;
  addVariableToTelemetryWIFIJSON[CALIBRATED_SENSOR_KEY] = !in3.calibrationError;
  addVariableToTelemetryWIFIJSON[GPRS_CONNECTIVITY_KEY] = false;
  addVariableToTelemetryWIFIJSON[WIFI_CONNECTIVITY_KEY] = true;
}

void addTelemetriesToWIFIJSON() {
  addAlarmTelemetriesToWIFIJSON();
  addVariableToTelemetryWIFIJSON[SKIN_CAPACITANCE_KEY] =
      in3.skinSensorCapacitance;
  addVariableToTelemetryWIFIJSON[SKIN_TEMPERATURE_KEY] = roundSignificantDigits(
      in3.temperature[SKIN_SENSOR], TELEMETRIES_DECIMALS);
  addVariableToTelemetryWIFIJSON[AIR_TEMPERATURE_KEY] = roundSignificantDigits(
      in3.temperature[ROOM_DIGITAL_TEMP_SENSOR], TELEMETRIES_DECIMALS);
  if (in3.temperature[AMBIENT_DIGITAL_TEMP_SENSOR] &&
      in3.humidity[AMBIENT_DIGITAL_HUM_SENSOR]) {
    addVariableToTelemetryWIFIJSON[AMBIENT_TEMPERATURE_KEY] =
        roundSignificantDigits(in3.temperature[AMBIENT_DIGITAL_TEMP_SENSOR],
                               TELEMETRIES_DECIMALS);
    addVariableToTelemetryWIFIJSON[HUMIDITY_AMBIENT_KEY] =
        roundSignificantDigits(in3.humidity[AMBIENT_DIGITAL_HUM_SENSOR],
                               TELEMETRIES_DECIMALS);
  }
  addVariableToTelemetryWIFIJSON[PHOTOTHERAPY_ACTIVE_KEY] = in3.phototherapy;
  addVariableToTelemetryWIFIJSON[HUMIDITY_ROOM_KEY] = roundSignificantDigits(
      in3.humidity[ROOM_DIGITAL_HUM_SENSOR], TELEMETRIES_DECIMALS);
  addVariableToTelemetryWIFIJSON[SYSTEM_CURRENT_KEY] =
      roundSignificantDigits(in3.system_current, TELEMETRIES_DECIMALS);
  addVariableToTelemetryWIFIJSON[SYSTEM_VOLTAGE_KEY] =
      roundSignificantDigits(in3.system_voltage, TELEMETRIES_DECIMALS);
  addVariableToTelemetryWIFIJSON[V5_CURRENT_KEY] =
      roundSignificantDigits(in3.USB_current, TELEMETRIES_DECIMALS);
  addVariableToTelemetryWIFIJSON[V5_VOLTAGE_KEY] =
      roundSignificantDigits(in3.USB_voltage, TELEMETRIES_DECIMALS);
  addVariableToTelemetryWIFIJSON[BAT_CURRENT_KEY] =
      roundSignificantDigits(in3.BATTERY_current, TELEMETRIES_DECIMALS);
  addVariableToTelemetryWIFIJSON[BAT_VOLTAGE_KEY] =
      roundSignificantDigits(in3.BATTERY_voltage, TELEMETRIES_DECIMALS);

  if (in3.temperatureControl || in3.humidityControl) {
    addVariableToTelemetryWIFIJSON[FAN_CURRENT_KEY] =
        roundSignificantDigits(in3.fan_current, TELEMETRIES_DECIMALS);
    addVariableToTelemetryWIFIJSON[CONTROL_ACTIVE_TIME_KEY] =
        roundSignificantDigits(in3.control_active_time, TELEMETRIES_DECIMALS);
    addVariableToTelemetryWIFIJSON[FAN_ACTIVE_TIME_KEY] =
        roundSignificantDigits(in3.fan_active_time, TELEMETRIES_DECIMALS);
    if (in3.temperatureControl) {
      addVariableToTelemetryWIFIJSON[HEATER_CURRENT_KEY] =
          roundSignificantDigits(in3.heater_current, TELEMETRIES_DECIMALS);
      addVariableToTelemetryWIFIJSON[DESIRED_TEMPERATURE_KEY] =
          in3.desiredControlTemperature;
      addVariableToTelemetryWIFIJSON[HEATER_ACTIVE_TIME_KEY] =
          roundSignificantDigits(in3.heater_active_time, TELEMETRIES_DECIMALS);
    }
    if (in3.humidityControl) {
      addVariableToTelemetryWIFIJSON[DESIRED_HUMIDITY_ROOM_KEY] =
          in3.desiredControlHumidity;
    }
    if (!Wifi_TB.firstConfigPost) {
      Wifi_TB.firstConfigPost = true;
      addVariableToTelemetryWIFIJSON[CONTROL_ACTIVE_KEY] = true;
      if (in3.temperatureControl) {
        if (in3.controlMode == CONTROL_AIR) {
          addVariableToTelemetryWIFIJSON[CONTROL_MODE_KEY] = "AIR";
        } else {
          addVariableToTelemetryWIFIJSON[CONTROL_MODE_KEY] = "SKIN";
        }
      }
    }
  } else {
    Wifi_TB.firstConfigPost = false;
    addVariableToTelemetryWIFIJSON[CONTROL_ACTIVE_KEY] = false;
    addVariableToTelemetryWIFIJSON[STANBY_TIME_KEY] =
        roundSignificantDigits(in3.standby_time, TELEMETRIES_DECIMALS);
  }
  if (in3.humidityControl) {
    addVariableToTelemetryWIFIJSON[HUMIDIFIER_CURRENT_KEY] =
        roundSignificantDigits(in3.humidifier_current, TELEMETRIES_DECIMALS);
    addVariableToTelemetryWIFIJSON[HUMIDIFIER_VOLTAGE_KEY] =
        roundSignificantDigits(in3.humidifier_voltage, TELEMETRIES_DECIMALS);
    addVariableToTelemetryWIFIJSON[HUMIDIFIER_ACTIVE_TIME_KEY] =
        roundSignificantDigits(in3.humidifier_active_time,
                               TELEMETRIES_DECIMALS);
  }
  if (in3.phototherapy) {
    addVariableToTelemetryWIFIJSON[PHOTOTHERAPY_CURRENT_KEY] =
        roundSignificantDigits(in3.phototherapy_current, TELEMETRIES_DECIMALS);
    addVariableToTelemetryWIFIJSON[PHOTHERAPY_ACTIVE_TIME_KEY] =
        roundSignificantDigits(in3.phototherapy_active_time,
                               TELEMETRIES_DECIMALS);
  }
}

void WEB_OTA() {
  if (WiFi.status() == WL_CONNECTED) {
    if (strlen(pendingSSID) > 0 && WiFi.SSID() == String(pendingSSID)) {
      logI("[WIFI] -> Connection successful, persisting credentials to EEPROM");
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

void WIFI_TB_OTA() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!Wifi_TB.provisioned) {
      if (GPRS.CCID.length() > 0) {
        if (!Wifi_TB.provision_request_sent) {
          WIFITBProvision();
        }
      }
    } else {
      if (!tb_wifi.connected()) {
        if (millis() - Wifi_TB.lastReconnectAttempt <
            THINGSBOARD_RECONNECT_DELAY) {
          return;
        }
        // Connect to the ThingsBoard
        logI(
            "[WIFI] -> Connecting over WIFI to: " + String(THINGSBOARD_SERVER) +
            " with token " + String(Wifi_TB.device_token));
        Wifi_TB.lastReconnectAttempt = millis();
        if (!tb_wifi.connect(THINGSBOARD_SERVER,
                             Wifi_TB.device_token.c_str())) {
          logI("[WIFI] ->Failed to connect");
          return;
        } else {
          if (!Wifi_TB.firstPublish) {
            addConfigTelemetriesToWIFIJSON();
            if (tb_wifi.sendTelemetryJson(
                    addVariableToTelemetryWIFIJSON,
                    JSON_STRING_SIZE(
                        measureJson(addVariableToTelemetryWIFIJSON)))) {
              logI("[WIFI] -> WIFI MQTT PUBLISH CONFIG SUCCESS");
            } else {
              logI("[WIFI] -> WIFI MQTT PUBLISH CONFIG FAIL");
            }
            WIFI_JSON.clear();
          }
          Wifi_TB.serverConnectionStatus = true;
          if (ENABLE_WIFI_OTA && !Wifi_TB.OTA_requested) {
            Wifi_TB.OTA_requested = true;
          }
          WIFICheckOTA();
          Wifi_TB.lastOTACheck = millis();
        }
      } else {
        if (millis() - Wifi_TB.lastMQTTPublish > WIFI_PUBLISH_INTERVAL) {
          addTelemetriesToWIFIJSON();
          if (tb_wifi.sendTelemetryJson(addVariableToTelemetryWIFIJSON,
                                        JSON_STRING_SIZE(measureJson(
                                            addVariableToTelemetryWIFIJSON)))) {
            logI("[WIFI] -> WIFI MQTT PUBLISH TELEMETRIES SUCCESS");
          } else {
            logI("[WIFI] -> WIFI MQTT PUBLISH TELEMETRIES FAIL");
          }
          WIFI_JSON.clear();
          Wifi_TB.lastMQTTPublish = millis();
        }
        if (millis() - Wifi_TB.lastOTACheck > WIFI_OTA_CHECK_INTERVAL &&
            !GPRS.OTAInProgress) {
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
  // static long lastLog = 0;
  // if (millis() - lastLog > 5000) {
  //   ESP_LOGI(TAG, "WifiOTAHandler alive. WiFi status: %d. IP: %s",
  //            WiFi.status(), WiFi.localIP().toString().c_str());
  //   lastLog = millis();
  // }

  if (WIFI_EN && WiFi.status() != WL_CONNECTED) {
    if (millis() - Wifi_TB.lastWifiReconnectAttempt > WIFI_RECONNECT_INTERVAL) {
      Wifi_TB.lastWifiReconnectAttempt = millis();
      logI("[WIFI] -> Connection lost, attempting to reconnect...");
      MDNS.end();
      WiFi.reconnect();
    }
  }

  WIFI_TB_OTA();
  WEB_OTA();
}
