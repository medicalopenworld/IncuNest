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
#include <time.h>
#include "lwip/dns.h"

#include "CommTask.h"
#include "GPRS.h"
#include "SPO2.h"
#include "PpgSnapshot.h"
#include "PpgSnapshotPublish.h"
#include "main.h"
#include "modules/util/tz_source.h"
#include "modules/baby_profile/baby_cloud.h"
#include "modules/baby_profile/baby_profile_store.h"
#include "modules/util/civil_time.h"
#include "modules/util/system_clock.h"
#include "alarm_policy.h"

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

extern IncuNest_parameters in3;
extern PID fanControlPID;
extern double fanControlPIDOutput;
extern bool WIFI_EN;
extern char pendingSSID[64];
extern char pendingPass[64];

WIFIstruct Wifi_TB;
Credentials wifi_credentials;
Espressif_Updater updater_WIFI;

const OTA_Update_Callback OTAcallback(&progressCallback, &updatedCallback,
                                      CURRENT_FIRMWARE_TITLE, FWversion,
                                      &updater_WIFI, FIRMWARE_FAILURE_RETRIES,
                                      FIRMWARE_PACKET_SIZE,
                                      WAIT_FAILED_OTA_CHUNKS);

// ---------------------------------------------------------------------------
// Estado real del enlace WiFi.
//
// WiFi.status() NO sirve para esto. En arduino-esp32 2.0.14 (el core que fija
// espressif32@6.6.0) el manejador de STA_DISCONNECTED tiene una rama vacía
// para reason=2 (AUTH_EXPIRE, WiFiGeneric.cpp:1068): a diferencia del resto de
// motivos, no llama a _setStatus(), así que _sta_status se queda en
// WL_CONNECTED aunque la STA ya esté desasociada y sin IP. WL_CONNECTED solo
// se vuelve a escribir en STA_GOT_IP (WiFiGeneric.cpp:1104), y el
// auto-reconnect del core —único camino que lo corregiría— está desactivado a
// propósito en wifiInit() por el event storm de ASSOC_TOOMANY.
//
// Resultado observado en banco: WiFi.status() miente para siempre,
// WifiOTAHandler() nunca reintenta wifiInit() (su guarda cree que hay enlace)
// y WIFI_TB_OTA() sigue llamando a tb_wifi.connect() cada
// THINGSBOARD_RECONNECT_DELAY, que falla en hostByName() por no haber IP. La
// placa queda sin WiFi indefinidamente y solo se recupera metiendo
// credenciales a mano.
//
// No se arregla subiendo el core: la misma rama vacía sigue en 2.0.16
// (WiFiGeneric.cpp:1069) y en 3.3.7 (STA.cpp:144). Por eso la comprobación se
// apoya en eventos, que son estables entre versiones.
//
// s_staHasIp lo mantienen los propios manejadores de eventos: es la única
// fuente que refleja DISCONNECTED/GOT_IP sin pasar por el estado corrupto.
static volatile bool s_staHasIp = false;

