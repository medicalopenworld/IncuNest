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
#include "EEPROM_defines.h"
#include "main.h"

extern char wifi_ssid[64];
extern char wifi_pass[64];
extern int photoTimerMinutes;

void resetFlash() {
  Preferences p;
  const char* ns[] = { HMI_NS_CFG, HMI_NS_WIFI, HMI_NS_GPRS };
  for (auto n : ns) { p.begin(n, false); p.clear(); p.end(); }
}

void loaddefaultValues() {
  Preferences p;
  p.begin(HMI_NS_CFG, false);
  p.putUChar (HMI_KEY_LANG,      LANG_EN);
  p.putFloat (HMI_KEY_AIR_TEMP,  DEFAULT_AIR_TEMP);
  p.putFloat (HMI_KEY_SKIN_TEMP, DEFAULT_SKIN_TEMP);
  p.putUChar (HMI_KEY_HUMIDITY,  DEFAULT_HUMIDITY);
  p.putUChar (HMI_KEY_PHOTO_MIN, PHOTO_TIMER_EEPROM_DEFAULT);
  p.putUChar (HMI_KEY_DARK_MODE, 0);
  p.putUChar (HMI_KEY_HUM_EN,    0);
  p.putUChar (HMI_KEY_SKIN_EN,   0);
  p.end();
}

static bool migrateFromEEPROM() {
  constexpr int OLD_FIRST_TURN_ON = 10;
  constexpr int OLD_LANG          = 30;
  constexpr int OLD_SERIAL        = 40;
  constexpr int OLD_AIR_TEMP      = 80;
  constexpr int OLD_SKIN_TEMP     = 85;
  constexpr int OLD_HUMIDITY      = 90;
  constexpr int OLD_PHOTO_MIN     = 66;
  constexpr int OLD_DARK_MODE     = 252;
  constexpr int OLD_HUM_EN        = 257;
  constexpr int OLD_WIFI_SSID     = 115;
  constexpr int OLD_WIFI_PASS     = 145;
  constexpr int OLD_TB_PROV       = 200;
  constexpr int OLD_TB_TOKEN      = 205;
  constexpr int OLD_VOLUME        = 251;
  constexpr int OLD_DISP_FREQ     = 253;

  Preferences old;
  old.begin("eeprom", true);
  uint8_t buf[263] = {};
  size_t len = old.getBytes("data", buf, sizeof(buf));
  old.end();

  if (len < 263 || buf[OLD_FIRST_TURN_ON] != 0) {
    return false;
  }

  auto rf   = [&](int off) { float    v; memcpy(&v, buf + off, 4); return v; };
  auto ru   = [&](int off) { uint32_t v; memcpy(&v, buf + off, 4); return v; };

  char ssid_tmp[31]  = {};
  char pass_tmp[26]  = {};
  char token_tmp[22] = {};
  memcpy(ssid_tmp,  buf + OLD_WIFI_SSID, 30);
  memcpy(pass_tmp,  buf + OLD_WIFI_PASS, 25);
  memcpy(token_tmp, buf + OLD_TB_TOKEN,  21);

  { Preferences p; p.begin(HMI_NS_CFG, false);
    p.putUChar (HMI_KEY_LANG,      buf[OLD_LANG]);
    p.putInt   (HMI_KEY_SERIAL,    *((int32_t*)(buf + OLD_SERIAL)));
    p.putFloat (HMI_KEY_AIR_TEMP,  rf(OLD_AIR_TEMP));
    p.putFloat (HMI_KEY_SKIN_TEMP, rf(OLD_SKIN_TEMP));
    p.putUChar (HMI_KEY_HUMIDITY,  buf[OLD_HUMIDITY]);
    p.putUChar (HMI_KEY_PHOTO_MIN, buf[OLD_PHOTO_MIN]);
    p.putUChar (HMI_KEY_DARK_MODE, buf[OLD_DARK_MODE]);
    p.putUChar (HMI_KEY_HUM_EN,    buf[OLD_HUM_EN]);
    p.putUChar (HMI_KEY_VOLUME,    buf[OLD_VOLUME]);
    p.putUInt  (HMI_KEY_DISP_FREQ, ru(OLD_DISP_FREQ));
    p.end(); }

  { Preferences p; p.begin(HMI_NS_WIFI, false);
    p.putString(HMI_KEY_SSID,     ssid_tmp);
    p.putString(HMI_KEY_PASSWORD, pass_tmp);
    p.end(); }

  { Preferences p; p.begin(HMI_NS_GPRS, false);
    p.putUChar (HMI_KEY_PROVISIONED, buf[OLD_TB_PROV]);
    p.putString(HMI_KEY_TOKEN,       token_tmp);
    p.end(); }

  old.begin("eeprom", false);
  old.clear();
  old.end();

  ESP_LOGI("HMI", "Migration EEPROM -> Preferences complete");
  return true;
}

