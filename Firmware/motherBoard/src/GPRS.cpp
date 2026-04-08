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

extern in3ator_parameters in3;

GPRSstruct GPRS;
Credentials credentials;
Espressif_Updater updater_GPRS;

// Statuses for updating
bool currentFWSent = false;
bool updateRequestSent = false;

extern double ReferenceTemperatureRange, ReferenceTemperatureLow;

extern double RawTemperatureLow[SENSOR_TEMP_QTY],
    RawTemperatureRange[SENSOR_TEMP_QTY];

void progressCallback(const uint32_t &currentChunk,
                      const uint32_t &totalChuncks) {
  if (LOG_GPRS) {
    char buffer[50]; // Create a buffer to hold the formatted string
    snprintf(buffer, sizeof(buffer), "Progress %.2f%%",
             static_cast<float>(currentChunk * 100U) / totalChuncks);
    logCon(String(buffer)); // Pass the formatted string to logCon
  }
  GPRS.OTAInProgress = true;
}

void updatedCallback(const bool &success) {
  if (success) {
    logCon("[GPRS] -> Done, OTA will be implemented on next boot");
    // esp_restart();
  } else {
    logCon("[GPRS] -> No new firmware");
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
  if (strstr(GPRS.buffer, success)) {
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
    logCon("[GPRS] -> Abnormal reset detected (" + String(reason) +
           "), deleting GPRS task this session");
    // Delete the *current* task (i.e. GPRS_Task)
    vTaskDelete(NULL);
    // Note: control never returns here
  }

  // Normal power‑up path:
  Serial2.begin(MODEM_BAUD, SERIAL_8N1, GSM_UART_TX_PIN, GSM_UART_RX_PIN);
  GPRS.powerUp = true;
#if (GPRS_PWRKEY)
  GPIOWrite(GPRS_PWRKEY, HIGH);
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

void GPRS_get_SIM_info() {

  GPRS.IMSI = modem.getIMSI();

  GPRS.COP = modem.getOperator();

  GPRS.IP = modem.localIP();

  logCon("[GPRS] -> IMSI is: " + GPRS.IMSI);
  logCon("[GPRS] -> COP is: " + GPRS.COP);
}

void GPRSUpdateCSQ() {
  GPRS.CSQ = modem.getSignalQuality();
  logCon("[GPRS] -> CSQ is: " + String(GPRS.CSQ));
}

void readGPRSData() {
  while (Serial2.available()) {
    GPRS.buffer[GPRS.bufferWritePos] = Serial2.read();
    if (LOG_GPRS) {
      debugSerial.print(GPRS.buffer[GPRS.bufferWritePos]);
    }
    GPRS.bufferWritePos++;
    if (GPRS.bufferWritePos >= RX_BUFFER_LENGTH) {
      GPRS.bufferWritePos = 0;
      logCon("[GPRS] -> Buffer overflow");
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
        logCon("[GPRS] -> powering module down...");
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
    GPIOWrite(GPRS_PWRKEY, LOW);
#endif
    GPRS.process++;
    GPRS.packetSentenceTime = millis();
    logCon("[GPRS] -> powering up GPRS");
    break;
  case 1:
#if (GPRS_PWRKEY)
    if (millis() - GPRS.packetSentenceTime > 1000) {
      GPIOWrite(GPRS_PWRKEY, HIGH);
      logCon("[GPRS] -> GPRS powered");
    }
#endif
    GPRS.process++;
    break;
  case 2:
    if (millis() - GPRS.packetSentenceTime > 1000) {
      clearGPRSBuffer();
      logCon("[GPRS] -> Sending AT command");
      Serial2.print(SIMCOM800_ASK_CPIN);
      GPRS.packetSentenceTime = millis();
    }
    if (strstr(GPRS.buffer, AT_CPIN_SIM_PIN)) {
      logCon("[GPRS] -> SIM PIN required, unlocking...");
      Serial2.print(SIMCOM800_ENTER_PIN);
      clearGPRSBuffer();
      GPRS.packetSentenceTime = 0; // force re-query after 1 s
    }
    checkSerial(AT_CPIN_READY, AT_ERROR);
    break;
  case 3:
    logCon("[GPRS] -> Power up success");
    GPRS.CCID = modem.getSimCCID();
    GPRS.CCID.remove(GPRS.CCID.length() - 1);
    GPRS.IMEI = modem.getIMEI();
    logModemData("[GPRS] -> CCID is: " + GPRS.CCID);
    logModemData("[GPRS] -> IMEI is: " + GPRS.IMEI);
    if (GPRS.firstPowerUp) {
      buzzerTone(3, buzzerStandbyToneDuration, buzzerStandbyTone);
      GPRS.firstPowerUp = false;
    }
    GPRS.powerUp = false;
    GPRS.connect = true;
    GPRS.process = false;
    GPRS.APN = APN_TM;
    break;
  }
}

void GPRSStablishConnection() {
  switch (GPRS.process) {
  case 0:
    logCon("[GPRS] -> Stablishing connection...");
    GPRS.processTime = millis();
    GPRS.packetSentenceTime = millis();
    GPRS.process++;
    break;
  case 1:
    logCon("[GPRS] -> Connecting...");
    if (modem.gprsConnect(GPRS.APN.c_str(), GPRS_USER, GPRS_PASS)) {
      logCon("[GPRS] -> Attached");
      GPRS.process++;
    } else {
      logCon("[GPRS] -> Attach FAIL, retrying with different APN...");
      if (GPRS.APN == APN_TM) {
        GPRS.APN = APN_TRUPHONE;
      } else {
        GPRS.APN = APN_TM;
      }
      logCon("[GPRS] -> New APN: " + GPRS.APN);
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
  logCon("[GPRS] -> Received device provision response");
  const size_t jsonSize = Helper::Measure_Json(data);
  char buffer[jsonSize];
  serializeJson(data, buffer, jsonSize);
  // logCon("[GPRS] -> " + String(buffer));
  if (strncmp(data["status"], "SUCCESS", strlen("SUCCESS")) != 0) {
    logCon("[GPRS] -> Provision response contains the error: ");
    logCon("[GPRS] -> " + data["errorMsg"].as<String>());
    return;
  }

  if (strncmp(data[CREDENTIALS_TYPE], ACCESS_TOKEN_CRED_TYPE,
              strlen(ACCESS_TOKEN_CRED_TYPE)) == 0) {

    credentials.client_id = "";
    credentials.username = data[CREDENTIALS_VALUE].as<std::string>();
    credentials.password = "";
    GPRS.provisioned = true;
    GPRS.device_token = credentials.username.c_str();
    EEPROM.writeString(EEPROM_THINGSBOARD_TOKEN, GPRS.device_token);
    EEPROM.write(EEPROM_THINGSBOARD_PROVISIONED, GPRS.provisioned);
    EEPROM.commit();
    logCon("[GPRS] -> Device provisioned successfully");
  } else if (strncmp(data[CREDENTIALS_TYPE], MQTT_BASIC_CRED_TYPE,
                     strlen(MQTT_BASIC_CRED_TYPE)) == 0) {
    auto credentials_value = data[CREDENTIALS_VALUE].as<JsonObjectConst>();
    credentials.client_id = credentials_value[CLIENT_ID].as<std::string>();
    credentials.username = credentials_value[CLIENT_USERNAME].as<std::string>();
    credentials.password = credentials_value[CLIENT_PASSWORD].as<std::string>();
    GPRS.provisioned = true;
    GPRS.device_token = credentials.username.c_str();
    EEPROM.writeString(EEPROM_THINGSBOARD_TOKEN, GPRS.device_token);
    EEPROM.write(EEPROM_THINGSBOARD_PROVISIONED, GPRS.provisioned);
    EEPROM.commit();
    logCon("[GPRS] -> Device provisioned successfully");
  } else {
    logCon("[GPRS] -> Unexpected provision credentialsType");
    return;
  }
  if (tb.connected()) {
    tb.disconnect();
  }
  GPRS.provision_request_processed = true;
}

void TBProvision() {
  if (!tb.connected()) {
    logCon("[GPRS] -> Connecting for provision to: " +
           String(THINGSBOARD_SERVER));
    if (!tb.connect(THINGSBOARD_SERVER, "provision", THINGSBOARD_PORT)) {
      logCon("Failed to connect");
      return;
    }
  }
  // Connect to the ThingsBoard
  logCon("[GPRS] -> Sending provision request to: " +
         String(THINGSBOARD_SERVER));
  const Provision_Callback provisionCallback(
      Access_Token(), &GPRSProvisionResponse, PROVISION_DEVICE_KEY,
      PROVISION_DEVICE_SECRET, GPRS.CCID.c_str());
  GPRS.provision_request_sent = tb.Provision_Request(provisionCallback);
}

void addIntVariableToTelemetryJSON(JsonObject &json, const char *key,
                                   const int &value) {
  if (value != 0) {
    json[key] = value;
  }
}

void GPRSCheckOTA() {
  logCon("Checking GPRS firwmare Update...");
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
    // updateRequestSent = tb.Subscribe_Firmware_Update(callback);
  }
}

void switchAlarmTelemetryGPRS(int alarm, bool value) {
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
  addVariableToTelemetryGPRSJSON[alarmKey] = value;
}

void addAlarmTelemetriesToGPRSJSON() {
  int alarmReported = false;
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
  addVariableToTelemetryGPRSJSON[HW_NUM_KEY] = HW_NUM;
  addVariableToTelemetryGPRSJSON[HW_REV_KEY] = String(HW_REVISION);
  addVariableToTelemetryGPRSJSON[FW_VERSION_KEY] = FWversion;
  addVariableToTelemetryGPRSJSON[CCID_KEY] = GPRS.CCID.c_str();
  addVariableToTelemetryGPRSJSON[IMEI_KEY] = GPRS.IMEI.c_str();
  addVariableToTelemetryGPRSJSON[APN_KEY] = GPRS.APN.c_str();
  addVariableToTelemetryGPRSJSON[COP_KEY] = GPRS.COP.c_str();

  addVariableToTelemetryGPRSJSON[SYS_CURR_STANDBY_TEST_KEY] =
      roundSignificantDigits(in3.system_current_standby_test,
                             TELEMETRIES_DECIMALS);
  addVariableToTelemetryGPRSJSON[HEATER_CURR_TEST_KEY] =
      roundSignificantDigits(in3.heater_current_test, TELEMETRIES_DECIMALS);
  addVariableToTelemetryGPRSJSON[FAN_CURR_TEST_KEY] =
      roundSignificantDigits(in3.fan_current_test, TELEMETRIES_DECIMALS);
  addVariableToTelemetryGPRSJSON[PHOTOTHERAPY_CURR_KEY] =
      roundSignificantDigits(in3.phototherapy_current_test,
                             TELEMETRIES_DECIMALS);
  addVariableToTelemetryGPRSJSON[PHOTOTHERAPY_PWM_KEY] =
      roundSignificantDigits(in3.phototherapy_intensity, TELEMETRIES_DECIMALS);
  addVariableToTelemetryGPRSJSON[HUMIDIFIER_CURR_KEY] =
      roundSignificantDigits(in3.humidifier_current_test, TELEMETRIES_DECIMALS);
  addVariableToTelemetryGPRSJSON[DISPLAY_CURR_TEST_KEY] =
      roundSignificantDigits(in3.display_current_test, TELEMETRIES_DECIMALS);
  addVariableToTelemetryGPRSJSON[BUZZER_CURR_TEST_KEY] =
      roundSignificantDigits(in3.buzzer_current_test, TELEMETRIES_DECIMALS);
  addVariableToTelemetryGPRSJSON[HW_TEST_KEY] = in3.HW_test_error_code;

  addVariableToTelemetryGPRSJSON[UI_LANGUAGE_KEY] = in3.language;
  addVariableToTelemetryGPRSJSON[CALIBRATED_SENSOR_KEY] = !in3.calibrationError;
  addVariableToTelemetryGPRSJSON[GPRS_CONNECTIVITY_KEY] = true;
  addVariableToTelemetryGPRSJSON[WIFI_CONNECTIVITY_KEY] = false;

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
}

void addTelemetriesToGPRSJSON() {
  addAlarmTelemetriesToGPRSJSON();
  if (GPRS.longitud || GPRS.latitud) {
    addVariableToTelemetryGPRSJSON[LOCATION_LONGTITUD_KEY] = GPRS.longitud;
    addVariableToTelemetryGPRSJSON[LOCATION_LATITUD_KEY] = GPRS.latitud;
    addVariableToTelemetryGPRSJSON[TRI_ACCURACY_KEY] = GPRS.accuracy;
  }
  addVariableToTelemetryGPRSJSON[SKIN_CAPACITANCE_KEY] =
      in3.skinSensorCapacitance;
  addVariableToTelemetryGPRSJSON[SKIN_TEMPERATURE_KEY] = roundSignificantDigits(
      in3.temperature[SKIN_SENSOR], TELEMETRIES_DECIMALS);
  addVariableToTelemetryGPRSJSON[AIR_TEMPERATURE_KEY] = roundSignificantDigits(
      in3.temperature[ROOM_DIGITAL_TEMP_SENSOR], TELEMETRIES_DECIMALS);
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
  addVariableToTelemetryGPRSJSON[CELL_SIGNAL_QUALITY_KEY] = GPRS.CSQ;
  addVariableToTelemetryGPRSJSON[V5_CURRENT_KEY] =
      roundSignificantDigits(in3.USB_current, TELEMETRIES_DECIMALS);
  addVariableToTelemetryGPRSJSON[V5_VOLTAGE_KEY] =
      roundSignificantDigits(in3.USB_voltage, TELEMETRIES_DECIMALS);
  addVariableToTelemetryGPRSJSON[BAT_CURRENT_KEY] =
      roundSignificantDigits(in3.BATTERY_current, TELEMETRIES_DECIMALS);
  addVariableToTelemetryGPRSJSON[BAT_VOLTAGE_KEY] =
      roundSignificantDigits(in3.BATTERY_voltage, TELEMETRIES_DECIMALS);

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
}

void GPRSPost() {
  if (!GPRS.provisioned) {
    if (!GPRS.provision_request_sent) {
      TBProvision();
    }
    tb.loop();
  } else {
    if (!tb.connected()) {
      if (millis() - GPRS.lastReconnectAttempt < THINGSBOARD_RECONNECT_DELAY) {
        return;
      }
      logCon(
          "[GPRS] -> Connecting over GPRS to: " + String(THINGSBOARD_SERVER) +
          " with token " + String(GPRS.device_token));

      GPRS.lastReconnectAttempt = millis();
      if (!tb.connect(THINGSBOARD_SERVER, GPRS.device_token.c_str())) {
        logCon("[GPRS] -> Failed to connect");
        return;
      } else {
        logCon("[GPRS] -> Connected to host");
        GPRS.serverConnectionStatus = true;
        if (ENABLE_GPRS_OTA && !GPRS.OTA_requested) {
          logCon("[GPRS] -> Requesting OTA");
          GPRSCheckOTA();
          GPRS.OTA_requested = true;
          GPRS.lastOTACheck = millis();
        }
      }
    }
    if (tb.connected()) {
      if (millis() - GPRS.lastSent > secsToMillis(GPRS.sendPeriod)) {
        // Send our firmware title and version
        // StaticJsonDocument<JSON_OBJECT_SIZE(2)> TB_telemetries;
        // JsonObject telemetriesObject = TB_telemetries.to<JsonObject>();

        logCon("[GPRS] -> sendPeriod is " + String(GPRS.sendPeriod) + " secs");
        logCon("[GPRS] -> Posting GPRS data...");

        if (!GPRS.firstPublish) {
          GPRS.firstPublish = true;
          addConfigTelemetriesToGPRSJSON();
          if (tb.sendTelemetryJson(addVariableToTelemetryGPRSJSON,
                                   JSON_STRING_SIZE(measureJson(
                                       addVariableToTelemetryGPRSJSON)))) {
            logCon("[GPRS] -> GPRS MQTT PUBLISH CONFIG SUCCESS");
          } else {
            logCon("[GPRS] -> GPRS MQTT PUBLISH CONFIG FAIL");
          }
          GPRS_JSON.clear();
        }
        GPRS_get_triangulation_location();
        GPRSUpdateCSQ();
        addTelemetriesToGPRSJSON();
        if (tb.sendTelemetryJson(addVariableToTelemetryGPRSJSON,
                                 JSON_STRING_SIZE(measureJson(
                                     addVariableToTelemetryGPRSJSON)))) {
          logCon("[GPRS] -> GPRS MQTT PUBLISH TELEMETRIES SUCCESS");
        } else {
          logCon("[GPRS] -> GPRS MQTT PUBLISH TELEMETRIES FAIL");
        }
        GPRS_JSON.clear();
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
  GPRS.provisioned = EEPROM.read(EEPROM_THINGSBOARD_PROVISIONED);
  if (GPRS.provisioned) {
    GPRS.device_token = EEPROM.readString(EEPROM_THINGSBOARD_TOKEN);
  }
}

void GPRS_Handler() {
  GPRSStatusHandler();
  if (GPRS.powerUp) {
    GPRSPowerUp();
  }
  if (!WIFIIsConnected()) {
    if (GPRS.connect) {
      GPRSStablishConnection();
    }
    if (GPRS.post) {
      GPRSSetPostPeriod();
      GPRSPost();
      tb.loop();
    }
  }
}