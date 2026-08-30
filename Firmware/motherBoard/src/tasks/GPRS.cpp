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

#include "GPRS.h"

#include <Arduino.h>
#include <Preferences.h>

#include "modules/baby_profile/baby_cloud.h"
#include "modules/baby_profile/baby_profile_store.h"
#include "modules/util/civil_time.h"
#include "modules/util/tz_source.h"
#include <sys/time.h>

#include "CommTask.h"
#include "PpgSnapshot.h"
#include "PpgSnapshotPublish.h"
#include "SPO2.h"
#include "Wifi_OTA.h"
#include "main.h"

// Initialize GSM modem
TinyGsm modem(modemSerial);

// Initialize GSM client
TinyGsmClient client(modem);

// Initalize the Mqtt client instance
Arduino_MQTT_Client mqttClientGPRS(client);

// Initialize ThingsBoard instance
// ThingsBoardSized<THINGSBOARD_BUFFER_SIZE, THINGSBOARD_FIELDS_AMOUNT>
// tb(client);

// Initialize ThingsBoard client provision instance
ThingsBoard tb(mqttClientGPRS, MAX_MESSAGE_SIZE);

StaticJsonDocument<JSON_OBJECT_SIZE(THINGSBOARD_FIELDS_AMOUNT)> GPRS_JSON;
JsonObject addVariableToTelemetryGPRSJSON = GPRS_JSON.to<JsonObject>();

unsigned long previous_processing_time;
extern bool ambientSensorPresent;

extern IncuNest_parameters in3;
extern PID fanControlPID;
extern double fanControlPIDOutput;

GPRSstruct GPRS;
Credentials credentials;
Espressif_Updater updater_GPRS;

// Tried in order on each attach failure; onomondo first.
static const char *const GPRS_APN_LIST[] = {APN_ONOMONDO, APN_TM,
                                            APN_TRUPHONE};
static constexpr size_t GPRS_APN_COUNT =
    sizeof(GPRS_APN_LIST) / sizeof(GPRS_APN_LIST[0]);

// Statuses for updating
bool currentFWSent = false;
bool updateRequestSent = false;

extern double ReferenceTemperatureRange, ReferenceTemperatureLow;

extern double RawTemperatureLow[SENSOR_TEMP_QTY],
    RawTemperatureRange[SENSOR_TEMP_QTY];

void GPRSCheckOTA();  // defined below; forced by the checkOta RPC

// ── RPC handlers ─────────────────────────────────────────────────────────────
static void rpc_restart_cb(JsonVariantConst const & /*data*/,
                           JsonDocument & response) {
  response["status"] = "restarting";
  logModemData("[RPC] restart command received");
  vTaskDelay(pdMS_TO_TICKS(200)); // let TB ACK the response before reset
  ESP.restart();
}

static void rpc_diag_cb(JsonVariantConst const & /*data*/,
                        JsonDocument & response) {
  response["boot_count"]    = g_bootCount;
  response["free_heap"]     = (uint32_t)ESP.getFreeHeap();
  response["min_free_heap"] = (uint32_t)ESP.getMinFreeHeap();
  response["uptime_s"]      = (uint32_t)(millis() / 1000);
  response["gprs_kill"]     = g_gprsKillCount;
  response["mon_kill"]      = g_monKillCount;
  response["hmi_boots"]     = g_hmiBootCount;
  response["hmi_last_rst"]  = g_hmiLastRst;
}

static void rpc_setwifi_cb(JsonVariantConst const & data,
                           JsonDocument & response) {
  const char* ssid = data["ssid"];
  const char* pass = data["password"];
  if (!ssid || !pass || ssid[0] == '\0' || pass[0] == '\0' ||
      strlen(ssid) > 63 || strlen(pass) > 63) {
    // Nunca registrar ssid/password: son credenciales.
    logModemData("[RPC] setWifi: invalid ssid or password");
    response["status"] = "invalid";
    return;
  }
  logModemData("[RPC] setWifi received, applying credentials");
  applyWifiCredentials(ssid, pass);
  response["status"] = "ok";
}

static void rpc_wipe_babies_cb(JsonVariantConst const & data,
                               JsonDocument & response) {
  // Same explicit confirmation as the /config path: a stray RPC must never
  // erase clinical records.
  if (data["confirm"] != 1234) {
    response["status"] = "refused";
    return;
  }
  int n = babyStore_wipeAll();
  response["status"] = "wiped";
  response["files_removed"] = n;
  logModemData("[RPC] baby data wiped");
}

static void rpc_check_ota_cb(JsonVariantConst const & /*data*/,
                             JsonDocument & response) {
  GPRSCheckOTA();  // real implementation earlier in this file
                   // (handles currentFWSent bookkeeping)
  response["status"] = "checking";
  logModemData("[RPC] OTA check forced");
}

