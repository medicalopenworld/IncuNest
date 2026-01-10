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

#include "EEPROM.h"
#include "esp_log.h"
#include "main.h"

static const char *TAG = "EEPROM";

void loaddefaultValues();
void recapVariables();
void resetCalibration();

void resetFlash() {
  for (int i = false; i < EEPROM_SIZE; i++) {
    EEPROM.write(i, 0);
    EEPROM.commit();
  }
}

void initEEPROM() {
  if (!EEPROM.begin(EEPROM_SIZE)) {
    ESP_LOGE(TAG, "failed to initialise EEPROM");
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
    ESP_LOGI(TAG, "[FLASH] -> First turn on, loading default values");
  } else {
    ESP_LOGI(TAG, "[FLASH] -> Loading variables stored in flash");
    recapVariables();
  }
  ESP_LOGI(TAG, "[FLASH] -> Variables loaded");
}

void loaddefaultValues() {
  // autoLock = DEFAULT_AUTOLOCK;
  // WIFI_EN = DEFAULT_WIFI_EN;
  // in3.language = defaultLanguage;
  // in3.controlMode = CONTROL_AIR;
  // in3.desiredControlTemperature = presetTemp[in3.controlMode];
  // in3.desiredControlHumidity = presetHumidity;
  // EEPROM.write(EEPROM_AUTO_LOCK, autoLock);
  // EEPROM.write(EEPROM_WIFI_EN, WIFI_EN);
  // EEPROM.write(EEPROM_LANGUAGE, in3.language);
  // EEPROM.write(EEPROM_CONTROL_MODE, in3.controlMode);
  // EEPROM.writeFloat(EEPROM_DESIRED_CONTROL_TEMPERATURE,
  //                   in3.desiredControlTemperature);
  // EEPROM.commit();
}

void recapVariables() {
  // autoLock = EEPROM.read(EEPROM_AUTO_LOCK);
  // in3.language = EEPROM.read(EEPROM_LANGUAGE);
  // in3.serialNumber = EEPROM.readInt(EEPROM_SERIAL_NUMBER);
  // WIFI_EN = EEPROM.read(EEPROM_WIFI_EN);
}
