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
extern double RawTemperatureLow[SENSOR_TEMP_QTY];
extern double RawTemperatureRange[SENSOR_TEMP_QTY];
extern double ReferenceTemperatureRange, ReferenceTemperatureLow;
extern IncuNest_parameters in3;
extern char wifi_ssid[64];
extern char wifi_pass[64];
extern int g_restore_photo_minutes;

void resetFlash() {
  Preferences p;
  const char* ns[] = { NS_CFG, NS_CAL, NS_WIFI, NS_GPRS, NS_RT, NS_STATE };
  for (auto n : ns) {
    p.begin(n, false);
    p.clear();
    p.end();
  }
}

void loaddefaultValues() {
  autoLock                      = DEFAULT_AUTOLOCK;
  in3.language                  = defaultLanguage;
  in3.controlMode               = CONTROL_AIR;
  in3.desiredControlTemperature = presetTemp[CONTROL_AIR];
  in3.desiredControlHumidity    = presetHumidity;
  in3.fanCtlPWM                 = FAN_CTL_PWM_DEFAULT;
  ReferenceTemperatureRange     = 0.0;
  ReferenceTemperatureLow       = 0.0;
  for (int i = 0; i < SENSOR_TEMP_QTY; i++) {
    RawTemperatureLow[i]   = 0.0;
    RawTemperatureRange[i] = 0.0;
  }

  { Preferences p; p.begin(NS_CFG, false);
    p.putUChar(KEY_LANG,        in3.language);
    p.putUChar(KEY_AUTOLOCK,    autoLock);
    p.putUChar(KEY_CTRL_MODE,   in3.controlMode);
    p.putFloat(KEY_CTRL_TEMP,   in3.desiredControlTemperature);
    p.putUChar(KEY_CTRL_HUM,    in3.desiredControlHumidity);
    p.putInt  (KEY_FAN_CTL_PWM, in3.fanCtlPWM);
    p.end(); }

  { Preferences p; p.begin(NS_CAL, false);
    p.putFloat(KEY_CAL_SK_LOW,  0.0f);
    p.putFloat(KEY_CAL_SK_RNG,  0.0f);
    p.putFloat(KEY_CAL_REF_RNG, 0.0f);
    p.putFloat(KEY_CAL_REF_LOW, 0.0f);
    p.putFloat(KEY_FT_SKIN,     0.0f);
    p.putFloat(KEY_FT_AIR,      0.0f);
    p.end(); }

  { Preferences p; p.begin(NS_WIFI, false);
    p.putString(KEY_SSID,     "");
    p.putString(KEY_PASSWORD, "");
    p.end(); }
}

void resetCalibration() {
  RawTemperatureLow[SKIN_SENSOR]   = false;
  RawTemperatureRange[SKIN_SENSOR] = false;
  ReferenceTemperatureRange        = false;
  ReferenceTemperatureLow          = false;
  in3.fineTuneSkinTemperature      = false;
  in3.fineTuneAirTemperature       = false;
}

