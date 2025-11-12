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

#include "main.h"

extern bool autoLock;
extern bool WIFI_EN;
extern int presetTemp[2]; // preset baby skin temperature
extern double RawTemperatureLow[SENSOR_TEMP_QTY],
    RawTemperatureRange[SENSOR_TEMP_QTY];
extern double ReferenceTemperatureRange, ReferenceTemperatureLow;
extern double fineTuneSkinTemperature, fineTuneAirTemperature;

extern in3ator_parameters in3;

void resetFlash() {
  for (int i = false; i < EEPROM_SIZE; i++) {
    EEPROM.write(i, 0);
    EEPROM.commit();
  }
}

void initEEPROM() {
  if (!EEPROM.begin(EEPROM_SIZE)) {
    logE("failed to initialise EEPROM");
  }
  // if (!EEPROM.read(EEPROM_PANIC_OTA_CHANGE)) {
  //   EEPROM.write(EEPROM_PANIC_OTA_CHANGE, true);
  //   EEPROM.writeFloat(EEPROM_RAW_SKIN_TEMP_RANGE_CORRECTION, 20.23);
  //   EEPROM.commit();
  // }
  //   EEPROM.write(EEPROM_CHECK_STATUS, 0);
  //   EEPROM.commit();
  //   vTaskDelay(30);
  // }
  // else
  // {
  //   EEPROM.write(EEPROM_CHECK_STATUS, 1);
  //   EEPROM.commit();
  //   vTaskDelay(30);
  // }
  if (EEPROM.read(EEPROM_FIRST_TURN_ON)) { // firstTimePowerOn
    resetFlash();
    loaddefaultValues();
    logI("[FLASH] -> First turn on, loading default values");
  } else {
    logI("[FLASH] -> Loading variables stored in flash");
    recapVariables();
  }
  logI("[FLASH] -> Variables loaded");
}

void loaddefaultValues() {
  autoLock = DEFAULT_AUTOLOCK;
  WIFI_EN = DEFAULT_WIFI_EN;
  in3.language = defaultLanguage;
  in3.controlMode = CONTROL_AIR;
  in3.desiredControlTemperature = presetTemp[in3.controlMode];
  in3.desiredControlHumidity = presetHumidity;
  EEPROM.write(EEPROM_AUTO_LOCK, autoLock);
  EEPROM.write(EEPROM_WIFI_EN, WIFI_EN);
  EEPROM.write(EEPROM_LANGUAGE, in3.language);
  EEPROM.write(EEPROM_CONTROL_MODE, in3.controlMode);
  EEPROM.writeFloat(EEPROM_DESIRED_CONTROL_TEMPERATURE,
                    in3.desiredControlTemperature);
  EEPROM.commit();
}

void resetCalibration() {
  RawTemperatureLow[SKIN_SENSOR] = false;
  RawTemperatureRange[SKIN_SENSOR] = false;
  ReferenceTemperatureRange = false;
  ReferenceTemperatureLow = false;
  fineTuneSkinTemperature = false;
  fineTuneAirTemperature = false;
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
  fineTuneSkinTemperature = EEPROM.readFloat(EEPROM_FINE_TUNE_TEMP_SKIN);
  fineTuneAirTemperature = EEPROM.readFloat(EEPROM_FINE_TUNE_TEMP_AIR);
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
  WIFI_EN = EEPROM.read(EEPROM_WIFI_EN);
  in3.controlMode = EEPROM.read(EEPROM_CONTROL_MODE);
  in3.desiredControlTemperature =
      EEPROM.readFloat(EEPROM_DESIRED_CONTROL_TEMPERATURE);
  in3.desiredControlHumidity = EEPROM.read(EEPROM_DESIRED_CONTROL_HUMIDITY);

  if (in3.restoreState) {
    in3.actuation = EEPROM.read(EEPROM_CONTROL_ACTIVE);
    in3.phototherapy = EEPROM.read(EEPROM_PHOTOTHERAPY_ACTIVE);
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
      in3.restoreState = false;
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
  EEPROM.writeFloat(EEPROM_FINE_TUNE_TEMP_SKIN, fineTuneSkinTemperature);
  EEPROM.writeFloat(EEPROM_FINE_TUNE_TEMP_AIR, fineTuneAirTemperature);
  EEPROM.commit();
}