void initEEPROM() {
  Preferences p;
  p.begin(HMI_NS_CFG, true);
  bool initialized = p.isKey(HMI_KEY_LANG);
  p.end();

  if (!initialized) {
    if (!migrateFromEEPROM()) {
      resetFlash();
      loaddefaultValues();
    }
  }
  recapVariables();
}

void recapVariables() {
  { Preferences p; p.begin(HMI_NS_CFG, true);
    g_lang            = (ui_lang_t)p.getUChar(HMI_KEY_LANG, LANG_EN);
    if (g_lang >= LANG_FR + 1) g_lang = LANG_EN;
    airTempValue      = p.getFloat (HMI_KEY_AIR_TEMP,  DEFAULT_AIR_TEMP);
    if (isnan(airTempValue) || airTempValue < AIR_TEMP_MIN || airTempValue > AIR_TEMP_MAX)
      airTempValue = DEFAULT_AIR_TEMP;
    skinTempValue     = p.getFloat (HMI_KEY_SKIN_TEMP, DEFAULT_SKIN_TEMP);
    if (isnan(skinTempValue) || skinTempValue < SKIN_TEMP_MIN || skinTempValue > SKIN_TEMP_MAX)
      skinTempValue = DEFAULT_SKIN_TEMP;
    humValue          = p.getUChar (HMI_KEY_HUMIDITY,  DEFAULT_HUMIDITY);
    photoTimerMinutes = p.getUChar (HMI_KEY_PHOTO_MIN, PHOTO_TIMER_EEPROM_DEFAULT);
    darkMode          = p.getUChar (HMI_KEY_DARK_MODE, 0) != 0;
    humidityEnabled   = p.getUChar (HMI_KEY_HUM_EN,    0) != 0;
    skinPanelEnabled  = p.getUChar (HMI_KEY_SKIN_EN,   0) != 0;
    in3.serialNumber  = p.getInt   (HMI_KEY_SERIAL,    0);
    p.end(); }

  { Preferences p; p.begin(HMI_NS_WIFI, true);
    String s  = p.getString(HMI_KEY_SSID,     "");
    String pw = p.getString(HMI_KEY_PASSWORD, "");
    strncpy(wifi_ssid, s.c_str(),  sizeof(wifi_ssid) - 1);
    wifi_ssid[sizeof(wifi_ssid) - 1] = '\0';
    strncpy(wifi_pass, pw.c_str(), sizeof(wifi_pass) - 1);
    wifi_pass[sizeof(wifi_pass) - 1] = '\0';
    p.end(); }

  // Validation
  if (humValue < HUM_MIN || humValue > HUM_MAX)
    humValue = DEFAULT_HUMIDITY;
  if (photoTimerMinutes < PHOTO_TIMER_MIN_MINUTES || photoTimerMinutes > PHOTO_TIMER_MAX_MINUTES)
    photoTimerMinutes = PHOTO_TIMER_EEPROM_DEFAULT;

  ESP_LOGI("HMI", "Language loaded: %d", g_lang);
  ESP_LOGI("HMI", "Serial Number loaded: %d", in3.serialNumber);
  ESP_LOGI("HMI", "Air Temp loaded: %.2f", airTempValue);
  ESP_LOGI("HMI", "Skin Temp loaded: %.2f", skinTempValue);
  ESP_LOGI("HMI", "Humidity loaded: %d", humValue);
  ESP_LOGI("HMI", "WiFi SSID loaded: %s", wifi_ssid);
}