// Registro idempotente de los manejadores de eventos. Vive aparte de
// wifiInit() porque applyWifiCredentials() también depende de s_staHasIp y
// puede ejecutarse antes (RPC setWifi): sin los manejadores puestos, la espera
// de 15 s expiraría siempre y revertiría unas credenciales válidas.
static void wifiRegisterEvents(void) {
  static bool s_wifiEventsRegistered = false;
  if (s_wifiEventsRegistered)
    return;

  WiFi.onEvent([](WiFiEvent_t, WiFiEventInfo_t info) {
    s_staHasIp = false;
    ESP_LOGW(TAG, "STA_DISCONNECTED reason=%d",
             info.wifi_sta_disconnected.reason);
  }, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

  WiFi.onEvent([](WiFiEvent_t, WiFiEventInfo_t) {
    s_staHasIp = true;
    ESP_LOGI(TAG, "STA_GOT_IP: %s", WiFi.localIP().toString().c_str());
  }, ARDUINO_EVENT_WIFI_STA_GOT_IP);

  s_wifiEventsRegistered = true;
}

void applyWifiCredentials(const char* ssid, const char* pass) {
  wifiRegisterEvents();

  Preferences prefs;
  char prevSSID[64] = "";
  char prevPass[64] = "";
  prefs.begin("mb_wifi", true);
  prefs.getString("ssid", prevSSID, sizeof(prevSSID));
  prefs.getString("password", prevPass, sizeof(prevPass));
  prefs.end();

  prefs.begin("mb_wifi", false);
  prefs.putString("ssid", ssid);
  prefs.putString("password", pass);
  prefs.end();

  WiFi.disconnect();
  WiFi.begin(ssid, pass);

  uint32_t start = millis();
  while (!WIFIIsConnected() && millis() - start < 15000) {
    vTaskDelay(pdMS_TO_TICKS(500));
  }

  if (WIFIIsConnected()) {
    sendWifiToHMI(ssid, pass);
  } else {
    prefs.begin("mb_wifi", false);
    prefs.putString("ssid", prevSSID);
    prefs.putString("password", prevPass);
    prefs.end();
    WiFi.begin(prevSSID, prevPass);
    pendingSSID[0] = '\0';
    pendingPass[0] = '\0';
  }
}

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
    "<label>Fan Speed PID:</label><br>"
    "<select name='fan_pid_en' id='fan_pid_en'>"
    "<option value='1'>Enabled</option>"
    "<option value='0'>Disabled (fixed Fan Control PWM)</option>"
    "</select><br><br>"
    "<label>Heater Max Power (Amps):</label><br>"
    "<input type='number' step='0.1' name='heater_amps' id='heater_amps'><br><br>"
    "<label>Air Temp Max (C):</label><br>"
    "<input type='number' step='0.1' name='air_tmax' id='air_tmax'><br><br>"
    "<label>Skin Temp Max (C):</label><br>"
    "<input type='number' step='0.1' name='skin_tmax' id='skin_tmax'><br><br>"
    "<button type='button' onclick='saveSection([\"serial\",\"fan_supply_pwm\",\"fan_ctl_pwm\",\"fan_pid_en\",\"heater_amps\",\"air_tmax\",\"skin_tmax\"])'>Save System Configuration</button>"
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
    "<h3>Date &amp; Time</h3>"
    "<p>Incubator clock: <b><span id='dev_time'>--</span></b> <span id='dev_time_src'></span></p>"
    "<label>Set clock:</label><br>"
    "<input type='datetime-local' step='1' name='set_time' id='set_time'><br><br>"
    "<button type='button' onclick='fillBrowserTime()'>Use this device's time</button> "
    "<button type='button' onclick='setDeviceTime()'>Set Date &amp; Time</button>"
    "<p><small>The incubator keeps a single clock with no time zone: what you"
    " enter here is exactly what the screen, the baby's age and the alarm log"
    " will show. A manual entry overrides WiFi/GPRS time sync until the next"
    " reboot; after a power cycle the clock is lost and sync takes over"
    " again.</small></p>"
    "<br>"
    "<div id='msg'></div>"
    "<script>"
    "$(document).ready(function() {"
    "  $.get('/get_config', function(data) {"
    "    $('#serial').val(data.serial);"
    "    $('#fan_supply_pwm').val(data.fan_supply_pwm);"
    "    $('#fan_ctl_pwm').val(data.fan_ctl_pwm);"
    "    $('#fan_pid_en').val(data.fan_pid_en);"
    "    $('#heater_amps').val(data.heater_amps);"
    "    $('#air_tmax').val(data.air_tmax);"
    "    $('#skin_tmax').val(data.skin_tmax);"
    "    $('#gprs_act').val(data.gprs_act);"
    "    $('#gprs_photo').val(data.gprs_photo);"
    "    $('#gprs_stby').val(data.gprs_stby);"
    "    $('#reference_temp').val(data.skin_temp_val);"
    "    $('#fine_tune').val(data.fine_tune);"
    "    showDeviceTime(data);"
    "    if (data.time_epoch > 0) { $('#set_time').val(stamp(data.time_epoch)); }"
    "    else { fillBrowserTime(); }"
    "  });"
    "});"
    "function saveSection(fields) {"
    "  var data = {};"
    "  fields.forEach(function(f) { data[f] = $('#'+f).val(); });"
    "  $.post('/config', data, function(resp) {"
    "    $('#msg').text(resp);"
    "  });"
    "}"
    "function pad(n) { return (n < 10 ? '0' : '') + n; }"
    // The board clock has no time zone, so render the epoch as-is (UTC fields)
    // instead of shifting it into the browser's zone: what is shown here must
    // be what the incubator screen shows.
    "function stamp(epoch) {"
    "  var d = new Date(epoch * 1000);"
    "  return d.getUTCFullYear() + '-' + pad(d.getUTCMonth()+1) + '-' + pad(d.getUTCDate())"
    "    + 'T' + pad(d.getUTCHours()) + ':' + pad(d.getUTCMinutes()) + ':' + pad(d.getUTCSeconds());"
    "}"
    "function showDeviceTime(data) {"
    "  if (data.time_epoch > 0) {"
    "    $('#dev_time').text(stamp(data.time_epoch).replace('T', ' '));"
    "    $('#dev_time_src').text(data.time_manual == 1 ? '(set manually)' : '(synced from WiFi/GPRS)');"
    "  } else {"
    "    $('#dev_time').text('not set');"
    "    $('#dev_time_src').text('(no WiFi/GPRS time sync yet)');"
    "  }"
    "}"
    "function fillBrowserTime() {"
    "  var d = new Date();"
    "  $('#set_time').val(d.getFullYear() + '-' + pad(d.getMonth()+1) + '-' + pad(d.getDate())"
    "    + 'T' + pad(d.getHours()) + ':' + pad(d.getMinutes()) + ':' + pad(d.getSeconds()));"
    "}"
    "function setDeviceTime() {"
    "  $.post('/config', { set_time: $('#set_time').val() }, function(resp) {"
    "    $('#msg').text(resp);"
    "    $.get('/get_config', showDeviceTime);"
    "  }).fail(function(x) { $('#msg').text(x.responseText || 'Error setting the clock'); });"
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

  // In Arduino 3.x (ESP-IDF 5.x), setHostname must be called BEFORE mode(WIFI_STA).
  WiFi.setHostname(hostname.c_str());
  // Skip mode change if already STA — calling mode() while connecting causes ESP_ERR_WIFI_CONN.
  if (WiFi.getMode() != WIFI_MODE_STA) {
    WiFi.mode(WIFI_STA);
  }

  // Register disconnect/got-IP handlers once so reason codes land in the log
  // and s_staHasIp refleje el enlace real (ver wifiRegisterEvents()).
  wifiRegisterEvents();

  WiFi.persistent(true);
  // Manual reconnect via WifiOTAHandler() at WIFI_RECONNECT_INTERVAL.
  // Auto-reconnect has no backoff for ASSOC_TOOMANY (reason=5) and causes a
  // 25 Hz event storm that starves other tasks and flickers the display.
  WiFi.setAutoReconnect(false);

  String ssid;
  String pass;

  if (strlen(pendingSSID) > 0) {
    ssid = pendingSSID;
    pass = pendingPass;
    ESP_LOGI(TAG, "Connecting to pending SSID: %s", ssid.c_str());
  } else {
    { Preferences p; p.begin(NS_WIFI, true);
      ssid = p.getString(KEY_SSID,     "");
      pass = p.getString(KEY_PASSWORD, "");
      p.end(); }
    if (ssid.length() > 0) {
      ESP_LOGI(TAG, "Connecting to SSID from Preferences: %s", ssid.c_str());
    } else {
      ESP_LOGI(TAG, "Connecting to default SSID: %s", WIFI_SSID);
      ssid = WIFI_SSID;
      pass = WIFI_PASSWORD;
    }
  }

  WiFi.begin(ssid.c_str(), pass.c_str());
  // Mobile hotspots often drop power-saving clients. Disable modem sleep.
  WiFi.setSleep(WIFI_PS_NONE);
  lastconnectiontrywifi = millis();
}

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
  MDNS.addService("http", "tcp", 80);
  logI("mDNS responder started");
  /*return index page which is stored in ServerIndex */
  wifiServer.on("/", HTTP_GET, []() {
    wifiServer.sendHeader("Connection", "close");
    wifiServer.send(200, "text/html", loginIndex);
  });
  wifiServer.on("/serverIndex", HTTP_GET, []() {
    if (!wifiServer.authenticate(WEB_SERVER_USERNAME, WEB_SERVER_PASSWORD)) {
      return wifiServer.requestAuthentication();
    }
    wifiServer.sendHeader("Connection", "close");
    wifiServer.send(200, "text/html", serverIndex);
  });
  wifiServer.on("/get_fw_version", HTTP_GET, []() {
    String json = "{";
    json += "\"version\":\"" + String(FWversion) + "\"";
    json += ",\"sn\":" + String(in3.serialNumber);
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
    if (!wifiServer.authenticate(WEB_SERVER_USERNAME, WEB_SERVER_PASSWORD)) {
      return wifiServer.requestAuthentication();
    }
    wifiServer.sendHeader("Connection", "close");
    wifiServer.send(200, "text/html", configIndex);
  });
  wifiServer.on("/get_config", HTTP_GET, []() {
    String json = "{";
    json += "\"serial\":" + String(in3.serialNumber) + ",";
    json += "\"fan_supply_pwm\":" + String(in3.fanPwrSupplyPWM) + ",";
    json += "\"fan_ctl_pwm\":" + String(in3.fanCtlPWM) + ",";
    json += "\"fan_pid_en\":" + String(in3.fanPidEnabled) + ",";
    json += "\"heater_amps\":" + String(in3.heaterMaxPowerAmps) + ",";
    json += "\"air_tmax\":" + String(in3.airTemperatureSetMax) + ",";
    json += "\"skin_tmax\":" + String(in3.skinTemperatureSetMax) + ",";
    json += "\"gprs_act\":" + String(in3.actuating_gprs_period) + ",";
    json += "\"gprs_photo\":" + String(in3.phototherapy_gprs_period) + ",";
    json += "\"gprs_stby\":" + String(in3.standby_gprs_period) + ",";
    // Return current displayed temperature so user can see what it is or use it
    // as base
    json += "\"skin_temp_val\":" + String(in3.temperature[SKIN_SENSOR]) + ",";
    json += "\"fine_tune\":" + String(in3.fineTuneSkinTemperature) + ",";
    // Wall clock. 0 means "never synced and never set" — same floor the rest
    // of the firmware uses to decide the date cannot be trusted.
    json += "\"time_epoch\":" + String((unsigned long)babyStore_nowEpoch()) + ",";
    json += "\"time_manual\":" + String(systemClockIsManual() ? 1 : 0);
    json += "}";
    wifiServer.sendHeader("Connection", "close");
    wifiServer.send(200, "application/json", json);
  });
  wifiServer.on("/config", HTTP_POST, []() {
    if (!wifiServer.authenticate(WEB_SERVER_USERNAME, WEB_SERVER_PASSWORD)) {
      return wifiServer.requestAuthentication();
    }
    extern float maxDesiredTemp[2];
    if (wifiServer.hasArg("serial")) {
      in3.serialNumber = wifiServer.arg("serial").toInt();
      { Preferences p; p.begin(NS_CFG, false); p.putInt(KEY_SERIAL, in3.serialNumber); p.end(); }
    }
    if (wifiServer.hasArg("fan_supply_pwm")) {
      in3.fanPwrSupplyPWM = wifiServer.arg("fan_supply_pwm").toInt();
      { Preferences p; p.begin(NS_CFG, false); p.putInt(KEY_FAN_PWR_SUPPLY_PWM, in3.fanPwrSupplyPWM); p.end(); }
    }
    if (wifiServer.hasArg("fan_ctl_pwm")) {
      in3.fanCtlPWM = wifiServer.arg("fan_ctl_pwm").toInt();
      { Preferences p; p.begin(NS_CFG, false); p.putInt(KEY_FAN_CTL_PWM, in3.fanCtlPWM); p.end(); }
      ledcWrite(FAN_CTL_PWM_CHANNEL, in3.fanCtlPWM);
    }
    if (wifiServer.hasArg("fan_pid_en")) {
      setFanPidEnabled(wifiServer.arg("fan_pid_en").toInt() != 0);
    }
    if (wifiServer.hasArg("heater_amps")) {
      in3.heaterMaxPowerAmps = wifiServer.arg("heater_amps").toFloat();
      { Preferences p; p.begin(NS_CFG, false); p.putFloat(KEY_HEAT_MAX_A, in3.heaterMaxPowerAmps); p.end(); }
    }
    if (wifiServer.hasArg("air_tmax")) {
      in3.airTemperatureSetMax =
          alarm_clamp_air_cutout(wifiServer.arg("air_tmax").toFloat());
      maxDesiredTemp[CONTROL_AIR] = in3.airTemperatureSetMax;
      { Preferences p; p.begin(NS_CFG, false); p.putFloat(KEY_AIR_T_MAX, in3.airTemperatureSetMax); p.end(); }
    }
    if (wifiServer.hasArg("skin_tmax")) {
      in3.skinTemperatureSetMax =
          alarm_clamp_skin_cutout(wifiServer.arg("skin_tmax").toFloat());
      maxDesiredTemp[CONTROL_SKIN] = in3.skinTemperatureSetMax;
      { Preferences p; p.begin(NS_CFG, false); p.putFloat(KEY_SKIN_T_MAX, in3.skinTemperatureSetMax); p.end(); }
    }
    if (wifiServer.hasArg("gprs_act")) {
      in3.actuating_gprs_period = wifiServer.arg("gprs_act").toInt();
      { Preferences p; p.begin(NS_GPRS, false); p.putInt(KEY_ACT_PERIOD, in3.actuating_gprs_period); p.end(); }
    }
    if (wifiServer.hasArg("gprs_photo")) {
      in3.phototherapy_gprs_period = wifiServer.arg("gprs_photo").toInt();
      { Preferences p; p.begin(NS_GPRS, false); p.putInt(KEY_PHOTO_PERIOD, in3.phototherapy_gprs_period); p.end(); }
    }
    if (wifiServer.hasArg("gprs_stby")) {
      in3.standby_gprs_period = wifiServer.arg("gprs_stby").toInt();
      { Preferences p; p.begin(NS_GPRS, false); p.putInt(KEY_STBY_PERIOD, in3.standby_gprs_period); p.end(); }
    }
    if (wifiServer.hasArg("reference_temp")) {
      double referenceTemp = wifiServer.arg("reference_temp").toDouble();
      in3.fineTuneSkinTemperature =
          in3.fineTuneSkinTemperature +
          (referenceTemp - in3.temperature[SKIN_SENSOR]);
      { Preferences p; p.begin(NS_CAL, false); p.putFloat(KEY_FT_SKIN, in3.fineTuneSkinTemperature); p.end(); }
    }
    if (wifiServer.hasArg("set_time")) {
      // "YYYY-MM-DDTHH:MM[:SS]" — what <input type='datetime-local'> posts.
      // Taken as the board clock verbatim (tz offset 0): the firmware keeps a
      // single timezone-less clock, so what the operator types is what the
      // screen, the baby's age and the alarm log show. Deployments with
      // neither WiFi nor a NITZ-capable operator had no other way in.
      String v = wifiServer.arg("set_time");
      int y = 0, mo = 0, d = 0, h = 0, mi = 0, s = 0;
      uint32_t epoch = 0;
      // %*c swallows the 'T' (or a space, for a hand-crafted curl POST).
      int n = sscanf(v.c_str(), "%d-%d-%d%*c%d:%d:%d", &y, &mo, &d, &h, &mi, &s);
      if (n < 5 ||
          !civil_to_unix_utc(y, (unsigned)mo, (unsigned)d, (unsigned)h,
                             (unsigned)mi, (unsigned)s, 0, &epoch) ||
          !systemClockSetManual(epoch)) {
        wifiServer.sendHeader("Connection", "close");
        wifiServer.send(400, "text/plain",
                        "Invalid date/time. Expected YYYY-MM-DDTHH:MM[:SS], "
                        "year 2021 or later.");
        return;
      }
      logI("[WIFI] -> clock set manually to epoch " + String(epoch));
      wifiServer.sendHeader("Connection", "close");
      wifiServer.send(200, "text/plain",
                      "Clock set. The display picks it up within 10 s.");
      return;
    }
    /* Preferences commits on p.end() — no explicit commit needed */
    wifiServer.sendHeader("Connection", "close");
    wifiServer.send(200, "text/plain", "Saved. Settings applied immediately.");
  });
  /*handling uploading firmware file */
  wifiServer.on(
      "/update", HTTP_POST,
      []() {
        if (!wifiServer.authenticate(WEB_SERVER_USERNAME, WEB_SERVER_PASSWORD)) {
          return wifiServer.requestAuthentication();
        }
        wifiServer.sendHeader("Connection", "close");
        wifiServer.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
        delay(500); // let TCP stack flush the response before hardware reset
        ESP.restart();
      },
      []() {
        if (!wifiServer.authenticate(WEB_SERVER_USERNAME, WEB_SERVER_PASSWORD)) return;
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
  bool WifiStatus = WIFIIsConnected();
  bool serverConnectionStatus = WIFIIsConnectedToServer();
  if (serverConnectionStatus != Wifi_TB.lastServerConnectionStatus ||
      WifiStatus != Wifi_TB.lastWIFIConnectionStatus) {
    retVal = true;
  }
  Wifi_TB.lastWIFIConnectionStatus = WifiStatus;
  Wifi_TB.lastServerConnectionStatus = serverConnectionStatus;
  return (retVal);
}

// Enlace WiFi real: los tres criterios deben coincidir. s_staHasIp es el que
// sobrevive al bug de AUTH_EXPIRE del core (ver su declaración); WiFi.status()
// se mantiene para no ser nunca menos estricto que antes, y localIP() es el
// mismo criterio que ya usa driveHostReachable() en DriveUpload.cpp.
bool WIFIIsConnected() {
  return (s_staHasIp && WiFi.status() == WL_CONNECTED &&
          WiFi.localIP() != IPAddress(0, 0, 0, 0));
}

bool WIFIIsConnectedToServer() {
  return (Wifi_TB.serverConnectionStatus && WIFIIsConnected());
}

void WIFICheckOTA() {
  logI("[WIFI] -> Checking WIFI firwmare Update...");
  tb_wifi.Firmware_Send_Info(CURRENT_FIRMWARE_TITLE, FWversion);
  tb_wifi.Start_Firmware_Update(OTAcallback);
}

void WIFI_TB_Init() {
  { Preferences p; p.begin(NS_GPRS, true);
    Wifi_TB.provisioned   = p.getUChar (KEY_PROVISIONED, 0);
    if (Wifi_TB.provisioned) {
      Wifi_TB.device_token = p.getString(KEY_TOKEN, "").c_str();
    }
    p.end(); }
  logI("[WIFI] -> WIFI_TB_Init check provisioning: " +
       String(Wifi_TB.provisioned));
  if (Wifi_TB.provisioned) {
    logI("[WIFI] -> Provisioned with token: " + String(Wifi_TB.device_token));
  }
}

void WIFIProvisionResponse(const JsonObjectConst &data) {
  logI("[WIFI] -> Received device provision response");
  const size_t jsonSize = JSON_OBJECT_SIZE(data.size()) + 200;
  char buffer[jsonSize];
  serializeJson(data, buffer, jsonSize);

  if (strncmp(data["status"], "SUCCESS", strlen("SUCCESS")) != 0) {
    Wifi_TB.provision_retry_count++;
    if (Wifi_TB.provision_retry_count <= PROVISION_MAX_RETRIES) {
      logI("[WIFI] -> Provision failed: " + data["errorMsg"].as<String>() +
           " - retrying as IncuNest-" + String(in3.serialNumber) + "_" + String(Wifi_TB.provision_retry_count));
      Wifi_TB.provision_request_sent = false;
    } else {
      logI("[WIFI] -> Provision failed after max retries, giving up");
    }
    return;
  }

  if (strncmp(data[CREDENTIALS_TYPE], ACCESS_TOKEN_CRED_TYPE,
              strlen(ACCESS_TOKEN_CRED_TYPE)) == 0) {

    wifi_credentials.client_id = "";
    wifi_credentials.username = data[CREDENTIALS_VALUE].as<std::string>();
    wifi_credentials.password = "";
    Wifi_TB.provisioned = true;
    Wifi_TB.device_token = wifi_credentials.username.c_str();
    { Preferences p; p.begin(NS_GPRS, false);
      p.putString(KEY_TOKEN,       Wifi_TB.device_token);
      p.putUChar (KEY_PROVISIONED, Wifi_TB.provisioned);
      p.end(); }
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
    { Preferences p; p.begin(NS_GPRS, false);
      p.putString(KEY_TOKEN,       Wifi_TB.device_token);
      p.putUChar (KEY_PROVISIONED, Wifi_TB.provisioned);
      p.end(); }
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

  String baseName = "IncuNest-" + String(in3.serialNumber);
  String deviceName = (Wifi_TB.provision_retry_count == 0)
      ? baseName
      : baseName + "_" + String(Wifi_TB.provision_retry_count);
  logI("[WIFI] -> Provisioning as: " + deviceName);
  const Provision_Callback provisionCallback(
      Access_Token(), &WIFIProvisionResponse, PROVISION_DEVICE_KEY,
      PROVISION_DEVICE_SECRET, deviceName.c_str());
  Wifi_TB.provision_request_sent = tb_wifi.Provision_Request(provisionCallback);
}

void switchAlarmTelemetryWIFI(int alarm, bool value) {
  String alarmKey;
  switch (alarm) {
  case ALARM_AIR_THERMAL_CUTOUT:
    alarmKey = ALARM_AIR_THERMAL_CUTOUT_KEY;
    break;
  case ALARM_SKIN_THERMAL_CUTOUT:
    alarmKey = ALARM_SKIN_THERMAL_CUTOUT_KEY;
    break;
  case ALARM_AIR_SENSOR_FAULT:
    alarmKey = ALARM_AIR_SENSOR_FAULT_KEY;
    break;
  case ALARM_SKIN_SENSOR_FAULT_SKIN_MODE:
    alarmKey = ALARM_SKIN_SENSOR_FAULT_SKIN_MODE_KEY;
    break;
  case ALARM_FAN_FAILURE:
    alarmKey = ALARM_FAN_FAILURE_KEY;
    break;
  case ALARM_AIR_OUTLET_BLOCKED:
    alarmKey = ALARM_AIR_OUTLET_BLOCKED_KEY;
    break;
  case ALARM_MAINS_INTERRUPTION:
    alarmKey = ALARM_MAINS_INTERRUPTION_KEY;
    break;
  case ALARM_AIR_TEMP_DEVIATION_HIGH:
    alarmKey = ALARM_AIR_TEMP_DEVIATION_HIGH_KEY;
    break;
  case ALARM_AIR_TEMP_DEVIATION_LOW:
    alarmKey = ALARM_AIR_TEMP_DEVIATION_LOW_KEY;
    break;
  case ALARM_SKIN_TEMP_DEVIATION_HIGH:
    alarmKey = ALARM_SKIN_TEMP_DEVIATION_HIGH_KEY;
    break;
  case ALARM_SKIN_TEMP_DEVIATION_LOW:
    alarmKey = ALARM_SKIN_TEMP_DEVIATION_LOW_KEY;
    break;
  case ALARM_HEATER_FAULT:
    alarmKey = ALARM_HEATER_FAULT_KEY;
    break;
  case ALARM_SUPPLY_UNDERVOLTAGE:
    alarmKey = ALARM_SUPPLY_UNDERVOLTAGE_KEY;
    break;
  case ALARM_HMI_LINK_LOST:
    alarmKey = ALARM_HMI_LINK_LOST_KEY;
    break;
  case ALARM_SKIN_SENSOR_FAULT_AIR_MODE:
    alarmKey = ALARM_SKIN_SENSOR_FAULT_AIR_MODE_KEY;
    break;
  case ALARM_HUMIDITY_DEVIATION:
    alarmKey = ALARM_HUMIDITY_DEVIATION_KEY;
    break;
  case ALARM_HEATER_SENSOR_FAULT:
    alarmKey = ALARM_HEATER_SENSOR_FAULT_KEY;
    break;
  default:
    return;
  }
  addVariableToTelemetryWIFIJSON[alarmKey] = value;
}

void addAlarmTelemetriesToWIFIJSON() {
  int alarmReported = 0;
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

#if TX_GROUP_DIAG_WIFI // grupo DIAG — config/transport_policy.h
  addVariableToTelemetryWIFIJSON[BOOT_COUNT_KEY] = g_bootCount;
  addVariableToTelemetryWIFIJSON[GPRS_KILL_COUNT_KEY] = g_gprsKillCount;
  addVariableToTelemetryWIFIJSON[GPRS_MON_KILL_COUNT_KEY] = g_monKillCount;
  addVariableToTelemetryWIFIJSON[FREE_HEAP_KEY] = (uint32_t)ESP.getFreeHeap();
  addVariableToTelemetryWIFIJSON[MIN_FREE_HEAP_KEY] =
      (uint32_t)ESP.getMinFreeHeap();
  addVariableToTelemetryWIFIJSON[UPTIME_S_KEY] = (uint32_t)(millis() / 1000);
  addVariableToTelemetryWIFIJSON[HMI_BOOT_COUNT_KEY] = g_hmiBootCount;
  addVariableToTelemetryWIFIJSON[HMI_LAST_RST_KEY] = g_hmiLastRst;
#endif

#if TX_GROUP_CELLULAR_WIFI // grupo CELLULAR — config/transport_policy.h
  // Ojo: por WiFi estos campos pueden ir vacíos o rancios si el módem no ha
  // llegado a registrarse. Por eso el flag está a 0 por defecto.
  addVariableToTelemetryWIFIJSON[IMEI_KEY] = GPRS.IMEI.c_str();
  addVariableToTelemetryWIFIJSON[APN_KEY] = GPRS.APN.c_str();
  addVariableToTelemetryWIFIJSON[COP_KEY] = GPRS.COP.c_str();
  addVariableToTelemetryWIFIJSON[CELL_SIGNAL_QUALITY_KEY] = GPRS.CSQ;
#endif

  addVariableToTelemetryWIFIJSON[SYS_CURR_STANDBY_TEST_KEY] =
      roundSignificantDigits(in3.system_current_standby_test,
                             TELEMETRIES_DECIMALS);
  addVariableToTelemetryWIFIJSON[HEATER_CURR_TEST_KEY] =
      roundSignificantDigits(in3.heater_current_test, TELEMETRIES_DECIMALS);
  addVariableToTelemetryWIFIJSON[FAN_CURR_TEST_KEY] =
      roundSignificantDigits(in3.fan_current_test, TELEMETRIES_DECIMALS);
  addVariableToTelemetryWIFIJSON[FAN_FEEDBACK_PRESENT_KEY] =
      in3.fanHasSpeedFeedback;
  addVariableToTelemetryWIFIJSON[PHOTOTHERAPY_CURR_KEY] =
      roundSignificantDigits(in3.phototherapy_current_test,
                             TELEMETRIES_DECIMALS);
  addVariableToTelemetryWIFIJSON[PHOTOTHERAPY_PWM_KEY] =
      roundSignificantDigits(in3.phototherapy_intensity, TELEMETRIES_DECIMALS);
  addVariableToTelemetryWIFIJSON[HUMIDIFIER_CURR_KEY] =
      roundSignificantDigits(in3.humidifier_current_test, TELEMETRIES_DECIMALS);
  addVariableToTelemetryWIFIJSON[BUZZER_CURR_TEST_KEY] =
      roundSignificantDigits(in3.buzzer_current_test, TELEMETRIES_DECIMALS);
  addVariableToTelemetryWIFIJSON[HW_TEST_KEY] = in3.HW_test_error_code;

  addVariableToTelemetryWIFIJSON[UI_LANGUAGE_KEY] = in3.language;
  addVariableToTelemetryWIFIJSON[CALIBRATED_SENSOR_KEY] = !in3.calibrationError;
  addVariableToTelemetryWIFIJSON[GPRS_CONNECTIVITY_KEY] = false;
  addVariableToTelemetryWIFIJSON[WIFI_CONNECTIVITY_KEY] = true;

#if TX_GROUP_CALIBRATION_WIFI // grupo CALIBRATION — config/transport_policy.h
  // Los globales de calibración no están en ningún header: cada .cpp que los
  // usa declara su propio extern (mismo patrón que GPRS.cpp).
  extern double ReferenceTemperatureRange, ReferenceTemperatureLow;
  extern double RawTemperatureLow[SENSOR_TEMP_QTY],
      RawTemperatureRange[SENSOR_TEMP_QTY];
  addVariableToTelemetryWIFIJSON[CALIBRATION_REFERENCE_TEMPERATURE_RANGE_KEY] =
      roundSignificantDigits(ReferenceTemperatureRange, TELEMETRIES_DECIMALS);
  addVariableToTelemetryWIFIJSON[CALIBRATION_REFERENCE_TEMPERATURE_LOW_KEY] =
      roundSignificantDigits(ReferenceTemperatureLow, TELEMETRIES_DECIMALS);
  addVariableToTelemetryWIFIJSON[CALIBRATION_SKIN_FINETUNE_KEY] =
      roundSignificantDigits(in3.fineTuneSkinTemperature, TELEMETRIES_DECIMALS);
  addVariableToTelemetryWIFIJSON[CALIBRATION_AIR_FINETUNE_KEY] =
      roundSignificantDigits(in3.fineTuneAirTemperature, TELEMETRIES_DECIMALS);
  addVariableToTelemetryWIFIJSON[CALIBRATION_RAW_TEMPERATURE_RANGE_SKIN_KEY] =
      roundSignificantDigits(RawTemperatureRange[SKIN_SENSOR],
                             TELEMETRIES_DECIMALS);
  addVariableToTelemetryWIFIJSON[CALIBRATION_RAW_TEMPERATURE_LOW_SKIN_KEY] =
      roundSignificantDigits(RawTemperatureLow[SKIN_SENSOR],
                             TELEMETRIES_DECIMALS);
#endif
}

void addTelemetriesToWIFIJSON() {
  addAlarmTelemetriesToWIFIJSON();
  // GSM-based location: the GPRS modem is kept attached to the cellular
  // network purely for this even when WiFi carries the telemetry itself
  // (see GPRSUpdateLocationIfDue() in GPRS.cpp). Only available while the
  // inserted SIM is active and registered on a network.
  if (GPRS.longitud || GPRS.latitud) {
    addVariableToTelemetryWIFIJSON[LOCATION_LONGTITUD_KEY] = GPRS.longitud;
    addVariableToTelemetryWIFIJSON[LOCATION_LATITUD_KEY] = GPRS.latitud;
    addVariableToTelemetryWIFIJSON[TRI_ACCURACY_KEY] = GPRS.accuracy;
  }
  addVariableToTelemetryWIFIJSON[SKIN_TEMPERATURE_KEY] = roundSignificantDigits(
      in3.temperature[SKIN_SENSOR], TELEMETRIES_DECIMALS);
  addVariableToTelemetryWIFIJSON[AIR_TEMPERATURE_KEY] = roundSignificantDigits(
      in3.temperature[ROOM_DIGITAL_TEMP_SENSOR], TELEMETRIES_DECIMALS);
  if (in3.airTemperatureRedundantSensor) {
    addVariableToTelemetryWIFIJSON[AIR_TEMPERATURE_REDUNDANT_KEY] =
        roundSignificantDigits(in3.airTemperatureRedundantSensor,
                               TELEMETRIES_DECIMALS);
  }
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

  if (chargerPresent && g_bq_status_valid &&
      g_bq_status.ac_present && g_bq_status.vbat_mv > 10000) {
    addVariableToTelemetryWIFIJSON[BQ_STATE_KEY] = (int)g_bq_status.state;
    addVariableToTelemetryWIFIJSON[BQ_FAULT_KEY] = g_bq_status.fault;
    addVariableToTelemetryWIFIJSON[BQ_AC_KEY]    = g_bq_status.ac_present;
    addVariableToTelemetryWIFIJSON[BQ_VBAT_KEY]  =
        roundSignificantDigits(g_bq_status.vbat_mv / 1000.0f, 3);
    addVariableToTelemetryWIFIJSON[BQ_VBUS_KEY]  =
        roundSignificantDigits(g_bq_status.vbus_mv / 1000.0f, 3);
    addVariableToTelemetryWIFIJSON[BQ_ICHG_KEY]  = (int)g_bq_status.ichg_ma;
  }

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

  if (in3.fanCommandedOn) {
    // Only while an OTA is actually downloading, so the key does not
  // sit at a stale value between updates.
  extern volatile int g_otaProgressPct;
  if (g_otaProgressPct >= 0) {
    addVariableToTelemetryWIFIJSON[OTA_PROGRESS_KEY] = g_otaProgressPct;
  }
  addVariableToTelemetryWIFIJSON[FAN_RPM_KEY] = (int)(in3.fan_rpm + 0.5f);
    addVariableToTelemetryWIFIJSON[FAN_PWM_KEY] =
        fanControlPID.GetMode() == AUTOMATIC ? (int)(fanControlPIDOutput + 0.5)
                                             : in3.fanCtlPWM;
  }

  // Suppress SpO2/HR telemetry unless the probe is actually on the patient —
  // no probe or probe-present-but-not-applied readings aren't valid vitals.
  if (g_spo2_data.probe_state == ProbeState::PROBE_APPLIED) {
    if (g_spo2_data.spo2_sqi > 0.0f) {
      addVariableToTelemetryWIFIJSON[SPO2_KEY] =
          roundSignificantDigits(g_spo2_data.spo2, TELEMETRIES_DECIMALS);
      addVariableToTelemetryWIFIJSON[SPO2_SQI_KEY] =
          roundSignificantDigits(g_spo2_data.spo2_sqi, TELEMETRIES_DECIMALS);
      addVariableToTelemetryWIFIJSON[PI_KEY] =
          roundSignificantDigits(g_spo2_data.pi, TELEMETRIES_DECIMALS);
    }
    if (g_spo2_data.hr1_sqi > 0.0f) {
      addVariableToTelemetryWIFIJSON[HR1_KEY] = (int)(g_spo2_data.hr1 + 0.5f);
      addVariableToTelemetryWIFIJSON[HR1_SQI_KEY] =
          roundSignificantDigits(g_spo2_data.hr1_sqi, TELEMETRIES_DECIMALS);
    }
    if (g_spo2_data.hr2_sqi > 0.0f) {
      addVariableToTelemetryWIFIJSON[HR2_KEY] = (int)(g_spo2_data.hr2 + 0.5f);
      addVariableToTelemetryWIFIJSON[HR2_SQI_KEY] =
          roundSignificantDigits(g_spo2_data.hr2_sqi, TELEMETRIES_DECIMALS);
    }
    if (g_spo2_data.hr3_sqi > 0.0f) {
      addVariableToTelemetryWIFIJSON[HR3_KEY] = (int)(g_spo2_data.hr3 + 0.5f);
      addVariableToTelemetryWIFIJSON[HR3_SQI_KEY] =
          roundSignificantDigits(g_spo2_data.hr3_sqi, TELEMETRIES_DECIMALS);
    }
  }

}


// Publishes queued baby lifecycle events and the current-occupant attributes.
// Peek -> send -> pop: an event is only dropped once the broker accepted it,
// so a publish failure retries on the next cycle instead of losing the
// record (the previous dirty-flag scheme cleared itself while building the
// payload, so any failed send lost the data permanently).
// One event per call keeps a backlog from monopolising the modem.
static void publishBabyCloudDataWIFI() {
  char json[512];

  if (babyStore_attributesDirty()) {
    const BabyProfile *occ = babyStore_currentOccupant();
    int n = occ ? babyCloud_buildAttributesJson(occ, json, sizeof(json))
                : babyCloud_buildEmptyAttributesJson(json, sizeof(json));
    if (n > 0 && tb_wifi.sendAttributeJson(json)) {
      babyStore_clearAttributesDirty();
    }
  }

  BabyCloudEvent e;
  if (babyStore_peekCloudEvent(&e)) {
    int n = babyCloud_buildEventJson(&e, json, sizeof(json));
    if (n <= 0) {
      babyStore_popCloudEvent();  // unbuildable: drop rather than wedge
    } else if (tb_wifi.sendTelemetryJson(json)) {
      babyStore_popCloudEvent();
    }
  }
}

void WEB_OTA() {
  if (WIFIIsConnected()) {
    if (strlen(pendingSSID) > 0 && WiFi.SSID() == String(pendingSSID)) {
      logI("[WIFI] -> Connection successful, persisting credentials to Preferences");
      { Preferences p; p.begin(NS_WIFI, false);
        p.putString(KEY_SSID,     pendingSSID);
        p.putString(KEY_PASSWORD, pendingPass);
        p.end(); }
      pendingSSID[0] = '\0';
      pendingPass[0] = '\0';
    }
    static bool s_server_initialized = false;
    if (!s_server_initialized) {
      configWifiServer();
      s_server_initialized = true;
    }
    wifiServer.handleClient();
    WIFI_connection_status = true;
  } else {
    WIFI_connection_status = false;
  }
}

// ── RPC handlers (WiFi TB path) ───────────────────────────────────────────────
static void rpc_setwifi_wifi_cb(JsonVariantConst const & data,
                                JsonDocument & response) {
  const char* ssid = data["ssid"];
  const char* pass = data["password"];
  if (!ssid || !pass || ssid[0] == '\0' || pass[0] == '\0' ||
      strlen(ssid) > 63 || strlen(pass) > 63) {
    // Nunca registrar ssid/password: son credenciales.
    ESP_LOGW("WiFi", "[RPC] setWifi: invalid ssid or password");
    response["status"] = "invalid";
    return;
  }
  ESP_LOGI("WiFi", "[RPC] setWifi received, applying credentials");
  applyWifiCredentials(ssid, pass);
  response["status"] = "ok";
}

// "Capturar ahora" desde el dashboard: arranca una captura de PPG_snapshot
// bajo demanda si el gate (sonda aplicada + rsqi válido) pasa. La respuesta
// del RPC le dice al usuario si arrancó, si ya había una en curso, o si la
// señal no está lista todavía (ej. sonda no puesta).
static void rpc_capture_ppg_cb(JsonVariantConst const & /*data*/,
                               JsonDocument & response) {
  PpgSnapshotStatus st = ppgSnapshotRequestCapture(
      g_spo2_data.probe_state == ProbeState::PROBE_APPLIED, g_spo2_data.rsqi,
      millis());
  switch (st) {
    case PpgSnapshotStatus::STARTED:
      response["status"] = "started";
      break;
    case PpgSnapshotStatus::BUSY:
      response["status"] = "busy";
      break;
    case PpgSnapshotStatus::SIGNAL_NOT_READY:
      response["status"] = "signal_not_ready";
      break;
  }
}

// Mismo nombre de RPC que en GPRS.cpp, pero cada transporte tiene que llamar a
// SU comprobación: las dos hablan con su propio cliente ThingsBoard. Sin este
// gemelo, el botón del dashboard solo respondía si el equipo estaba en GPRS, y
// por WiFi expiraba sin que el RPC existiese siquiera.
static void rpc_check_ota_wifi_cb(JsonVariantConst const & /*data*/,
                                  JsonDocument & response) {
  WIFICheckOTA();
  response["status"] = "checking";
  logI("[WIFI] -> OTA check forced by RPC");
}

static RPC_Callback wifi_rpc_callbacks[] = {
  RPC_Callback("setWifi", rpc_setwifi_wifi_cb),
  RPC_Callback("capturePPG", rpc_capture_ppg_cb),
  RPC_Callback("checkOta", rpc_check_ota_wifi_cb),
};

// El array de PPG_snapshot necesita un ts real por muestra (20 ms entre
// muestras a 50 Hz) para que un chart de serie temporal estándar lo dibuje
// bien — no basta con millis() desde el arranque. No bloqueante: si el reloj
// aún no está sincronizado, arranca la sincronización y devuelve false; el
// envío del snapshot simplemente se reintenta en la siguiente vuelta del
// loop (cada OTA_TASK_PERIOD_MS, 50 ms) hasta que sincronice.
// La comprobación de reloj para el snapshot se hace ahora dentro de
// ppgSnapshotPublish(), común a los dos transportes. Aquí solo queda el
// arranque de SNTP, que sigue haciendo falta para tener hora por WiFi.
static void ensureWifiTimeStarted() {
  static bool s_requested = false;
  if (s_requested)
    return;
  time_t now = 0;
  time(&now);
  if (now < 1609459200L) { // antes de 2021-01-01: aún no sincronizado
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    return; // reintenta en la siguiente vuelta hasta que SNTP conteste
  }
  s_requested = true;
}

// Zona horaria por WiFi, preguntando a quién sí la sabe.
//
// NTP transporta UTC y NADA más: no tiene un campo de zona horaria que pedirle,
// y por eso `configTime(0, 0, ...)` de arriba no está mal configurado — es que
// no hay nada que configurar. La hora local no es deducible por el equipo: es
// una convención política, no una magnitud observable, así que alguien externo
// tiene que comunicarla. Con módem es NITZ; sin módem, esto.
//
// ip-api.com devuelve el offset de la zona en SEGUNDOS. Su nivel gratuito es
// solo HTTP (verificado contra su documentación: el SSL es de pago), así que
// esto viaja en claro. Es un riesgo asumido y acotado: el offset solo afecta a
// lo que se PINTA. Los epochs almacenados y la telemetría siguen en UTC, y
// nada de esto toca el PID ni las alarmas. Lo peor que consigue quien
// manipulase la respuesta es un reloj de pantalla desplazado.
static void ensureWifiTimeZoneSynced() {
  static uint32_t s_lastAttemptMs = 0;
  // Si NITZ ya la resolvió no hay nada que preguntar: la antena está donde
  // está el equipo y una IP puede ser de una VPN o de otro país. Además
  // tz_source_set() nunca deja que IP pise a NITZ, así que consultar aquí
  // sería tirar peticiones HTTP sin ningún efecto.
  if (tz_source_origin() == TZ_SOURCE_NITZ)
    return;
  // Mientras no haya zona resuelta se reintenta cada 5 min (45
  // peticiones/minuto es el límite del servicio, de sobra incluso
  // compartiendo IP con toda una clínica); una vez resuelta se refresca una
  // vez al día para que un cambio de horario de verano/invierno no se quede
  // pillado en un equipo que lleve semanas sin reiniciar.
  uint32_t interval = tz_source_known() ? TX_TIMEZONE_REFRESH_MS : 300000u;
  if (s_lastAttemptMs != 0 && millis() - s_lastAttemptMs < interval)
    return;
  s_lastAttemptMs = millis();
  if (WiFi.status() != WL_CONNECTED)
    return;

  WiFiClient client;
  client.setTimeout(5);
  if (!client.connect("ip-api.com", 80))
    return;
  client.print("GET /json/?fields=status,offset HTTP/1.1\r\n"
               "Host: ip-api.com\r\nConnection: close\r\n\r\n");

  // Respuesta minúscula (fields= recorta todo lo demás). Buffer fijo y con
  // tope de tiempo: esto corre en la tarea de OTA y no puede quedarse colgado.
  char buf[256];
  size_t len = 0;
  uint32_t deadline = millis() + 5000;
  while ((client.connected() || client.available()) && millis() < deadline &&
         len < sizeof(buf) - 1) {
    if (!client.available()) {
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }
    int c = client.read();
    if (c < 0)
      break;
    buf[len++] = (char)c;
  }
  buf[len] = '\0';
  client.stop();

  // El cuerpo va tras la línea en blanco de las cabeceras. Si no aparece, la
  // respuesta está incompleta y se descarta entera.
  const char *body = strstr(buf, "\r\n\r\n");
  if (!body)
    return;
  body += 4;

  int quarters = 0;
  if (!tz_parse_ipapi_offset(body, &quarters))
    return;
  if (tz_source_set(quarters, TZ_SOURCE_IP)) {
    ESP_LOGI("WiFi", "Timezone from IP lookup: %d quarter-hours", quarters);
  }
}
static constexpr size_t WIFI_RPC_CB_COUNT =
    sizeof(wifi_rpc_callbacks) / sizeof(wifi_rpc_callbacks[0]);
// ─────────────────────────────────────────────────────────────────────────────

void WIFI_TB_OTA() {
  if (WIFIIsConnected()) {
    // Independiente del aprovisionamiento en ThingsBoard a propósito: mismo
    // fallo que en GPRSPost() (ver GPRS.cpp) — una unidad sin número de serie
    // se queda esperando el provisioning para siempre y, si esto viviera más
    // abajo, jamás llegaría a poner en hora ni a resolver la zona horaria.
    ensureWifiTimeStarted();
    ensureWifiTimeZoneSynced();

    if (!Wifi_TB.provisioned) {
      if (in3.serialNumber == 0) {
        logI("[WIFI] -> Waiting for serial number before provisioning");
      } else if (!Wifi_TB.provision_request_sent) {
        WIFITBProvision();
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
          for (size_t i = 0; i < WIFI_RPC_CB_COUNT; i++) {
            tb_wifi.RPC_Subscribe(wifi_rpc_callbacks[i]);
          }
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
          publishBabyCloudDataWIFI();
          Wifi_TB.lastMQTTPublish = millis();
        }
        if (millis() - Wifi_TB.lastOTACheck > WIFI_OTA_CHECK_INTERVAL &&
            !GPRS.OTAInProgress) {
          WIFICheckOTA();
          Wifi_TB.lastOTACheck = millis();
        }

        // PPG snapshot: captura automática cada PPG_SNAPSHOT_AUTO_INTERVAL_MS
        // mientras haya WiFi/TB arriba (además del botón "capturar ahora" del
        // RPC capturePPG). BUSY/SIGNAL_NOT_READY se ignoran aquí a propósito:
        // simplemente se reintenta en el siguiente intervalo.
#if TX_FEATURE_PPG_AUTOCAPTURE_WIFI
        if (millis() - Wifi_TB.lastPpgSnapshotAttempt >
            PPG_SNAPSHOT_AUTO_INTERVAL_MS) {
          Wifi_TB.lastPpgSnapshotAttempt = millis();
          ppgSnapshotRequestCapture(
              g_spo2_data.probe_state == ProbeState::PROBE_APPLIED,
              g_spo2_data.rsqi, millis());
        }
#endif
        // El montaje del JSON es idéntico por GPRS: vive en
        // PpgSnapshotPublish.cpp para no tener dos copias que diverjan.
        ppgSnapshotPublish(tb_wifi, "WIFI");
      }
    }
  } else {
    Wifi_TB.serverConnectionStatus = false;
  }
  tb_wifi.loop();
}

void WifiOTAHandler(void) {
  if (WIFI_EN && !WIFIIsConnected()) {
    if (millis() - Wifi_TB.lastWifiReconnectAttempt > WIFI_RECONNECT_INTERVAL) {
      logI("[WIFI] -> Connection lost, re-init WiFi");
      wifiInit();   // updates lastWifiReconnectAttempt
    }
  }

  WIFI_TB_OTA();
  WEB_OTA();
}