static bool migrateFromEEPROM() {
  constexpr int OLD_MAGIC_OFFSET  = 10;
  constexpr uint8_t OLD_MAGIC_VAL = 0xAB;
  constexpr int OLD_AUTOLOCK      = 20;
  constexpr int OLD_LANG          = 30;
  constexpr int OLD_SERIAL        = 40;
  constexpr int OLD_CTRL_ACTIVE   = 60;
  constexpr int OLD_PHOTO_ACTIVE  = 65;
  constexpr int OLD_CTRL_MODE     = 70;
  constexpr int OLD_CTRL_TEMP     = 80;
  constexpr int OLD_CTRL_HUM      = 90;
  constexpr int OLD_SK_LOW        = 100;
  constexpr int OLD_SK_RNG        = 110;
  constexpr int OLD_WIFI_SSID     = 115;
  constexpr int OLD_WIFI_PASS     = 145;
  constexpr int OLD_REF_RNG       = 170;
  constexpr int OLD_REF_LOW       = 180;
  constexpr int OLD_FT_SKIN       = 190;
  constexpr int OLD_FT_AIR        = 194;
  constexpr int OLD_TB_PROV       = 200;
  constexpr int OLD_TB_TOKEN      = 205;
  constexpr int OLD_STANDBY       = 226;
  constexpr int OLD_CTRL_TIME     = 230;
  constexpr int OLD_HEATER_TIME   = 234;
  constexpr int OLD_FAN_TIME      = 238;
  constexpr int OLD_PHOTO_TIME    = 242;
  constexpr int OLD_HUM_TIME      = 246;
  constexpr int OLD_PANIC_OTA     = 250;
  constexpr int OLD_FAN_PWM       = 254;
  constexpr int OLD_HEAT_MAX_A    = 258;
  constexpr int OLD_SKIN_T_MAX    = 262;
  constexpr int OLD_AIR_T_MAX     = 266;
  constexpr int OLD_ACT_PERIOD    = 270;
  constexpr int OLD_PHOTO_PERIOD  = 274;
  constexpr int OLD_STBY_PERIOD   = 278;
  constexpr int OLD_FAN_CTL_PWM   = 282;

  Preferences old;
  old.begin("eeprom", true);
  uint8_t buf[512] = {};
  size_t len = old.getBytes("data", buf, sizeof(buf));
  old.end();

  if (len < 512 || buf[OLD_MAGIC_OFFSET] != OLD_MAGIC_VAL) {
    return false;
  }

  auto rf = [&](int off) { float v;   memcpy(&v, buf + off, 4); return v; };
  auto ri = [&](int off) { int32_t v; memcpy(&v, buf + off, 4); return v; };

  { Preferences p; p.begin(NS_CFG, false);
    p.putUChar (KEY_AUTOLOCK,     buf[OLD_AUTOLOCK]);
    p.putUChar (KEY_LANG,         buf[OLD_LANG]);
    p.putInt   (KEY_SERIAL,       ri(OLD_SERIAL));
    p.putUChar (KEY_CTRL_MODE,    buf[OLD_CTRL_MODE]);
    p.putFloat (KEY_CTRL_TEMP,    rf(OLD_CTRL_TEMP));
    p.putUChar (KEY_CTRL_HUM,     buf[OLD_CTRL_HUM]);
    p.putUChar (KEY_PANIC_OTA,    buf[OLD_PANIC_OTA]);
    p.putInt   (KEY_FAN_PWM,      ri(OLD_FAN_PWM));
    p.putFloat (KEY_HEAT_MAX_A,   rf(OLD_HEAT_MAX_A));
    p.putFloat (KEY_SKIN_T_MAX,   rf(OLD_SKIN_T_MAX));
    p.putFloat (KEY_AIR_T_MAX,    rf(OLD_AIR_T_MAX));
    p.putInt   (KEY_ACT_PERIOD,   ri(OLD_ACT_PERIOD));
    p.putInt   (KEY_PHOTO_PERIOD, ri(OLD_PHOTO_PERIOD));
    p.putInt   (KEY_STBY_PERIOD,  ri(OLD_STBY_PERIOD));
    p.putInt   (KEY_FAN_CTL_PWM,  ri(OLD_FAN_CTL_PWM));
    p.end(); }

  { Preferences p; p.begin(NS_CAL, false);
    p.putFloat(KEY_CAL_SK_LOW,  rf(OLD_SK_LOW));
    p.putFloat(KEY_CAL_SK_RNG,  rf(OLD_SK_RNG));
    p.putFloat(KEY_CAL_REF_RNG, rf(OLD_REF_RNG));
    p.putFloat(KEY_CAL_REF_LOW, rf(OLD_REF_LOW));
    p.putFloat(KEY_FT_SKIN,     rf(OLD_FT_SKIN));
    p.putFloat(KEY_FT_AIR,      rf(OLD_FT_AIR));
    p.end(); }

  { Preferences p; p.begin(NS_WIFI, false);
    p.putString(KEY_SSID,     String((char*)(buf + OLD_WIFI_SSID)));
    p.putString(KEY_PASSWORD, String((char*)(buf + OLD_WIFI_PASS)));
    p.end(); }

  { Preferences p; p.begin(NS_GPRS, false);
    p.putUChar (KEY_PROVISIONED, buf[OLD_TB_PROV]);
    p.putString(KEY_TOKEN,       String((char*)(buf + OLD_TB_TOKEN)));
    p.end(); }

  { Preferences p; p.begin(NS_RT, false);
    p.putFloat(KEY_RT_STANDBY, rf(OLD_STANDBY));
    p.putFloat(KEY_RT_CTRL,    rf(OLD_CTRL_TIME));
    p.putFloat(KEY_RT_HEATER,  rf(OLD_HEATER_TIME));
    p.putFloat(KEY_RT_FAN,     rf(OLD_FAN_TIME));
    p.putFloat(KEY_RT_PHOTO,   rf(OLD_PHOTO_TIME));
    p.putFloat(KEY_RT_HUM,     rf(OLD_HUM_TIME));
    p.end(); }

  { Preferences p; p.begin(NS_STATE, false);
    p.putUChar(KEY_CTRL_ACTIVE,  buf[OLD_CTRL_ACTIVE]);
    p.putUChar(KEY_PHOTO_ACTIVE, buf[OLD_PHOTO_ACTIVE]);
    p.end(); }

  old.begin("eeprom", false);
  old.clear();
  old.end();

  ESP_LOGI("APP", "Migración EEPROM → Preferences completada");
  return true;
}

