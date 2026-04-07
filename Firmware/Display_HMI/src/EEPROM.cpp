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
#include <cmath>

#include "EEPROM.h"
#include "esp_log.h"
#include "main.h"

static const char *TAG = "EEPROM";

void loaddefaultValues();
void recapVariables();
void resetCalibration();

extern int photoTimerMinutes;

void resetFlash() {
  for (int i = false; i < EEPROM_SIZE; i++) {
    EEPROM.write(i, 0);
    EEPROM.commit();
  }
}

void initEEPROM() {
  if (!EEPROM.begin(EEPROM_SIZE)) {
    ESP_LOGE(TAG, "Failed to initialise EEPROM");
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
  EEPROM.write(EEPROM_LANGUAGE, LANG_EN);
  EEPROM.writeFloat(EEPROM_DESIRED_AIR_TEMP, 30.0);
  EEPROM.writeFloat(EEPROM_DESIRED_SKIN_TEMP, 37);
  EEPROM.write(EEPROM_DESIRED_HUMIDITY, 50);
  EEPROM.write(EEPROM_PHOTO_TIMER_MINUTES, 240);
  EEPROM.write(EEPROM_DARK_MODE, 0); // Default off
  EEPROM.write(EEPROM_HUMIDITY_ENABLED, 0); // Default off
  EEPROM.commit();
}

extern char wifi_ssid[64];
extern char wifi_pass[64];

void recapVariables() {
  g_lang = (ui_lang_t)EEPROM.read(EEPROM_LANGUAGE);
  // Validation
  if (g_lang > 2 || g_lang < 0) {
    g_lang = LANG_EN;
  }

  airTempValue = EEPROM.readFloat(EEPROM_DESIRED_AIR_TEMP);
  skinTempValue = EEPROM.readFloat(EEPROM_DESIRED_SKIN_TEMP);
  humValue = EEPROM.read(EEPROM_DESIRED_HUMIDITY);
  photoTimerMinutes = EEPROM.read(EEPROM_PHOTO_TIMER_MINUTES);
  darkMode = EEPROM.read(EEPROM_DARK_MODE) == 1;
  humidityEnabled = EEPROM.read(EEPROM_HUMIDITY_ENABLED) == 1;

  String ssid = EEPROM.readString(EEPROM_WIFI_SSID);
  String pass = EEPROM.readString(EEPROM_WIFI_PASSWORD);
  strncpy(wifi_ssid, ssid.c_str(), sizeof(wifi_ssid));
  strncpy(wifi_pass, pass.c_str(), sizeof(wifi_pass));

  // Validation
  if (isnan(airTempValue) || airTempValue < AIR_TEMP_MIN || airTempValue > AIR_TEMP_MAX)
    airTempValue = 30.0;
  if (isnan(skinTempValue) || skinTempValue < SKIN_TEMP_MIN || skinTempValue > SKIN_TEMP_MAX)
    skinTempValue = 37.0;
  if (humValue < HUM_MIN || humValue > HUM_MAX)
    humValue = 50;

  if (photoTimerMinutes < 120 || photoTimerMinutes > 600)
    photoTimerMinutes = 240;

  in3.serialNumber = EEPROM.readInt(EEPROM_SERIAL_NUMBER);

  ESP_LOGI(TAG, "Language loaded: %d", g_lang);
  ESP_LOGI(TAG, "Serial Number loaded: %d", in3.serialNumber);
  ESP_LOGI(TAG, "Air Temp loaded: %.2f", airTempValue);
  ESP_LOGI(TAG, "Skin Temp loaded: %.2f", skinTempValue);
  ESP_LOGI(TAG, "Humidity loaded: %d", humValue);
  ESP_LOGI(TAG, "WiFi SSID loaded: %s", wifi_ssid);
}