#if TX_FEATURE_PPG_SNAPSHOT_GPRS
// Gemelo del capturePPG de Wifi_OTA.cpp. La captura la hace el mismo módulo
// compartido; lo único propio del transporte es por dónde sale el resultado.
static void rpc_capture_ppg_gprs_cb(JsonVariantConst const & /*data*/,
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
#endif

static RPC_Callback rpc_callbacks[] = {
  RPC_Callback("restart",  rpc_restart_cb),
  RPC_Callback("getDiag",  rpc_diag_cb),
  RPC_Callback("setWifi",  rpc_setwifi_cb),
  RPC_Callback("wipeBabies", rpc_wipe_babies_cb),
  RPC_Callback("checkOta",   rpc_check_ota_cb),
#if TX_FEATURE_PPG_SNAPSHOT_GPRS
  RPC_Callback("capturePPG", rpc_capture_ppg_gprs_cb),
#endif
};
static constexpr size_t RPC_CB_COUNT = sizeof(rpc_callbacks) / sizeof(rpc_callbacks[0]);

static void subscribeRPCHandlers() {
  for (size_t i = 0; i < RPC_CB_COUNT; i++) {
    tb.RPC_Subscribe(rpc_callbacks[i]);
  }
}
// ─────────────────────────────────────────────────────────────────────────────

// Download progress, 0-100. Published as telemetry so an OTA can be watched
// from the dashboard instead of only from a serial log nobody is attached to
// while the unit is in the field. -1 = no OTA in flight.
volatile int g_otaProgressPct = -1;

void progressCallback(const uint32_t &currentChunk,
                      const uint32_t &totalChuncks) {
  if (totalChuncks > 0) {
    g_otaProgressPct = (int)((currentChunk * 100U) / totalChuncks);
  }
  if (LOG_MODEM_DATA) {
    char buffer[50]; // Create a buffer to hold the formatted string
    snprintf(buffer, sizeof(buffer), "Progress %.2f%%",
             static_cast<float>(currentChunk * 100U) / totalChuncks);
    logModemData(String(buffer)); // Pass the formatted string to logModemData
  }
  GPRS.OTAInProgress = true;
}

void updatedCallback(const bool &success) {
  g_otaProgressPct = -1;
  if (success) {
    logModemData("[GPRS] -> Done, OTA will be implemented on next boot");
    // esp_restart();
  } else {
    logModemData("[GPRS] -> No new firmware");
  }
  GPRS.OTAInProgress = false;
}

const OTA_Update_Callback OTAcallback(&progressCallback, &updatedCallback,
                                      CURRENT_FIRMWARE_TITLE, FWversion,
                                      &updater_GPRS, FIRMWARE_FAILURE_RETRIES,
                                      FIRMWARE_PACKET_SIZE,
                                      WAIT_FAILED_OTA_CHUNKS);

void clearGPRSBuffer() {
  memset(GPRS.buffer, 0, sizeof(GPRS.buffer));
  GPRS.charToRead = false;
  GPRS.bufferWritePos = false;
}

int checkSerial(const char *success, const char *error) {
  if (strstr(GPRS.buffer, success)) {
    GPRS.process++;
    clearGPRSBuffer();
    return true;
  }
  if (strstr(GPRS.buffer, error)) {
    logE("[GPRS] -> GPRS error: " + String(error));
    clearGPRSBuffer();
    return -1;
  }
  return false;
}

void initGPRS() {
  esp_reset_reason_t reason = esp_reset_reason();
  if (reason == ESP_RST_SW || reason == ESP_RST_WDT ||
      reason == ESP_RST_TASK_WDT || reason == ESP_RST_INT_WDT ||
      reason == ESP_RST_PANIC) {
    logModemData("[GPRS] -> Abnormal reset detected (" + String(reason) +
                 "), deleting GPRS task this session");
    {
      Preferences p;
      p.begin("diag", false);
      g_gprsKillCount = p.getUInt("gprs_kill", 0) + 1;
      p.putUInt("gprs_kill", g_gprsKillCount);
      p.end();
    }
    // Delete the *current* task (i.e. GPRS_Task)
    vTaskDelete(NULL);
    // Note: control never returns here
  }

  // Normal power‑up path:
  Serial2.begin(MODEM_BAUD, SERIAL_8N1, GSM_UART_TX_PIN, GSM_UART_RX_PIN);
  GPRS.powerUp = true;
#if (GPRS_PWRKEY)
  digitalWrite(GPRS_PWRKEY, HIGH);
#endif
}

bool GPRSCheckNewEvent() {
  bool retVal = false;
  bool isGPRSConnected = GPRS.post;
  bool serverConnectionStatus = false;
  if (isGPRSConnected) {
    serverConnectionStatus = GPRS.serverConnectionStatus;
  }
  if (serverConnectionStatus != GPRS.lastServerConnectionStatus ||
      isGPRSConnected != GPRS.lastGPRSConnectionStatus) {
    retVal = true;
  }
  GPRS.lastGPRSConnectionStatus = isGPRSConnected;
  GPRS.lastServerConnectionStatus = serverConnectionStatus;
  return (retVal);
}

bool GPRSIsAttached() { return (GPRS.post); }

bool GPRSIsConnectedToServer() { return (GPRS.serverConnectionStatus); }

void GPRS_get_triangulation_location() {
  int year = 0;
  int month = 0;
  int day = 0;
  int hour = 0;
  int min = 0;
  int sec = 0;
  modem.getGsmLocation(&GPRS.longitud, &GPRS.latitud, &GPRS.accuracy, &year,
                       &month, &day, &hour, &min, &sec);
}

// Refreshes GPRS.longitud/latitud/accuracy at most once per
// GPRS_TRIANGULATION_INTERVAL. Requires an active GPRS/PDP context
// (GPRS.post), which the modem keeps up even while WiFi carries the
// telemetry, purely so location stays available on that path too.
void GPRSUpdateLocationIfDue() {
  if (!GPRS.post) {
    return;
  }
  if (millis() - GPRS.lastTriangulationUpdate > GPRS_TRIANGULATION_INTERVAL) {
    GPRS_get_triangulation_location();
    GPRS.lastTriangulationUpdate = millis();
  }
}

// Cellular wall-clock sync. Until now the only time source was WiFi NTP
// (DriveUpload/Wifi_OTA), so a unit deployed on GPRS alone never knew the
// date and every baby profile had to have its age typed in by hand.
//
// Two sources, cheapest first:
//   1. The modem's own RTC (AT+CCLK? via TinyGSM). TinyGSM already enables
//      NITZ (AT+CLTS=1) at init, so the operator sets this for free, with no
//      data traffic — but not every network sends it.
//   2. NTP over the PDP context (AT+CNTP), which needs an attached context.
//
// Success sets the ESP32 system clock, which is the single integration
// point: babyStore_nowEpoch() and the CTRL,TIME broadcast both read
// time(nullptr), so age derivation starts working with no further changes.
void GPRSEnsureTimeSynced() {
  static bool s_synced = false;
  static uint32_t s_lastAttemptMs = 0;
  if (s_synced) return;

  // WiFi NTP may have won the race; nothing to do if the clock is already set.
  if (time(nullptr) >= (time_t)1609459200L) {
    s_synced = true;
    logModemData("[GPRS] -> time already synced (WiFi NTP)");
    return;
  }

  if (s_lastAttemptMs != 0 &&
      millis() - s_lastAttemptMs < GPRS_TIME_SYNC_RETRY_INTERVAL) {
    return;
  }
  s_lastAttemptMs = millis();

  int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
  float tz = 0.0f;
  uint32_t epoch = 0;
  bool got = false;

  if (modem.getNetworkTime(&year, &month, &day, &hour, &minute, &second,
                           &tz)) {
    got = civil_to_unix_utc(year, (unsigned)month, (unsigned)day,
                            (unsigned)hour, (unsigned)minute,
                            (unsigned)second, (int)tz, &epoch);
    if (!got) {
      // Expected when the operator sends no NITZ: the SIM800 reports its
      // 2004 default, which civil_to_unix_utc() rejects outright.
      logModemData("[GPRS] -> NITZ clock not valid yet");
    }
  }

  if (!got && GPRS.post) {
    // Fall back to NTP over the PDP context; only possible once attached.
    if (modem.NTPServerSync("pool.ntp.org", 0) == 1 &&
        modem.getNetworkTime(&year, &month, &day, &hour, &minute, &second,
                             &tz)) {
      got = civil_to_unix_utc(year, (unsigned)month, (unsigned)day,
                              (unsigned)hour, (unsigned)minute,
                              (unsigned)second, (int)tz, &epoch);
    }
    if (!got) logModemData("[GPRS] -> NTP over PDP failed");
  }

  if (!got) return;

  struct timeval tv = {};
  tv.tv_sec = (time_t)epoch;
  settimeofday(&tv, nullptr);
  s_synced = true;
  logModemData("[GPRS] -> clock synced from cellular, epoch " + String(epoch));
}

// Zona horaria desde la red movil (NITZ), separada de la puesta en hora.
//
// Son dos cosas distintas y estaban en la misma funcion: el RELOJ lo puede
// poner el NTP por WiFi, pero la ZONA solo la sabe el modem. Como
// GPRSEnsureTimeSynced() sale antes cuando el reloj ya esta puesto, en toda
// unidad donde el WiFi ganase la carrera nunca se llegaba a preguntar por la
// zona, y su latch impedia una segunda oportunidad: se quedaba en UTC de por
// vida. Con latch propio, la zona se busca aunque la hora ya este resuelta.
//
// getNetworkTime() entrega la hora LOCAL de la antena junto a su offset; aqui
// solo interesa el offset, porque el instante ya lo tiene el sistema.
void GPRSEnsureTimeZoneSynced() {
  static bool s_tzSynced = false;
  static uint32_t s_lastTzAttemptMs = 0;
  if (s_tzSynced) return;

  if (s_lastTzAttemptMs != 0 &&
      millis() - s_lastTzAttemptMs < GPRS_TIME_SYNC_RETRY_INTERVAL) {
    return;
  }
  s_lastTzAttemptMs = millis();

  int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
  float tz = 0.0f;
  if (!modem.getNetworkTime(&year, &month, &day, &hour, &minute, &second,
                            &tz)) {
    return;
  }
  // Sin NITZ el SIM800 contesta con su fecha por defecto de 2004 y un tz que
  // no significa nada. Se valida la FECHA para decidir si creerse el offset.
  uint32_t epochProbe = 0;
  if (!civil_to_unix_utc(year, (unsigned)month, (unsigned)day, (unsigned)hour,
                         (unsigned)minute, (unsigned)second, (int)tz,
                         &epochProbe)) {
    logModemData("[GPRS] -> NITZ timezone not valid yet");
    return;
  }

  if (tz_source_set((int)tz, TZ_SOURCE_NITZ)) {
    s_tzSynced = true;
    logModemData("[GPRS] -> timezone from NITZ: " + String((int)tz) +
                 " quarter-hours");
  }
}

void GPRS_get_SIM_info() {

  GPRS.IMSI = modem.getIMSI();

  GPRS.COP = modem.getOperator();

  GPRS.IP = modem.localIP();

  logModemData("[GPRS] -> IMSI is: " + GPRS.IMSI);
  logModemData("[GPRS] -> COP is: " + GPRS.COP);
}

void GPRSUpdateCSQ() {
  GPRS.CSQ = modem.getSignalQuality();
  logModemData("[GPRS] -> CSQ is: " + String(GPRS.CSQ));
}

void readGPRSData() {
  while (Serial2.available()) {
    GPRS.buffer[GPRS.bufferWritePos] = Serial2.read();
    if (LOG_MODEM_DATA) {
      debugSerial.print(GPRS.buffer[GPRS.bufferWritePos]);
    }
    GPRS.bufferWritePos++;
    if (GPRS.bufferWritePos >= RX_BUFFER_LENGTH) {
      GPRS.bufferWritePos = 0;
      logModemData("[GPRS] -> Buffer overflow");
    }
    GPRS.charToRead++;
  }
}

void GPRSStatusHandler() {
  if (GPRS.process) {
    if (GPRS.powerUp || GPRS.connect) {
      readGPRSData();
      if (millis() - GPRS.processTime > GPRS_TIMEOUT) {
        logE("[GPRS] -> timeOut: " + String(GPRS.powerUp) +
             String(GPRS.connect) + String(GPRS.post) + String(GPRS.process));
        GPRS.timeOut = false;
        GPRS.process = false;
        GPRS.post = false;
        GPRS.connect = false;
        GPRS.powerUp = true;
        GPRS.serverConnectionStatus = false;
        logModemData("[GPRS] -> powering module down...");
        Serial2.print("AT+CPOWD=1\n");
        GPRS.packetSentenceTime = millis();
        GPRS.processTime = millis();
      }
    }
  }
  if (!GPRS.powerUp && !GPRS.connect && !GPRS.post) {
    GPRS.powerUp = true;
  }
}

void GPRSPowerUp() {
  switch (GPRS.process) {
  case 0:
    GPRS.processTime = millis();
#if (GPRS_PWRKEY)
    digitalWrite(GPRS_PWRKEY, LOW);
#endif
    GPRS.process++;
    GPRS.packetSentenceTime = millis();
    logModemData("[GPRS] -> powering up GPRS");
    break;
  case 1:
#if (GPRS_PWRKEY)
    if (millis() - GPRS.packetSentenceTime > 1000) {
      digitalWrite(GPRS_PWRKEY, HIGH);
      logModemData("[GPRS] -> GPRS powered");
    }
#endif
    GPRS.process++;
    break;
  case 2:
    if (millis() - GPRS.packetSentenceTime > 1000) {
      clearGPRSBuffer();
      logModemData("[GPRS] -> Sending AT command");
      Serial2.print(SIMCOM800_ASK_CPIN);
      GPRS.packetSentenceTime = millis();
    }
    if (strstr(GPRS.buffer, AT_CPIN_SIM_PIN)) {
      if (!GPRS.pinAttempted) {
        logModemData("[GPRS] -> SIM PIN required, unlocking...");
        Serial2.print(SIMCOM800_ENTER_PIN);
        GPRS.pinAttempted = true;
      } else {
        // Already tried once this boot and the SIM is still asking for the
        // PIN: stop retrying instead of risking a PUK lock after 3 wrong
        // attempts. The outer GPRS_TIMEOUT will keep cycling powerUp, but we
        // will never send SIMCOM800_ENTER_PIN again until the board reboots.
        logE("[GPRS] -> SIM still requesting PIN after one attempt, giving up");
      }
      clearGPRSBuffer();
      GPRS.packetSentenceTime = 0; // force re-query after 1 s
    }
    if (checkSerial(AT_CPIN_READY, AT_ERROR) == -1) {
      logE("[GPRS] -> CPIN query failed, SIM may be missing or damaged");
    }
    break;
  case 3:
    logModemData("[GPRS] -> Power up success");
    GPRS.CCID = modem.getSimCCID();
    GPRS.CCID.remove(GPRS.CCID.length() - 1);
    GPRS.IMEI = modem.getIMEI();
    logModemData("[GPRS] -> CCID is: " + GPRS.CCID);
    logModemData("[GPRS] -> IMEI is: " + GPRS.IMEI);
    if (GPRS.firstPowerUp) {
      // buzzerTone(3, buzzerStandbyToneDuration, buzzerStandbyTone);
      GPRS.firstPowerUp = false;
    }
    GPRS.powerUp = false;
    GPRS.connect = true;
    GPRS.process = false;
    GPRS.apnIndex = 0; // always try onomondo first
    GPRS.APN = GPRS_APN_LIST[GPRS.apnIndex];
    break;
  }
}

void GPRSStablishConnection() {
  switch (GPRS.process) {
  case 0:
    logModemData("[GPRS] -> Stablishing connection...");
    GPRS.processTime = millis();
    GPRS.packetSentenceTime = millis();
    GPRS.process++;
    break;
  case 1:
    logModemData("[GPRS] -> Connecting...");
    if (modem.gprsConnect(GPRS.APN.c_str(), GPRS_USER, GPRS_PASS)) {
      logModemData("[GPRS] -> Attached");
      // gprsConnect() blocks for the whole attach handshake, which can by
      // itself exceed GPRS_TIMEOUT on slow networks. Refresh processTime here
      // so GPRSStatusHandler() doesn't see stale elapsed time and kill a
      // connection that just succeeded before case 2 gets to run.
      GPRS.processTime = millis();
      GPRS.process++;
    } else {
      logModemData("[GPRS] -> Attach FAIL, retrying with different APN...");
      GPRS.apnIndex = (GPRS.apnIndex + 1) % GPRS_APN_COUNT;
      GPRS.APN = GPRS_APN_LIST[GPRS.apnIndex];
      logModemData("[GPRS] -> New APN: " + GPRS.APN);
      vTaskDelay(GPRS_RECONNECT_INTERVAL / portTICK_PERIOD_MS);
    }
    break;
  case 2:
    GPRS_get_SIM_info();
    GPRS.connect = false;
    GPRS.process = false;
    GPRS.post = true;
    break;
  }
}

void GPRSSetPostPeriod() {
  if (GPRS.firstPublish) {
    if (in3.actuation == ACTUATION_TEMPERATURE ||
        in3.actuation == ACTUATION_HUMIDITY ||
        in3.actuation == ACTUATION_TEMP_AND_HUMIDITY) {
      GPRS.sendPeriod = in3.actuating_gprs_period;
    } else if (in3.phototherapy) {
      GPRS.sendPeriod = in3.phototherapy_gprs_period;
    } else {
      GPRS.sendPeriod = in3.standby_gprs_period;
    }
  } else {
    GPRS.sendPeriod = false;
  }
}

void GPRSProvisionResponse(const JsonObjectConst &data) {
  logModemData("[GPRS] -> Received device provision response");
  const size_t jsonSize = Helper::Measure_Json(data);
  char buffer[jsonSize];
  serializeJson(data, buffer, jsonSize);
  // logModemData("[GPRS] -> " + String(buffer));
  if (strncmp(data["status"], "SUCCESS", strlen("SUCCESS")) != 0) {
    GPRS.provision_retry_count++;
    if (GPRS.provision_retry_count <= PROVISION_MAX_RETRIES) {
      logModemData("[GPRS] -> Provision failed: " + data["errorMsg"].as<String>() +
                   " - retrying as IncuNest-" + String(in3.serialNumber) + "_" + String(GPRS.provision_retry_count));
      GPRS.provision_request_sent = false;
    } else {
      logModemData("[GPRS] -> Provision failed after max retries, giving up");
    }
    return;
  }

  if (strncmp(data[CREDENTIALS_TYPE], ACCESS_TOKEN_CRED_TYPE,
              strlen(ACCESS_TOKEN_CRED_TYPE)) == 0) {

    credentials.client_id = "";
    credentials.username = data[CREDENTIALS_VALUE].as<std::string>();
    credentials.password = "";
    GPRS.provisioned = true;
    GPRS.device_token = credentials.username.c_str();
    { Preferences p; p.begin(NS_GPRS, false);
      p.putString(KEY_TOKEN,       GPRS.device_token);
      p.putUChar (KEY_PROVISIONED, GPRS.provisioned);
      p.end(); }
    logModemData("[GPRS] -> Device provisioned successfully");
  } else if (strncmp(data[CREDENTIALS_TYPE], MQTT_BASIC_CRED_TYPE,
                     strlen(MQTT_BASIC_CRED_TYPE)) == 0) {
    auto credentials_value = data[CREDENTIALS_VALUE].as<JsonObjectConst>();
    credentials.client_id = credentials_value[CLIENT_ID].as<std::string>();
    credentials.username = credentials_value[CLIENT_USERNAME].as<std::string>();
    credentials.password = credentials_value[CLIENT_PASSWORD].as<std::string>();
    GPRS.provisioned = true;
    GPRS.device_token = credentials.username.c_str();
    { Preferences p; p.begin(NS_GPRS, false);
      p.putString(KEY_TOKEN,       GPRS.device_token);
      p.putUChar (KEY_PROVISIONED, GPRS.provisioned);
      p.end(); }
    logModemData("[GPRS] -> Device provisioned successfully");
  } else {
    logModemData("[GPRS] -> Unexpected provision credentialsType");
    return;
  }
  if (tb.connected()) {
    tb.disconnect();
  }
  GPRS.provision_request_processed = true;
}

void TBProvision() {
  if (!tb.connected()) {
    logModemData("[GPRS] -> Connecting for provision to: " +
                 String(THINGSBOARD_SERVER));
    if (!tb.connect(THINGSBOARD_SERVER, "provision", THINGSBOARD_PORT)) {
      logModemData("Failed to connect");
      return;
    }
  }
  // Connect to the ThingsBoard
  logModemData("[GPRS] -> Sending provision request to: " +
               String(THINGSBOARD_SERVER));
  String baseName = "IncuNest-" + String(in3.serialNumber);
  String deviceName = (GPRS.provision_retry_count == 0)
      ? baseName
      : baseName + "_" + String(GPRS.provision_retry_count);
  logModemData("[GPRS] -> Provisioning as: " + deviceName);
  const Provision_Callback provisionCallback(
      Access_Token(), &GPRSProvisionResponse, PROVISION_DEVICE_KEY,
      PROVISION_DEVICE_SECRET, deviceName.c_str());
  GPRS.provision_request_sent = tb.Provision_Request(provisionCallback);
}

void addIntVariableToTelemetryJSON(JsonObject &json, const char *key,
                                   const int &value) {
  if (value != 0) {
    json[key] = value;
  }
}

void GPRSCheckOTA() {
  logModemData("Checking GPRS firwmare Update...");
  if (!currentFWSent) {
    // Firmware state send at the start of the firmware, to inform the cloud
    // about the current firmware and that it was installed correctly,
    // especially important when using OTA update, because the OTA update sends
    // the last firmware state as UPDATING, meaning the device is restarting if
    // the device restarted correctly and has the new given firmware title and
    // version it should then send thoose to the cloud with the state UPDATED,
    // to inform any end user that the device has successfully restarted and
    // does actually contain the version it was flashed too
    currentFWSent = tb.Firmware_Send_Info(CURRENT_FIRMWARE_TITLE, FWversion) &&
                    tb.Firmware_Send_State(FW_STATE_UPDATED);
  }
  if (!updateRequestSent) {
    tb.Start_Firmware_Update(OTAcallback);
  }
}

void switchAlarmTelemetryGPRS(int alarm, bool value) {
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
  addVariableToTelemetryGPRSJSON[alarmKey] = value;
}

void addAlarmTelemetriesToGPRSJSON() {
  int alarmReported = 0;
  for (int i = NO_ALARMS + 1; i < NUM_ALARMS; i++) {
    if (in3.alarmToReport[i]) {
      switchAlarmTelemetryGPRS(i, true);
      alarmReported = true;
      in3.previousAlarmReport = true;
    }
  }
  if (!alarmReported) {
    if (in3.previousAlarmReport) {
      in3.previousAlarmReport = false;
      for (int i = NO_ALARMS + 1; i < NUM_ALARMS; i++) {
        switchAlarmTelemetryGPRS(i, false);
      }
    }
  }
}

void addConfigTelemetriesToGPRSJSON() {
  addAlarmTelemetriesToGPRSJSON();
  addVariableToTelemetryGPRSJSON[SN_KEY] = in3.serialNumber;
  addVariableToTelemetryGPRSJSON[SYSTEM_RESET_REASON] = in3.resetReason;
#if TX_GROUP_DIAG_GPRS // grupo DIAG — config/transport_policy.h
  addVariableToTelemetryGPRSJSON[BOOT_COUNT_KEY] = g_bootCount;
  addVariableToTelemetryGPRSJSON[GPRS_KILL_COUNT_KEY] = g_gprsKillCount;
  addVariableToTelemetryGPRSJSON[GPRS_MON_KILL_COUNT_KEY] = g_monKillCount;
  addVariableToTelemetryGPRSJSON[FREE_HEAP_KEY] = (uint32_t)ESP.getFreeHeap();
  addVariableToTelemetryGPRSJSON[MIN_FREE_HEAP_KEY] = (uint32_t)ESP.getMinFreeHeap();
  addVariableToTelemetryGPRSJSON[UPTIME_S_KEY] = (uint32_t)(millis() / 1000);
  addVariableToTelemetryGPRSJSON[HMI_BOOT_COUNT_KEY] = g_hmiBootCount;
  addVariableToTelemetryGPRSJSON[HMI_LAST_RST_KEY] = g_hmiLastRst;
#endif
  addVariableToTelemetryGPRSJSON[HW_NUM_KEY] = HW_NUM;
  addVariableToTelemetryGPRSJSON[HW_REV_KEY] = String(HW_REVISION);
  addVariableToTelemetryGPRSJSON[FW_VERSION_KEY] = FWversion;
  addVariableToTelemetryGPRSJSON[CCID_KEY] = GPRS.CCID.c_str();
#if TX_GROUP_CELLULAR_GPRS // grupo CELLULAR — config/transport_policy.h
  addVariableToTelemetryGPRSJSON[IMEI_KEY] = GPRS.IMEI.c_str();
  addVariableToTelemetryGPRSJSON[APN_KEY] = GPRS.APN.c_str();
  addVariableToTelemetryGPRSJSON[COP_KEY] = GPRS.COP.c_str();
#endif

  addVariableToTelemetryGPRSJSON[SYS_CURR_STANDBY_TEST_KEY] =
      roundSignificantDigits(in3.system_current_standby_test,
                             TELEMETRIES_DECIMALS);
  addVariableToTelemetryGPRSJSON[HEATER_CURR_TEST_KEY] =
      roundSignificantDigits(in3.heater_current_test, TELEMETRIES_DECIMALS);
  addVariableToTelemetryGPRSJSON[FAN_CURR_TEST_KEY] =
      roundSignificantDigits(in3.fan_current_test, TELEMETRIES_DECIMALS);
  addVariableToTelemetryGPRSJSON[FAN_FEEDBACK_PRESENT_KEY] =
      in3.fanHasSpeedFeedback;
  addVariableToTelemetryGPRSJSON[PHOTOTHERAPY_CURR_KEY] =
      roundSignificantDigits(in3.phototherapy_current_test,
                             TELEMETRIES_DECIMALS);
  addVariableToTelemetryGPRSJSON[PHOTOTHERAPY_PWM_KEY] =
      roundSignificantDigits(in3.phototherapy_intensity, TELEMETRIES_DECIMALS);
  addVariableToTelemetryGPRSJSON[HUMIDIFIER_CURR_KEY] =
      roundSignificantDigits(in3.humidifier_current_test, TELEMETRIES_DECIMALS);
  addVariableToTelemetryGPRSJSON[BUZZER_CURR_TEST_KEY] =
      roundSignificantDigits(in3.buzzer_current_test, TELEMETRIES_DECIMALS);
  addVariableToTelemetryGPRSJSON[HW_TEST_KEY] = in3.HW_test_error_code;

  addVariableToTelemetryGPRSJSON[UI_LANGUAGE_KEY] = in3.language;
  addVariableToTelemetryGPRSJSON[CALIBRATED_SENSOR_KEY] = !in3.calibrationError;
  addVariableToTelemetryGPRSJSON[GPRS_CONNECTIVITY_KEY] = true;
  addVariableToTelemetryGPRSJSON[WIFI_CONNECTIVITY_KEY] = false;

#if TX_GROUP_CALIBRATION_GPRS // grupo CALIBRATION — config/transport_policy.h
  addVariableToTelemetryGPRSJSON[CALIBRATION_REFERENCE_TEMPERATURE_RANGE_KEY] =
      roundSignificantDigits(ReferenceTemperatureRange, TELEMETRIES_DECIMALS);
  addVariableToTelemetryGPRSJSON[CALIBRATION_REFERENCE_TEMPERATURE_LOW_KEY] =
      roundSignificantDigits(ReferenceTemperatureLow, TELEMETRIES_DECIMALS);
  addVariableToTelemetryGPRSJSON[CALIBRATION_SKIN_FINETUNE_KEY] =
      roundSignificantDigits(in3.fineTuneSkinTemperature, TELEMETRIES_DECIMALS);
  addVariableToTelemetryGPRSJSON[CALIBRATION_AIR_FINETUNE_KEY] =
      roundSignificantDigits(in3.fineTuneAirTemperature, TELEMETRIES_DECIMALS);
  addVariableToTelemetryGPRSJSON[CALIBRATION_RAW_TEMPERATURE_RANGE_SKIN_KEY] =
      roundSignificantDigits(RawTemperatureRange[SKIN_SENSOR],
                             TELEMETRIES_DECIMALS);
  addVariableToTelemetryGPRSJSON[CALIBRATION_RAW_TEMPERATURE_LOW_SKIN_KEY] =
      roundSignificantDigits(RawTemperatureLow[SKIN_SENSOR],
                             TELEMETRIES_DECIMALS);
#endif // TX_GROUP_CALIBRATION_GPRS
}

void addTelemetriesToGPRSJSON() {
  addAlarmTelemetriesToGPRSJSON();
  if (GPRS.longitud || GPRS.latitud) {
    addVariableToTelemetryGPRSJSON[LOCATION_LONGTITUD_KEY] = GPRS.longitud;
    addVariableToTelemetryGPRSJSON[LOCATION_LATITUD_KEY] = GPRS.latitud;
    addVariableToTelemetryGPRSJSON[TRI_ACCURACY_KEY] = GPRS.accuracy;
  }
  addVariableToTelemetryGPRSJSON[SKIN_TEMPERATURE_KEY] = roundSignificantDigits(
      in3.temperature[SKIN_SENSOR], TELEMETRIES_DECIMALS);
  addVariableToTelemetryGPRSJSON[AIR_TEMPERATURE_KEY] = roundSignificantDigits(
      in3.temperature[ROOM_DIGITAL_TEMP_SENSOR], TELEMETRIES_DECIMALS);
  if (in3.airTemperatureRedundantSensor) {
    addVariableToTelemetryGPRSJSON[AIR_TEMPERATURE_REDUNDANT_KEY] =
        roundSignificantDigits(in3.airTemperatureRedundantSensor,
                               TELEMETRIES_DECIMALS);
  }
  if (in3.temperature[AMBIENT_DIGITAL_TEMP_SENSOR] &&
      in3.humidity[AMBIENT_DIGITAL_HUM_SENSOR]) {
    addVariableToTelemetryGPRSJSON[AMBIENT_TEMPERATURE_KEY] =
        roundSignificantDigits(in3.temperature[AMBIENT_DIGITAL_TEMP_SENSOR],
                               TELEMETRIES_DECIMALS);
    addVariableToTelemetryGPRSJSON[HUMIDITY_AMBIENT_KEY] =
        roundSignificantDigits(in3.humidity[AMBIENT_DIGITAL_HUM_SENSOR],
                               TELEMETRIES_DECIMALS);
  }
  addVariableToTelemetryGPRSJSON[PHOTOTHERAPY_ACTIVE_KEY] = in3.phototherapy;
  addVariableToTelemetryGPRSJSON[HUMIDITY_ROOM_KEY] = roundSignificantDigits(
      in3.humidity[ROOM_DIGITAL_HUM_SENSOR], TELEMETRIES_DECIMALS);
  addVariableToTelemetryGPRSJSON[SYSTEM_CURRENT_KEY] =
      roundSignificantDigits(in3.system_current, TELEMETRIES_DECIMALS);
  addVariableToTelemetryGPRSJSON[SYSTEM_VOLTAGE_KEY] =
      roundSignificantDigits(in3.system_voltage, TELEMETRIES_DECIMALS);
#if TX_GROUP_CELLULAR_GPRS // grupo CELLULAR — config/transport_policy.h
  addVariableToTelemetryGPRSJSON[CELL_SIGNAL_QUALITY_KEY] = GPRS.CSQ;
#endif
  addVariableToTelemetryGPRSJSON[V5_CURRENT_KEY] =
      roundSignificantDigits(in3.USB_current, TELEMETRIES_DECIMALS);
  addVariableToTelemetryGPRSJSON[V5_VOLTAGE_KEY] =
      roundSignificantDigits(in3.USB_voltage, TELEMETRIES_DECIMALS);
  addVariableToTelemetryGPRSJSON[BAT_CURRENT_KEY] =
      roundSignificantDigits(in3.BATTERY_current, TELEMETRIES_DECIMALS);
  addVariableToTelemetryGPRSJSON[BAT_VOLTAGE_KEY] =
      roundSignificantDigits(in3.BATTERY_voltage, TELEMETRIES_DECIMALS);

  if (chargerPresent && g_bq_status_valid &&
      g_bq_status.ac_present && g_bq_status.vbat_mv > 10000) {
    addVariableToTelemetryGPRSJSON[BQ_STATE_KEY] = (int)g_bq_status.state;
    addVariableToTelemetryGPRSJSON[BQ_FAULT_KEY] = g_bq_status.fault;
    addVariableToTelemetryGPRSJSON[BQ_AC_KEY]    = g_bq_status.ac_present;
    addVariableToTelemetryGPRSJSON[BQ_VBAT_KEY]  =
        roundSignificantDigits(g_bq_status.vbat_mv / 1000.0f, 3);
    addVariableToTelemetryGPRSJSON[BQ_VBUS_KEY]  =
        roundSignificantDigits(g_bq_status.vbus_mv / 1000.0f, 3);
    addVariableToTelemetryGPRSJSON[BQ_ICHG_KEY]  = (int)g_bq_status.ichg_ma;
  }

  if (in3.temperatureControl || in3.humidityControl) {
    addVariableToTelemetryGPRSJSON[FAN_CURRENT_KEY] =
        roundSignificantDigits(in3.fan_current, TELEMETRIES_DECIMALS);
    addVariableToTelemetryGPRSJSON[CONTROL_ACTIVE_TIME_KEY] =
        roundSignificantDigits(in3.control_active_time, TELEMETRIES_DECIMALS);
    addVariableToTelemetryGPRSJSON[FAN_ACTIVE_TIME_KEY] =
        roundSignificantDigits(in3.fan_active_time, TELEMETRIES_DECIMALS);
    if (in3.temperatureControl) {
      addVariableToTelemetryGPRSJSON[HEATER_CURRENT_KEY] =
          roundSignificantDigits(in3.heater_current, TELEMETRIES_DECIMALS);
      addVariableToTelemetryGPRSJSON[DESIRED_TEMPERATURE_KEY] =
          in3.desiredControlTemperature;
      addVariableToTelemetryGPRSJSON[HEATER_ACTIVE_TIME_KEY] =
          roundSignificantDigits(in3.heater_active_time, TELEMETRIES_DECIMALS);
    }
    if (in3.humidityControl) {
      addVariableToTelemetryGPRSJSON[DESIRED_HUMIDITY_ROOM_KEY] =
          in3.desiredControlHumidity;
    }
    if (!GPRS.firstConfigPost) {
      GPRS.firstConfigPost = true;
      addVariableToTelemetryGPRSJSON[CONTROL_ACTIVE_KEY] = true;
      if (in3.temperatureControl) {
        if (in3.controlMode == CONTROL_AIR) {
          addVariableToTelemetryGPRSJSON[CONTROL_MODE_KEY] = "AIR";
        } else {
          addVariableToTelemetryGPRSJSON[CONTROL_MODE_KEY] = "SKIN";
        }
        addVariableToTelemetryGPRSJSON[DESIRED_TEMPERATURE_KEY] =
            in3.desiredControlTemperature;
      }
      if (in3.humidityControl) {
        addVariableToTelemetryGPRSJSON[DESIRED_HUMIDITY_ROOM_KEY] =
            in3.desiredControlHumidity;
      }
    }
  } else {
    GPRS.firstConfigPost = false;
    addVariableToTelemetryGPRSJSON[CONTROL_ACTIVE_KEY] = false;
    addVariableToTelemetryGPRSJSON[STANBY_TIME_KEY] =
        roundSignificantDigits(in3.standby_time, TELEMETRIES_DECIMALS);
  }
  if (in3.humidityControl) {
    addVariableToTelemetryGPRSJSON[HUMIDIFIER_CURRENT_KEY] =
        roundSignificantDigits(in3.humidifier_current, TELEMETRIES_DECIMALS);
    addVariableToTelemetryGPRSJSON[HUMIDIFIER_VOLTAGE_KEY] =
        roundSignificantDigits(in3.humidifier_voltage, TELEMETRIES_DECIMALS);
    addVariableToTelemetryGPRSJSON[HUMIDIFIER_ACTIVE_TIME_KEY] =
        roundSignificantDigits(in3.humidifier_active_time,
                               TELEMETRIES_DECIMALS);
    addVariableToTelemetryGPRSJSON[DESIRED_HUMIDITY_ROOM_KEY] =
        in3.desiredControlHumidity;
  }
  if (in3.phototherapy) {
    addVariableToTelemetryGPRSJSON[PHOTOTHERAPY_CURRENT_KEY] =
        roundSignificantDigits(in3.phototherapy_current, TELEMETRIES_DECIMALS);
    addVariableToTelemetryGPRSJSON[PHOTHERAPY_ACTIVE_TIME_KEY] =
        roundSignificantDigits(in3.phototherapy_active_time,
                               TELEMETRIES_DECIMALS);
  }

  if (in3.fanCommandedOn) {
    // Only while an OTA is actually downloading, so the key does not
  // sit at a stale value between updates.
  extern volatile int g_otaProgressPct;
  if (g_otaProgressPct >= 0) {
    addVariableToTelemetryGPRSJSON[OTA_PROGRESS_KEY] = g_otaProgressPct;
  }
  addVariableToTelemetryGPRSJSON[FAN_RPM_KEY] = (int)(in3.fan_rpm + 0.5f);
    addVariableToTelemetryGPRSJSON[FAN_PWM_KEY] =
        fanControlPID.GetMode() == AUTOMATIC ? (int)(fanControlPIDOutput + 0.5)
                                             : in3.fanCtlPWM;
  }

  // Suppress SpO2/HR telemetry unless the probe is actually on the patient —
  // no probe or probe-present-but-not-applied readings aren't valid vitals.
  if (g_spo2_data.probe_state == ProbeState::PROBE_APPLIED) {
    if (g_spo2_data.spo2_sqi > 0.0f) {
      addVariableToTelemetryGPRSJSON[SPO2_KEY] =
          roundSignificantDigits(g_spo2_data.spo2, TELEMETRIES_DECIMALS);
      addVariableToTelemetryGPRSJSON[SPO2_SQI_KEY] =
          roundSignificantDigits(g_spo2_data.spo2_sqi, TELEMETRIES_DECIMALS);
      addVariableToTelemetryGPRSJSON[PI_KEY] =
          roundSignificantDigits(g_spo2_data.pi, TELEMETRIES_DECIMALS);
    }
    if (g_spo2_data.hr1_sqi > 0.0f) {
      addVariableToTelemetryGPRSJSON[HR1_KEY] = (int)(g_spo2_data.hr1 + 0.5f);
      addVariableToTelemetryGPRSJSON[HR1_SQI_KEY] =
          roundSignificantDigits(g_spo2_data.hr1_sqi, TELEMETRIES_DECIMALS);
    }
    if (g_spo2_data.hr2_sqi > 0.0f) {
      addVariableToTelemetryGPRSJSON[HR2_KEY] = (int)(g_spo2_data.hr2 + 0.5f);
      addVariableToTelemetryGPRSJSON[HR2_SQI_KEY] =
          roundSignificantDigits(g_spo2_data.hr2_sqi, TELEMETRIES_DECIMALS);
    }
    if (g_spo2_data.hr3_sqi > 0.0f) {
      addVariableToTelemetryGPRSJSON[HR3_KEY] = (int)(g_spo2_data.hr3 + 0.5f);
      addVariableToTelemetryGPRSJSON[HR3_SQI_KEY] =
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
static void publishBabyCloudDataGPRS() {
  char json[512];

  if (babyStore_attributesDirty()) {
    const BabyProfile *occ = babyStore_currentOccupant();
    int n = occ ? babyCloud_buildAttributesJson(occ, json, sizeof(json))
                : babyCloud_buildEmptyAttributesJson(json, sizeof(json));
    if (n > 0 && tb.sendAttributeJson(json)) {
      babyStore_clearAttributesDirty();
    }
  }

  BabyCloudEvent e;
  if (babyStore_peekCloudEvent(&e)) {
    int n = babyCloud_buildEventJson(&e, json, sizeof(json));
    if (n <= 0) {
      babyStore_popCloudEvent();  // unbuildable: drop rather than wedge
    } else if (tb.sendTelemetryJson(json)) {
      babyStore_popCloudEvent();
    }
  }
}

void GPRSPost() {
  if (!GPRS.provisioned) {
    if (in3.serialNumber == 0) {
      // GPRSPost() se llama cada pocos ms: sin limitar la cadencia, esta traza
      // inunda el log mientras el número de serie sigue sin conocerse.
      static uint32_t lastSerialWaitLog = 0;
      uint32_t now = millis();
      if (lastSerialWaitLog == 0 ||
          now - lastSerialWaitLog >= GPRS_SERIAL_WAIT_LOG_PERIOD) {
        lastSerialWaitLog = now;
        logModemData("[GPRS] -> Waiting for serial number before provisioning");
      }
      return;
    }
    if (!GPRS.provision_request_sent) {
      TBProvision();
    }
    tb.loop();
  } else {
    if (!tb.connected()) {
      if (millis() - GPRS.lastReconnectAttempt < THINGSBOARD_RECONNECT_DELAY) {
        return;
      }
      logModemData(
          "[GPRS] -> Connecting over GPRS to: " + String(THINGSBOARD_SERVER) +
          " with token " + String(GPRS.device_token));

      GPRS.lastReconnectAttempt = millis();
      if (!tb.connect(THINGSBOARD_SERVER, GPRS.device_token.c_str())) {
        logModemData("[GPRS] -> Failed to connect");
        return;
      } else {
        logModemData("[GPRS] -> Connected to host");
        GPRS.serverConnectionStatus = true;
        subscribeRPCHandlers();
        if (ENABLE_GPRS_OTA && !GPRS.OTA_requested) {
          logModemData("[GPRS] -> Requesting OTA");
          GPRSCheckOTA();
          GPRS.OTA_requested = true;
          GPRS.lastOTACheck = millis();
        }
      }
    }
    if (tb.connected()) {
      if (millis() - GPRS.lastSent > secsToMillis(GPRS.sendPeriod)) {
        // Send our firmware title and version
        logModemData("[GPRS] -> sendPeriod is " + String(GPRS.sendPeriod) +
                     " secs");
        logModemData("[GPRS] -> Posting GPRS data...");

        if (!GPRS.firstPublish) {
          GPRS.firstPublish = true;
          addConfigTelemetriesToGPRSJSON();
          if (tb.sendTelemetryJson(addVariableToTelemetryGPRSJSON,
                                   JSON_STRING_SIZE(measureJson(
                                       addVariableToTelemetryGPRSJSON)))) {
            logModemData("[GPRS] -> GPRS MQTT PUBLISH CONFIG SUCCESS");
          } else {
            logModemData("[GPRS] -> GPRS MQTT PUBLISH CONFIG FAIL");
          }
          GPRS_JSON.clear();
        }
        GPRSUpdateLocationIfDue();
        GPRSEnsureTimeSynced();
        GPRSEnsureTimeZoneSynced();
        GPRSUpdateCSQ();
        addTelemetriesToGPRSJSON();
        if (tb.sendTelemetryJson(addVariableToTelemetryGPRSJSON,
                                 JSON_STRING_SIZE(measureJson(
                                     addVariableToTelemetryGPRSJSON)))) {
          logModemData("[GPRS] -> GPRS MQTT PUBLISH TELEMETRIES SUCCESS");
        } else {
          logModemData("[GPRS] -> GPRS MQTT PUBLISH TELEMETRIES FAIL");
        }
        GPRS_JSON.clear();
        publishBabyCloudDataGPRS();
#if TX_FEATURE_PPG_SNAPSHOT_GPRS
        // Va después de la telemetría normal a propósito: son ~23 KB por
        // SIM800, así que si algo se va a atascar, que no sea lo clínico.
#if TX_FEATURE_PPG_AUTOCAPTURE_GPRS
        if (millis() - GPRS.lastPpgSnapshotAttempt >
            PPG_SNAPSHOT_AUTO_INTERVAL_MS) {
          GPRS.lastPpgSnapshotAttempt = millis();
          ppgSnapshotRequestCapture(
              g_spo2_data.probe_state == ProbeState::PROBE_APPLIED,
              g_spo2_data.rsqi, millis());
        }
#endif
        ppgSnapshotPublish(tb, "GPRS");
#endif
        GPRS.process = false;
        GPRS.lastSent = millis();
      }
      if (millis() - GPRS.lastOTACheck > GPRS_OTA_CHECK_INTERVAL) {
        GPRSCheckOTA();
        GPRS.lastOTACheck = millis();
      }
    }
  }
}

void GPRS_TB_Init() {
  { Preferences p; p.begin(NS_GPRS, true);
    GPRS.provisioned   = p.getUChar (KEY_PROVISIONED, 0);
    if (GPRS.provisioned) {
      GPRS.device_token = p.getString(KEY_TOKEN, "").c_str();
    }
    p.end(); }
}

void GPRS_Handler() {
  GPRSStatusHandler();
  if (GPRS.powerUp) {
    GPRSPowerUp();
  }
  // Keep attaching to the cellular network regardless of WiFi status: even
  // when WiFi carries telemetry to ThingsBoard, the GPRS/PDP context is kept
  // up so GSM-based location stays available (see GPRSUpdateLocationIfDue()).
  if (GPRS.connect) {
    GPRSStablishConnection();
  }
  if (WIFIIsConnected()) {
    // WiFi already owns the ThingsBoard connection/publish; don't duplicate
    // it over cellular, just keep location fresh off the existing attach.
    GPRSUpdateLocationIfDue();
    GPRSEnsureTimeSynced();
    GPRSEnsureTimeZoneSynced();
  } else if (GPRS.post) {
    GPRSSetPostPeriod();
    GPRSPost();
    tb.loop();
  }
}