void initEEPROM() {
  Preferences p;
  p.begin(NS_CFG, true);
  bool initialized = p.isKey(KEY_LANG);
  p.end();

  if (!initialized) {
    if (!migrateFromEEPROM()) {
      ESP_LOGI("APP", "Primer arranque — cargando valores por defecto");
      resetFlash();
      loaddefaultValues();
    }
  } else {
    ESP_LOGI("APP", "Cargando variables desde Preferences");
  }
  recapVariables();
}

void recapVariables() {
  { Preferences p; p.begin(NS_CFG, true);
    autoLock                      = p.getUChar(KEY_AUTOLOCK,   DEFAULT_AUTOLOCK);
    in3.language                  = p.getUChar(KEY_LANG,       defaultLanguage);
    in3.serialNumber              = p.getInt  (KEY_SERIAL,     0);
    in3.controlMode               = p.getUChar(KEY_CTRL_MODE,  CONTROL_AIR);
    in3.desiredControlTemperature = p.getFloat(KEY_CTRL_TEMP,  presetTemp[CONTROL_AIR]);
    in3.desiredControlHumidity    = p.getUChar(KEY_CTRL_HUM,   presetHumidity);
    in3.fanPWM                    = p.getInt  (KEY_FAN_PWM,    0);
    if (in3.fanPWM <= 0 || in3.fanPWM > 255) in3.fanPWM = 0;
    in3.heaterMaxPowerAmps        = p.getFloat(KEY_HEAT_MAX_A, 0.0f);
    if (isnan(in3.heaterMaxPowerAmps) || in3.heaterMaxPowerAmps <= 0) in3.heaterMaxPowerAmps = 0.0f;
    in3.skinTemperatureSetMax     = p.getFloat(KEY_SKIN_T_MAX, 0.0f);
    if (isnan(in3.skinTemperatureSetMax) || in3.skinTemperatureSetMax <= 0) in3.skinTemperatureSetMax = 0.0f;
    in3.airTemperatureSetMax      = p.getFloat(KEY_AIR_T_MAX,  0.0f);
    if (isnan(in3.airTemperatureSetMax) || in3.airTemperatureSetMax <= 0) in3.airTemperatureSetMax = 0.0f;
    in3.actuating_gprs_period     = p.getInt(KEY_ACT_PERIOD,   0);
    in3.phototherapy_gprs_period  = p.getInt(KEY_PHOTO_PERIOD, 0);
    in3.standby_gprs_period       = p.getInt(KEY_STBY_PERIOD,  0);
    in3.fanCtlPWM                 = p.getInt(KEY_FAN_CTL_PWM,  FAN_CTL_PWM_DEFAULT);
    if (in3.fanCtlPWM <= 0 || in3.fanCtlPWM > 255) in3.fanCtlPWM = FAN_CTL_PWM_DEFAULT;
    p.end(); }

  { Preferences p; p.begin(NS_CAL, true);
    RawTemperatureLow[SKIN_SENSOR]   = p.getFloat(KEY_CAL_SK_LOW,  0.0f);
    RawTemperatureRange[SKIN_SENSOR] = p.getFloat(KEY_CAL_SK_RNG,  0.0f);
    ReferenceTemperatureRange        = p.getFloat(KEY_CAL_REF_RNG, 0.0f);
    ReferenceTemperatureLow          = p.getFloat(KEY_CAL_REF_LOW, 0.0f);
    in3.fineTuneSkinTemperature      = p.getFloat(KEY_FT_SKIN,     0.0f);
    if (isnan(in3.fineTuneSkinTemperature)) in3.fineTuneSkinTemperature = 0.0f;
    in3.fineTuneAirTemperature       = p.getFloat(KEY_FT_AIR,      0.0f);
    if (isnan(in3.fineTuneAirTemperature)) in3.fineTuneAirTemperature = 0.0f;
    p.end(); }

  if (!ReferenceTemperatureRange) {
    in3.calibrationError = true;
    logE("[HW] -> Fail -> temperature sensor is not calibrated");
  }

  { Preferences p; p.begin(NS_RT, true);
    in3.standby_time             = p.getFloat(KEY_RT_STANDBY, 0.0f);
    in3.control_active_time      = p.getFloat(KEY_RT_CTRL,    0.0f);
    in3.heater_active_time       = p.getFloat(KEY_RT_HEATER,  0.0f);
    in3.fan_active_time          = p.getFloat(KEY_RT_FAN,     0.0f);
    in3.phototherapy_active_time = p.getFloat(KEY_RT_PHOTO,   0.0f);
    in3.humidifier_active_time   = p.getFloat(KEY_RT_HUM,     0.0f);
    p.end(); }

  { Preferences p; p.begin(NS_WIFI, true);
    String s  = p.getString(KEY_SSID,     "");
    String pw = p.getString(KEY_PASSWORD, "");
    strncpy(wifi_ssid, s.c_str(),  sizeof(wifi_ssid) - 1);
    strncpy(wifi_pass, pw.c_str(), sizeof(wifi_pass) - 1);
    p.end(); }

  { Preferences p; p.begin(NS_STATE, true);
    in3.restoreState = true;
    in3.actuation    = p.getUChar(KEY_ACTUATION,    0);
    in3.phototherapy = p.getUChar(KEY_PHOTO_ACTIVE, 0);
    p.end(); }

  // Restore phototherapy timer if it was active
  if (in3.phototherapy) {
    Preferences p;
    p.begin("photo", true);
    bool was_active = p.getBool("active", false);
    int  saved_mins = p.getInt ("mins",   0);
    p.end();
    if (was_active && saved_mins > 0) {
      g_restore_photo_minutes = saved_mins;
      logI("[HW] -> restoreState: phototherapy timer restored (" + String(saved_mins) + " min remaining)");
    }
  }

  // Process actuation mode (restore temperature/humidity control state)
  if (in3.restoreState) {
    switch (in3.actuation) {
      case ACTUATION_TEMPERATURE:
        in3.temperatureControl = true;
        in3.humidityControl    = false;
        break;
      case ACTUATION_HUMIDITY:
        in3.temperatureControl = false;
        in3.humidityControl    = true;
        break;
      case ACTUATION_TEMP_AND_HUMIDITY:
        in3.temperatureControl = true;
        in3.humidityControl    = true;
        break;
      default:
        in3.temperatureControl = false;
        in3.humidityControl    = false;
        break;
    }
  } else {
    Preferences p;
    p.begin(NS_STATE, false);
    p.putUChar(KEY_CTRL_ACTIVE, 0);
    p.end();
  }

  ESP_LOGI("APP", "Serial: %d, Lang: %d", in3.serialNumber, in3.language);
}

void saveCalibrationToEEPROM() {
  Preferences p;
  p.begin(NS_CAL, false);
  p.putFloat(KEY_CAL_SK_LOW,  RawTemperatureLow[SKIN_SENSOR]);
  p.putFloat(KEY_CAL_SK_RNG,  RawTemperatureRange[SKIN_SENSOR]);
  p.putFloat(KEY_CAL_REF_RNG, ReferenceTemperatureRange);
  p.putFloat(KEY_CAL_REF_LOW, ReferenceTemperatureLow);
  p.putFloat(KEY_FT_SKIN,     in3.fineTuneSkinTemperature);
  p.putFloat(KEY_FT_AIR,      in3.fineTuneAirTemperature);
  p.end();
}
