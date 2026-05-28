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
#include <Preferences.h>

#include "main.h"

extern bool autoLock;
extern bool WIFI_EN;
extern int presetTemp[2]; // preset baby skin temperature
extern double RawTemperatureLow[SENSOR_TEMP_QTY],
    RawTemperatureRange[SENSOR_TEMP_QTY];
extern double ReferenceTemperatureRange, ReferenceTemperatureLow;

extern IncuNest_parameters in3;

void resetFlash() {
  for (int i = false; i < EEPROM_SIZE; i++) {
    EEPROM.write(i, 0);
    EEPROM.commit();
  }
}

// Magic byte to indicate EEPROM is initialized
#define EEPROM_MAGIC_BYTE 0xAB

void initEEPROM() {
  if (!EEPROM.begin(EEPROM_SIZE)) {
    logE("failed to initialise EEPROM");
    return;
  }

  // Check if EEPROM has been initialized with our magic byte
  if (EEPROM.read(EEPROM_FIRST_TURN_ON) != EEPROM_MAGIC_BYTE) {
    logI("[FLASH] -> First turn on or uninitialized, resetting flash...");
    resetFlash();
    loaddefaultValues();

    // Mark as initialized
    EEPROM.write(EEPROM_FIRST_TURN_ON, EEPROM_MAGIC_BYTE);
    EEPROM.commit();

    logI("[FLASH] -> Default values loaded and flash marked as initialized");
  } else {
    logI("[FLASH] -> Loading variables stored in flash");
    recapVariables();
    logI("[FLASH] -> Variables loaded");
  }
}

void loaddefaultValues() {
  autoLock = DEFAULT_AUTOLOCK;
  in3.language = defaultLanguage;
  in3.controlMode = CONTROL_AIR;
  in3.desiredControlTemperature = presetTemp[in3.controlMode];
  in3.desiredControlHumidity = presetHumidity;

  // Initialize calibration variables to safe defaults
  ReferenceTemperatureRange = 0.0;
  ReferenceTemperatureLow = 0.0;
  for (int i = 0; i < SENSOR_TEMP_QTY; i++) {
    RawTemperatureLow[i] = 0.0;
    RawTemperatureRange[i] = 0.0;
  }

  EEPROM.write(EEPROM_AUTO_LOCK, autoLock);
  EEPROM.write(EEPROM_LANGUAGE, in3.language);
  EEPROM.write(EEPROM_CONTROL_MODE, in3.controlMode);
  EEPROM.writeFloat(EEPROM_DESIRED_CONTROL_TEMPERATURE,
                    in3.desiredControlTemperature);
  EEPROM.writeFloat(EEPROM_FINE_TUNE_TEMP_SKIN, in3.fineTuneSkinTemperature);

  // Save calibration defaults
  EEPROM.writeFloat(EEPROM_REFERENCE_TEMP_RANGE, ReferenceTemperatureRange);
  EEPROM.writeFloat(EEPROM_REFERENCE_TEMP_LOW, ReferenceTemperatureLow);
  EEPROM.writeFloat(EEPROM_RAW_SKIN_TEMP_LOW_CORRECTION,
                    RawTemperatureLow[SKIN_SENSOR]);
  EEPROM.writeFloat(EEPROM_RAW_SKIN_TEMP_RANGE_CORRECTION,
                    RawTemperatureRange[SKIN_SENSOR]);

  in3.fanCtlPWM = FAN_CTL_PWM_DEFAULT;
  EEPROM.writeInt(EEPROM_FAN_CTL_PWM, in3.fanCtlPWM);

  EEPROM.commit();
}

void resetCalibration() {
  RawTemperatureLow[SKIN_SENSOR] = false;
  RawTemperatureRange[SKIN_SENSOR] = false;
  ReferenceTemperatureRange = false;
  ReferenceTemperatureLow = false;
  in3.fineTuneSkinTemperature = false;
  in3.fineTuneAirTemperature = false;
}

void recapVariables() {
  autoLock = EEPROM.read(EEPROM_AUTO_LOCK);
  in3.language = EEPROM.read(EEPROM_LANGUAGE);
  RawTemperatureLow[SKIN_SENSOR] =
      EEPROM.readFloat(EEPROM_RAW_SKIN_TEMP_LOW_CORRECTION);
  RawTemperatureRange[SKIN_SENSOR] =
      EEPROM.readFloat(EEPROM_RAW_SKIN_TEMP_RANGE_CORRECTION);
  ReferenceTemperatureRange = EEPROM.readFloat(EEPROM_REFERENCE_TEMP_RANGE);
  ReferenceTemperatureLow = EEPROM.readFloat(EEPROM_REFERENCE_TEMP_LOW);
  in3.fineTuneSkinTemperature = EEPROM.readFloat(EEPROM_FINE_TUNE_TEMP_SKIN);
  if (isnan(in3.fineTuneSkinTemperature)) {
    in3.fineTuneSkinTemperature = 0.0;
  }
  in3.fineTuneAirTemperature = EEPROM.readFloat(EEPROM_FINE_TUNE_TEMP_AIR);
  if (isnan(in3.fineTuneAirTemperature)) {
    in3.fineTuneAirTemperature = 0.0;
  }
  in3.standby_time = EEPROM.readFloat(EEPROM_STANDBY_TIME);
  in3.control_active_time = EEPROM.readFloat(EEPROM_CONTROL_ACTIVE_TIME);
  in3.heater_active_time = EEPROM.readFloat(EEPROM_HEATER_ACTIVE_TIME);
  in3.fan_active_time = EEPROM.readFloat(EEPROM_FAN_ACTIVE_TIME);
  in3.humidifier_active_time = EEPROM.readFloat(EEPROM_HUMIDIFIER_ACTIVE_TIME);
  in3.phototherapy_active_time =
      EEPROM.readFloat(EEPROM_PHOTOTHERAPY_ACTIVE_TIME);

  for (int i = 0; i < SENSOR_TEMP_QTY; i++) {
    logI("calibration factors: " + String(RawTemperatureLow[i]) + "," +
         String(RawTemperatureRange[i]) + "," +
         String(ReferenceTemperatureRange) + "," +
         String(ReferenceTemperatureLow));
  }

  if (!ReferenceTemperatureRange) {
    in3.calibrationError = true;
    logE("[HW] -> Fail -> temperature sensor is not calibrated");
  }

  for (int i = 0; i < SENSOR_TEMP_QTY; i++) {
    if (RawTemperatureLow[i] > 100) {
      // critical error
    }
  }
  in3.serialNumber = EEPROM.readInt(EEPROM_SERIAL_NUMBER);
  ESP_LOGI("APP", "Serial Number from EEPROM: %d", in3.serialNumber);

  // Read config settings from EEPROM (with validation against defaults)
  int fanPWM_ee = EEPROM.readInt(EEPROM_FAN_PWM);
  if (fanPWM_ee > 0 && fanPWM_ee <= 255)
    in3.fanPWM = fanPWM_ee;
  float heaterAmps_ee = EEPROM.readFloat(EEPROM_HEATER_MAX_AMPS);
  if (!isnan(heaterAmps_ee) && heaterAmps_ee > 0)
    in3.heaterMaxPowerAmps = heaterAmps_ee;
  float skinTmax_ee = EEPROM.readFloat(EEPROM_SKIN_TEMP_MAX);
  if (!isnan(skinTmax_ee) && skinTmax_ee > 0)
    in3.skinTemperatureSetMax = skinTmax_ee;
  float airTmax_ee = EEPROM.readFloat(EEPROM_AIR_TEMP_MAX);
  if (!isnan(airTmax_ee) && airTmax_ee > 0)
    in3.airTemperatureSetMax = airTmax_ee;
  int gprsAct_ee = EEPROM.readInt(EEPROM_GPRS_ACT_PERIOD);
  if (gprsAct_ee > 0)
    in3.actuating_gprs_period = gprsAct_ee;
  int gprsPhoto_ee = EEPROM.readInt(EEPROM_GPRS_PHOTO_PERIOD);
  if (gprsPhoto_ee > 0)
    in3.phototherapy_gprs_period = gprsPhoto_ee;
  int gprsStby_ee = EEPROM.readInt(EEPROM_GPRS_STBY_PERIOD);
  if (gprsStby_ee > 0)
    in3.standby_gprs_period = gprsStby_ee;
  int fanCtlPWM_ee = EEPROM.readInt(EEPROM_FAN_CTL_PWM);
  if (fanCtlPWM_ee > 0 && fanCtlPWM_ee <= 255)
    in3.fanCtlPWM = fanCtlPWM_ee;

  in3.controlMode = EEPROM.read(EEPROM_CONTROL_MODE);
  ESP_LOGI("APP", "Control Mode from EEPROM: %d", in3.controlMode);
  in3.desiredControlTemperature =
      EEPROM.readFloat(EEPROM_DESIRED_CONTROL_TEMPERATURE);

  // Validation
  if (in3.controlMode == CONTROL_AIR) {
    if (isnan(in3.desiredControlTemperature) ||
        in3.desiredControlTemperature < AIR_TEMPERATURE_SET_MIN ||
        in3.desiredControlTemperature > in3.airTemperatureSetMax)
      in3.desiredControlTemperature = presetTemp[CONTROL_AIR];
  } else {
    if (isnan(in3.desiredControlTemperature) ||
        in3.desiredControlTemperature < SKIN_TEMPERATURE_SET_MIN ||
        in3.desiredControlTemperature > in3.skinTemperatureSetMax)
      in3.desiredControlTemperature = presetTemp[CONTROL_SKIN];
  }

  ESP_LOGI("APP", "Control Temperature from EEPROM: %f",
           in3.desiredControlTemperature);
  in3.desiredControlHumidity = EEPROM.read(EEPROM_DESIRED_CONTROL_HUMIDITY);

  if (in3.desiredControlHumidity < minHum ||
      in3.desiredControlHumidity > maxHum)
    in3.desiredControlHumidity = presetHumidity;

  ESP_LOGI("APP", "Control Humidity from EEPROM: %d",
           in3.desiredControlHumidity);

  extern char wifi_ssid[64];
  extern char wifi_pass[64];

  String ssid = EEPROM.readString(EEPROM_WIFI_SSID);
  String pass = EEPROM.readString(EEPROM_WIFI_PASSWORD);
  strncpy(wifi_ssid, ssid.c_str(), sizeof(wifi_ssid));
  strncpy(wifi_pass, pass.c_str(), sizeof(wifi_pass));
  // logI("[WIFI] -> Read SSID: " + String(wifi_ssid));
  ESP_LOGI("APP", "WiFi SSID from EEPROM: %s", wifi_ssid);
  ESP_LOGI("APP", "WiFi Pass from EEPROM: %s", wifi_pass);

  if (in3.restoreState) {
    in3.actuation = EEPROM.read(EEPROM_CONTROL_ACTIVE);
    in3.phototherapy = EEPROM.read(EEPROM_PHOTOTHERAPY_ACTIVE);

    if (in3.phototherapy) {
      Preferences p;
      p.begin("photo", true);
      bool was_active = p.getBool("active", false);
      int  saved_mins = p.getInt("mins", 0);
      p.end();
      if (was_active && saved_mins > 0) {
        g_restore_photo_minutes = saved_mins;
        logI("[HW] -> restoreState: phototherapy timer restored (" +
             String(saved_mins) + " min remaining)");
      }
    }

    switch (in3.actuation) {
    case ACTUATION_TEMPERATURE:
      in3.temperatureControl = true;
      in3.humidityControl = false;
      break;
    case ACTUATION_HUMIDITY:
      in3.temperatureControl = false;
      in3.humidityControl = true;
      break;
    case ACTUATION_TEMP_AND_HUMIDITY:
      in3.temperatureControl = true;
      in3.humidityControl = true;
      break;
    default:
      in3.temperatureControl = false;
      in3.humidityControl = false;
      // ACTUATION_NONE is a valid crash-recovery state — still skip actuatorsTest
      break;
    }
  } else {
    EEPROM.write(EEPROM_CONTROL_ACTIVE, false);
    EEPROM.commit();
  }
}

void saveCalibrationToEEPROM() {
  EEPROM.writeFloat(EEPROM_RAW_SKIN_TEMP_LOW_CORRECTION,
                    RawTemperatureLow[SKIN_SENSOR]);
  EEPROM.writeFloat(EEPROM_RAW_SKIN_TEMP_RANGE_CORRECTION,
                    RawTemperatureRange[SKIN_SENSOR]);
  EEPROM.writeFloat(EEPROM_REFERENCE_TEMP_RANGE, ReferenceTemperatureRange);
  EEPROM.writeFloat(EEPROM_REFERENCE_TEMP_LOW, ReferenceTemperatureLow);
  EEPROM.writeFloat(EEPROM_FINE_TUNE_TEMP_SKIN, in3.fineTuneSkinTemperature);
  EEPROM.writeFloat(EEPROM_FINE_TUNE_TEMP_AIR, in3.fineTuneAirTemperature);
  EEPROM.commit();
}
