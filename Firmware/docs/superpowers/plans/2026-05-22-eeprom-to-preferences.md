# EEPROM → Preferences Migration Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the Arduino `EEPROM.h` library with `Preferences.h` in both `motherBoard` and `Display_HMI`, eliminating byte-offset fragility and unlocking type-safe, named key access.

**Architecture:** Each project gets its own set of short NVS namespaces (≤15 chars). All reads stay in `EEPROM.cpp` inside `recapVariables()`. Writes in other files replace `EEPROM.writeXxx(OFFSET, val) + EEPROM.commit()` with a local `Preferences` open/put/end block. No magic-byte sentinel is needed — `getXxx(key, defaultValue)` returns the default when the key is absent.

**Tech Stack:** Arduino ESP32 `Preferences.h`, ESP-IDF NVS under the hood.

**Migration strategy:** On first boot after update, `initEEPROM()` detects that the new Preferences namespaces are empty and calls `migrateFromEEPROM()`. This reads the raw 512-byte (or 263-byte) blob from the old `"eeprom"` NVS namespace (where `EEPROM.h` stored it), parses values using the old byte offsets (kept as local constants inside the migration function), writes them into the new namespaces, and finally clears the `"eeprom"` namespace. If old data is absent (truly fresh device), `loaddefaultValues()` runs instead. Transparent to the user — settings, calibration, and serial number survive the update.

---

## Namespace & Key Design

### motherBoard namespaces

| Namespace | Contents |
|-----------|----------|
| `"mb_cfg"` | lang, autolock, serial, ctrl_mode, ctrl_temp, ctrl_hum, fan_pwm, heat_max_A, skin_t_max, air_t_max, heater_test, panic_ota, fan_ctl_pwm |
| `"mb_cal"` | cal_sk_low, cal_sk_rng, cal_ref_rng, cal_ref_low, ft_skin, ft_air |
| `"mb_wifi"` | ssid, password |
| `"mb_gprs"` | provisioned, token, act_period, photo_period, stby_period |
| `"mb_rt"` | standby, ctrl_time, heater_t, fan_t, photo_t, hum_t |
| `"mb_state"` | ctrl_active, photo_active, ctrl_actuation |

### Display_HMI namespaces

| Namespace | Contents |
|-----------|----------|
| `"hmi_cfg"` | lang, serial, ctrl_mode, air_temp, skin_temp, humidity, photo_min, dark_mode, hum_en, volume, disp_freq, aa_weight, aa_gest, aa_age_h |
| `"hmi_wifi"` | ssid, password |
| `"hmi_gprs"` | provisioned, token |

---

## File Map

### motherBoard — files to change

| File | Change |
|------|--------|
| `include/main.h` | Remove all `EEPROM_*` offset `#define`s; add namespace + key `constexpr char[]` constants; remove `EEPROM_SIZE` |
| `src/EEPROM.cpp` | Full rewrite: drop `EEPROM.h`, use `Preferences`; remove magic-byte check; `resetFlash()` → clear each namespace |
| `src/CommTask.cpp` | Replace 7 write+commit blocks |
| `src/GPRS.cpp` | Replace 4 write+commit blocks (2 locations × 2 writes) |
| `src/main.cpp` | Replace 3 write+commit blocks |
| `src/Wifi_OTA.cpp` | Replace ~12 write+commit blocks |
| `src/userInterface.cpp` | Replace 7 write+commit blocks |
| `src/initHardware.cpp` | Replace 1 write+commit block |
| `src/UI_actuatorsProgress.cpp` | Replace 2 write+commit blocks |
| `src/updateData.cpp` | Replace 6 write+commit blocks |

### Display_HMI — files to change

| File | Change |
|------|--------|
| `include/EEPROM_defines.h` | Remove all `EEPROM_*` offset `#define`s; add namespace + key constants |
| `src/EEPROM.cpp` | Full rewrite |
| `src/AudioManager.cpp` | Replace 1 read (move read to `recapVariables` or use inline Preferences) |
| `src/UITask.cpp` | Replace ~14 write+commit blocks |
| `src/CommTask.cpp` | Replace 1 write+commit block |
| `src/Wifi_OTA.cpp` | Replace 3 write+commit blocks |

---

## Task 1 — motherBoard: Replace EEPROM constants in `main.h`

**Files:**
- Modify: `motherBoard/include/main.h` (lines 423–460)

- [ ] **Step 1: Remove EEPROM offset defines and add Preferences constants**

  Remove the block `#define EEPROM_CHECK_STATUS` through `#define EEPROM_FAN_CTL_PWM` and replace with:

  ```cpp
  // --------------- Preferences namespaces ---------------
  constexpr char NS_CFG[]   = "mb_cfg";
  constexpr char NS_CAL[]   = "mb_cal";
  constexpr char NS_WIFI[]  = "mb_wifi";
  constexpr char NS_GPRS[]  = "mb_gprs";
  constexpr char NS_RT[]    = "mb_rt";
  constexpr char NS_STATE[] = "mb_state";

  // --------------- Key names: mb_cfg ---------------
  constexpr char KEY_LANG[]        = "lang";
  constexpr char KEY_AUTOLOCK[]    = "autolock";
  constexpr char KEY_SERIAL[]      = "serial";
  constexpr char KEY_CTRL_MODE[]   = "ctrl_mode";
  constexpr char KEY_CTRL_TEMP[]   = "ctrl_temp";
  constexpr char KEY_CTRL_HUM[]    = "ctrl_hum";
  constexpr char KEY_FAN_PWM[]     = "fan_pwm";
  constexpr char KEY_HEAT_MAX_A[]  = "heat_max_A";
  constexpr char KEY_SKIN_T_MAX[]  = "skin_t_max";
  constexpr char KEY_AIR_T_MAX[]   = "air_t_max";
  constexpr char KEY_HEATER_TEST[] = "heater_test";
  constexpr char KEY_PANIC_OTA[]   = "panic_ota";
  constexpr char KEY_FAN_CTL_PWM[] = "fan_ctl_pwm";

  // --------------- Key names: mb_cal ---------------
  constexpr char KEY_CAL_SK_LOW[]  = "cal_sk_low";
  constexpr char KEY_CAL_SK_RNG[]  = "cal_sk_rng";
  constexpr char KEY_CAL_REF_RNG[] = "cal_ref_rng";
  constexpr char KEY_CAL_REF_LOW[] = "cal_ref_low";
  constexpr char KEY_FT_SKIN[]     = "ft_skin";
  constexpr char KEY_FT_AIR[]      = "ft_air";

  // --------------- Key names: mb_wifi ---------------
  constexpr char KEY_SSID[]     = "ssid";
  constexpr char KEY_PASSWORD[] = "password";

  // --------------- Key names: mb_gprs ---------------
  constexpr char KEY_PROVISIONED[]  = "provisioned";
  constexpr char KEY_TOKEN[]        = "token";
  constexpr char KEY_ACT_PERIOD[]   = "act_period";
  constexpr char KEY_PHOTO_PERIOD[] = "photo_period";
  constexpr char KEY_STBY_PERIOD[]  = "stby_period";

  // --------------- Key names: mb_rt ---------------
  constexpr char KEY_RT_STANDBY[]  = "standby";
  constexpr char KEY_RT_CTRL[]     = "ctrl_time";
  constexpr char KEY_RT_HEATER[]   = "heater_t";
  constexpr char KEY_RT_FAN[]      = "fan_t";
  constexpr char KEY_RT_PHOTO[]    = "photo_t";
  constexpr char KEY_RT_HUM[]      = "hum_t";

  // --------------- Key names: mb_state ---------------
  constexpr char KEY_CTRL_ACTIVE[]  = "ctrl_active";
  constexpr char KEY_PHOTO_ACTIVE[] = "photo_active";
  constexpr char KEY_ACTUATION[]    = "actuation";
  ```

- [ ] **Step 2: Remove the `#include <EEPROM.h>` from main.h (if present) and add Preferences include**

  At the top of `main.h`, replace or add:
  ```cpp
  #include <Preferences.h>
  ```

- [ ] **Step 3: Build the project to confirm no missing symbol errors yet (the .cpp files still use EEPROM — expected errors)**

  Run: `pio run -e IncuNest_V16 2>&1 | head -30`

---

## Task 2 — motherBoard: Rewrite `src/EEPROM.cpp`

**Files:**
- Modify: `motherBoard/src/EEPROM.cpp`

- [ ] **Step 1: Replace the header includes and remove EEPROM.h**

  ```cpp
  #include <Arduino.h>
  #include <Preferences.h>
  #include "main.h"

  extern bool autoLock;
  extern bool WIFI_EN;
  extern int presetTemp[2];
  extern double RawTemperatureLow[SENSOR_TEMP_QTY],
      RawTemperatureRange[SENSOR_TEMP_QTY];
  extern double ReferenceTemperatureRange, ReferenceTemperatureLow;
  extern in3ator_parameters in3;
  extern char wifi_ssid[64];
  extern char wifi_pass[64];
  ```

- [ ] **Step 2: Rewrite `resetFlash()`**

  ```cpp
  void resetFlash() {
    Preferences p;
    const char* ns[] = { NS_CFG, NS_CAL, NS_WIFI, NS_GPRS, NS_RT, NS_STATE };
    for (auto n : ns) {
      p.begin(n, false);
      p.clear();
      p.end();
    }
  }
  ```

- [ ] **Step 3: Rewrite `loaddefaultValues()`**

  ```cpp
  void loaddefaultValues() {
    autoLock             = DEFAULT_AUTOLOCK;
    in3.language         = defaultLanguage;
    in3.controlMode      = CONTROL_AIR;
    in3.desiredControlTemperature = presetTemp[CONTROL_AIR];
    in3.desiredControlHumidity    = presetHumidity;
    in3.fanCtlPWM        = FAN_CTL_PWM_DEFAULT;
    ReferenceTemperatureRange = 0.0;
    ReferenceTemperatureLow   = 0.0;
    for (int i = 0; i < SENSOR_TEMP_QTY; i++) {
      RawTemperatureLow[i]  = 0.0;
      RawTemperatureRange[i] = 0.0;
    }

    { Preferences p; p.begin(NS_CFG, false);
      p.putUChar(KEY_LANG,       in3.language);
      p.putUChar(KEY_AUTOLOCK,   autoLock);
      p.putUChar(KEY_CTRL_MODE,  in3.controlMode);
      p.putFloat(KEY_CTRL_TEMP,  in3.desiredControlTemperature);
      p.putUChar(KEY_CTRL_HUM,   in3.desiredControlHumidity);
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
  ```

- [ ] **Step 4: Add `migrateFromEEPROM()`**

  The old EEPROM.h library stored the entire blob in NVS under namespace `"eeprom"`, key `"data"`. This function reads that blob and writes each field into the new namespaces. All old offsets are kept as local constants inside the function so they can be removed from the header.

  ```cpp
  static bool migrateFromEEPROM() {
    // Old byte offsets — kept only for this one-time migration
    constexpr int OLD_MAGIC_OFFSET   = 10;
    constexpr uint8_t OLD_MAGIC_VAL  = 0xAB;
    constexpr int OLD_AUTOLOCK       = 20;
    constexpr int OLD_LANG           = 30;
    constexpr int OLD_SERIAL         = 40;
    constexpr int OLD_CTRL_ACTIVE    = 60;
    constexpr int OLD_PHOTO_ACTIVE   = 65;
    constexpr int OLD_CTRL_MODE      = 70;
    constexpr int OLD_CTRL_TEMP      = 80;
    constexpr int OLD_CTRL_HUM       = 90;
    constexpr int OLD_SK_LOW         = 100;
    constexpr int OLD_SK_RNG         = 110;
    constexpr int OLD_WIFI_SSID      = 115;
    constexpr int OLD_WIFI_PASS      = 145;
    constexpr int OLD_REF_RNG        = 170;
    constexpr int OLD_REF_LOW        = 180;
    constexpr int OLD_FT_SKIN        = 190;
    constexpr int OLD_FT_AIR         = 194;
    constexpr int OLD_TB_PROV        = 200;
    constexpr int OLD_TB_TOKEN       = 205;   // 21 bytes
    constexpr int OLD_STANDBY        = 226;
    constexpr int OLD_CTRL_TIME      = 230;
    constexpr int OLD_HEATER_TIME    = 234;
    constexpr int OLD_FAN_TIME       = 238;
    constexpr int OLD_PHOTO_TIME     = 242;
    constexpr int OLD_HUM_TIME       = 246;
    constexpr int OLD_PANIC_OTA      = 250;
    constexpr int OLD_FAN_PWM        = 254;
    constexpr int OLD_HEAT_MAX_A     = 258;
    constexpr int OLD_SKIN_T_MAX     = 262;
    constexpr int OLD_AIR_T_MAX      = 266;
    constexpr int OLD_ACT_PERIOD     = 270;
    constexpr int OLD_PHOTO_PERIOD   = 274;
    constexpr int OLD_STBY_PERIOD    = 278;
    constexpr int OLD_FAN_CTL_PWM    = 282;

    // Read old EEPROM blob
    Preferences old;
    old.begin("eeprom", true);
    uint8_t buf[512] = {};
    size_t len = old.getBytes("data", buf, sizeof(buf));
    old.end();

    if (len < 512 || buf[OLD_MAGIC_OFFSET] != OLD_MAGIC_VAL) {
      return false;  // no old data or not initialized
    }

    auto rf = [&](int off) { float v; memcpy(&v, buf + off, 4); return v; };
    auto ri = [&](int off) { int32_t v; memcpy(&v, buf + off, 4); return v; };

    { Preferences p; p.begin(NS_CFG, false);
      p.putUChar (KEY_AUTOLOCK,    buf[OLD_AUTOLOCK]);
      p.putUChar (KEY_LANG,        buf[OLD_LANG]);
      p.putInt   (KEY_SERIAL,      ri(OLD_SERIAL));
      p.putUChar (KEY_CTRL_MODE,   buf[OLD_CTRL_MODE]);
      p.putFloat (KEY_CTRL_TEMP,   rf(OLD_CTRL_TEMP));
      p.putUChar (KEY_CTRL_HUM,    buf[OLD_CTRL_HUM]);
      p.putUChar (KEY_PANIC_OTA,   buf[OLD_PANIC_OTA]);
      p.putInt   (KEY_FAN_PWM,     ri(OLD_FAN_PWM));
      p.putFloat (KEY_HEAT_MAX_A,  rf(OLD_HEAT_MAX_A));
      p.putFloat (KEY_SKIN_T_MAX,  rf(OLD_SKIN_T_MAX));
      p.putFloat (KEY_AIR_T_MAX,   rf(OLD_AIR_T_MAX));
      p.putInt   (KEY_ACT_PERIOD,  ri(OLD_ACT_PERIOD));
      p.putInt   (KEY_PHOTO_PERIOD,ri(OLD_PHOTO_PERIOD));
      p.putInt   (KEY_STBY_PERIOD, ri(OLD_STBY_PERIOD));
      p.putInt   (KEY_FAN_CTL_PWM, ri(OLD_FAN_CTL_PWM));
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
      p.putString(KEY_TOKEN, String((char*)(buf + OLD_TB_TOKEN)));
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

    // Clear old namespace so migration doesn't run again
    old.begin("eeprom", false);
    old.clear();
    old.end();

    ESP_LOGI("APP", "Migración EEPROM → Preferences completada");
    return true;
  }
  ```

- [ ] **Step 5: Rewrite `initEEPROM()`**

  ```cpp
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
  ```

- [ ] **Step 6: Rewrite `recapVariables()`**

  ```cpp
  void recapVariables() {
    { Preferences p; p.begin(NS_CFG, true);
      autoLock              = p.getUChar(KEY_AUTOLOCK,   DEFAULT_AUTOLOCK);
      in3.language          = p.getUChar(KEY_LANG,       defaultLanguage);
      in3.serialNumber      = p.getInt  (KEY_SERIAL,     0);
      in3.controlMode       = p.getUChar(KEY_CTRL_MODE,  CONTROL_AIR);
      in3.desiredControlTemperature = p.getFloat(KEY_CTRL_TEMP,
                                        presetTemp[CONTROL_AIR]);
      in3.desiredControlHumidity    = p.getUChar(KEY_CTRL_HUM,
                                        presetHumidity);
      in3.fanPWM            = p.getInt  (KEY_FAN_PWM,    0);
      if (in3.fanPWM <= 0 || in3.fanPWM > 255) in3.fanPWM = 0;
      in3.heaterMaxPowerAmps       = p.getFloat(KEY_HEAT_MAX_A,  0.0f);
      if (isnan(in3.heaterMaxPowerAmps) || in3.heaterMaxPowerAmps <= 0)
        in3.heaterMaxPowerAmps = 0.0f;
      in3.skinTemperatureSetMax    = p.getFloat(KEY_SKIN_T_MAX,  0.0f);
      if (isnan(in3.skinTemperatureSetMax) || in3.skinTemperatureSetMax <= 0)
        in3.skinTemperatureSetMax = 0.0f;
      in3.airTemperatureSetMax     = p.getFloat(KEY_AIR_T_MAX,   0.0f);
      if (isnan(in3.airTemperatureSetMax) || in3.airTemperatureSetMax <= 0)
        in3.airTemperatureSetMax = 0.0f;
      in3.actuating_gprs_period    = p.getInt(KEY_ACT_PERIOD,    0);
      if (in3.actuating_gprs_period <= 0) in3.actuating_gprs_period = 0;
      in3.phototherapy_gprs_period = p.getInt(KEY_PHOTO_PERIOD,  0);
      if (in3.phototherapy_gprs_period <= 0) in3.phototherapy_gprs_period = 0;
      in3.standby_gprs_period      = p.getInt(KEY_STBY_PERIOD,   0);
      if (in3.standby_gprs_period <= 0) in3.standby_gprs_period = 0;
      in3.fanCtlPWM                = p.getInt(KEY_FAN_CTL_PWM,   FAN_CTL_PWM_DEFAULT);
      if (in3.fanCtlPWM <= 0 || in3.fanCtlPWM > 255)
        in3.fanCtlPWM = FAN_CTL_PWM_DEFAULT;
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
      String s = p.getString(KEY_SSID,     "");
      String pw = p.getString(KEY_PASSWORD, "");
      strncpy(wifi_ssid, s.c_str(),  sizeof(wifi_ssid));
      strncpy(wifi_pass, pw.c_str(), sizeof(wifi_pass));
      p.end(); }

    { Preferences p; p.begin(NS_STATE, true);
      in3.restoreState  = true; // kept for compatibility
      in3.actuation     = p.getUChar(KEY_ACTUATION,    0);
      in3.phototherapy  = p.getUChar(KEY_PHOTO_ACTIVE, 0);
      p.end(); }

    ESP_LOGI("APP", "Serial: %d, Lang: %d", in3.serialNumber, in3.language);
  }
  ```

- [ ] **Step 7: Rewrite `saveCalibrationToEEPROM()` (keep same name for compatibility)**

  ```cpp
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
  ```

- [ ] **Step 8: Verify the phototherapy Preferences block (already there) still compiles — it uses namespace "photo" which is separate and unchanged**

  Lines 234–243 in the old file use `Preferences p; p.begin("photo", true)`. This block is correct as-is; keep it.

- [ ] **Step 9: Build**

  Run: `pio run -e IncuNest_V16 2>&1 | grep -E "error:|warning:" | head -30`

  Expected: errors in other .cpp files about missing `EEPROM.*` symbols. EEPROM.cpp itself should be clean.

---

## Task 3 — motherBoard: Update write sites in remaining files

**Files:**
- Modify: `src/CommTask.cpp`, `src/GPRS.cpp`, `src/main.cpp`, `src/Wifi_OTA.cpp`, `src/userInterface.cpp`, `src/initHardware.cpp`, `src/UI_actuatorsProgress.cpp`, `src/updateData.cpp`

**Pattern to apply everywhere:**

```cpp
// BEFORE
EEPROM.writeXxx(EEPROM_SOME_KEY, value);
EEPROM.commit();

// AFTER
{ Preferences p; p.begin(NS_XXX, false); p.putXxx(KEY_SOME_KEY, value); p.end(); }
```

Multiple keys in the same namespace that are committed together can share one Preferences block:
```cpp
// BEFORE
EEPROM.writeInt(EEPROM_FAN_PWM, in3.fanPWM);
EEPROM.writeFloat(EEPROM_HEATER_MAX_AMPS, in3.heaterMaxPowerAmps);
EEPROM.commit();

// AFTER
{ Preferences p; p.begin(NS_CFG, false);
  p.putInt  (KEY_FAN_PWM,    in3.fanPWM);
  p.putFloat(KEY_HEAT_MAX_A, in3.heaterMaxPowerAmps);
  p.end(); }
```

- [ ] **Step 1: Update `src/CommTask.cpp` (~line 414–443)**

  ```cpp
  // Lines 414–443 — 7 writes + 1 commit → 1 Preferences block
  { Preferences p; p.begin(NS_CFG, false);
    p.putInt  (KEY_FAN_PWM,      in3.fanPWM);
    p.putFloat(KEY_HEAT_MAX_A,   in3.heaterMaxPowerAmps);
    p.putFloat(KEY_SKIN_T_MAX,   in3.skinTemperatureSetMax);
    p.putFloat(KEY_AIR_T_MAX,    in3.airTemperatureSetMax);
    p.putInt  (KEY_ACT_PERIOD,   in3.actuating_gprs_period);
    p.putInt  (KEY_PHOTO_PERIOD, in3.phototherapy_gprs_period);
    p.putInt  (KEY_STBY_PERIOD,  in3.standby_gprs_period);
    p.putInt  (KEY_FAN_CTL_PWM,  in3.fanCtlPWM);
    p.end(); }
  ```

  Remove any `#include <EEPROM.h>` from this file.

- [ ] **Step 2: Update `src/GPRS.cpp` (lines 391–393 and 403–405)**

  ```cpp
  // Both locations (token + provisioned)
  { Preferences p; p.begin(NS_GPRS, false);
    p.putString(KEY_TOKEN,       GPRS.device_token);
    p.putUChar (KEY_PROVISIONED, GPRS.provisioned);
    p.end(); }
  ```

  Also update the GPRS reads (~lines 798–800):
  ```cpp
  { Preferences p; p.begin(NS_GPRS, true);
    GPRS.provisioned  = p.getUChar (KEY_PROVISIONED, 0);
    GPRS.device_token = p.getString(KEY_TOKEN, "").c_str(); // or strncpy
    p.end(); }
  ```

- [ ] **Step 3: Update `src/main.cpp` (lines 400–405 and 448–449)**

  ```cpp
  // ~line 400–405
  { Preferences p; p.begin(NS_STATE, false);
    p.putUChar(KEY_ACTUATION,   in3.actuation);
    p.putUChar(KEY_CTRL_MODE,   in3.controlMode);
    p.end(); }

  // ~line 448–449
  { Preferences p; p.begin(NS_STATE, false);
    p.putUChar(KEY_PHOTO_ACTIVE, in3.phototherapy);
    p.end(); }
  ```

- [ ] **Step 4: Update `src/Wifi_OTA.cpp`**

  Reads at lines 283–284:
  ```cpp
  { Preferences p; p.begin(NS_WIFI, true);
    ssid = p.getString(KEY_SSID,     "");
    pass = p.getString(KEY_PASSWORD, "");
    p.end(); }
  ```

  Reads at lines 488–495:
  ```cpp
  { Preferences p; p.begin(NS_GPRS, true);
    Wifi_TB.provisioned  = p.getUChar (KEY_PROVISIONED, 0);
    Wifi_TB.device_token = p.getString(KEY_TOKEN, "").c_str();
    p.end(); }
  ```

  OTA parameter writes (lines 369–414):
  ```cpp
  { Preferences p; p.begin(NS_CFG, false);
    p.putInt  (KEY_SERIAL,       in3.serialNumber);
    p.putInt  (KEY_FAN_PWM,      in3.fanPWM);
    p.putInt  (KEY_FAN_CTL_PWM,  in3.fanCtlPWM);
    p.putFloat(KEY_HEAT_MAX_A,   in3.heaterMaxPowerAmps);
    p.putFloat(KEY_AIR_T_MAX,    in3.airTemperatureSetMax);
    p.putFloat(KEY_SKIN_T_MAX,   in3.skinTemperatureSetMax);
    p.putInt  (KEY_ACT_PERIOD,   in3.actuating_gprs_period);
    p.putInt  (KEY_PHOTO_PERIOD, in3.phototherapy_gprs_period);
    p.putInt  (KEY_STBY_PERIOD,  in3.standby_gprs_period);
    p.putFloat(KEY_FT_SKIN,      in3.fineTuneSkinTemperature);
    p.end(); }
  // Move calibration keys to NS_CAL if they are in OTA writes:
  { Preferences p; p.begin(NS_CAL, false);
    p.putFloat(KEY_FT_SKIN, in3.fineTuneSkinTemperature);
    p.end(); }
  ```

  ThingsBoard writes (lines 520–535, two locations):
  ```cpp
  { Preferences p; p.begin(NS_GPRS, false);
    p.putString(KEY_TOKEN,       Wifi_TB.device_token);
    p.putUChar (KEY_PROVISIONED, Wifi_TB.provisioned);
    p.end(); }
  ```

  WiFi credential write (lines 790–792):
  ```cpp
  { Preferences p; p.begin(NS_WIFI, false);
    p.putString(KEY_SSID,     pendingSSID);
    p.putString(KEY_PASSWORD, pendingPass);
    p.end(); }
  ```

- [ ] **Step 5: Update `src/userInterface.cpp`**

  ```cpp
  // ~line 235: ctrl mode
  { Preferences p; p.begin(NS_CFG, false); p.putUChar(KEY_CTRL_MODE, in3.controlMode); p.end(); }

  // ~line 278: ctrl temperature
  { Preferences p; p.begin(NS_CFG, false); p.putFloat(KEY_CTRL_TEMP, in3.desiredControlTemperature); p.end(); }

  // ~line 318: ctrl humidity
  { Preferences p; p.begin(NS_CFG, false); p.putUChar(KEY_CTRL_HUM, in3.desiredControlHumidity); p.end(); }

  // ~line 324: phototherapy
  { Preferences p; p.begin(NS_STATE, false); p.putUChar(KEY_PHOTO_ACTIVE, in3.phototherapy); p.end(); }

  // ~line 398: language
  { Preferences p; p.begin(NS_CFG, false); p.putUChar(KEY_LANG, in3.language); p.end(); }

  // ~line 409: serial number
  { Preferences p; p.begin(NS_CFG, false); p.putInt(KEY_SERIAL, in3.serialNumber); p.end(); }
  ```

  Check line 507 — find the corresponding write and apply the same pattern.

- [ ] **Step 6: Update `src/initHardware.cpp` (~lines 732–733 and 851–858)**

  ```cpp
  // Write heater test (~line 732)
  { Preferences p; p.begin(NS_CFG, false); p.putUChar(KEY_HEATER_TEST, 1); p.end(); }

  // Reads (~lines 851, 858) — replace EEPROM.read with:
  { Preferences p; p.begin(NS_CFG, true);
    uint8_t heaterTest = p.getUChar(KEY_HEATER_TEST, 0);
    p.end(); }
  ```

- [ ] **Step 7: Update `src/UI_actuatorsProgress.cpp` (lines 292 and 317)**

  ```cpp
  { Preferences p; p.begin(NS_STATE, false); p.putUChar(KEY_CTRL_ACTIVE, in3.actuation); p.end(); }
  ```

- [ ] **Step 8: Update `src/updateData.cpp` (lines 311–340)**

  ```cpp
  // Runtime accumulator writes (~lines 311–329)
  { Preferences p; p.begin(NS_RT, false);
    p.putFloat(KEY_RT_CTRL,   in3.control_active_time);
    p.putFloat(KEY_RT_HEATER, in3.heater_active_time);
    p.putFloat(KEY_RT_FAN,    in3.fan_active_time);
    p.putFloat(KEY_RT_HUM,    in3.humidifier_active_time);
    p.putFloat(KEY_RT_PHOTO,  in3.phototherapy_active_time);
    p.end(); }

  // ~line 339: standby time
  { Preferences p; p.begin(NS_RT, false); p.putFloat(KEY_RT_STANDBY, in3.standby_time); p.end(); }
  ```

- [ ] **Step 9: Remove all `#include <EEPROM.h>` from every file touched above**

- [ ] **Step 10: Full build**

  Run: `pio run -e IncuNest_V16 2>&1 | grep -c "error:"`

  Expected: 0 errors.

- [ ] **Step 11: Commit**

  ```bash
  git add motherBoard/include/main.h motherBoard/src/
  git commit -m "feat(motherBoard): migrate EEPROM to Preferences (named NVS keys)"
  ```

---

## Task 4 — Display_HMI: Replace EEPROM constants in `EEPROM_defines.h`

**Files:**
- Modify: `Display_HMI/include/EEPROM_defines.h`

- [ ] **Step 1: Remove all `#define EEPROM_*` offset constants and add Preferences constants**

  ```cpp
  #pragma once
  #include <Preferences.h>

  // --------------- Namespaces ---------------
  constexpr char HMI_NS_CFG[]  = "hmi_cfg";
  constexpr char HMI_NS_WIFI[] = "hmi_wifi";
  constexpr char HMI_NS_GPRS[] = "hmi_gprs";

  // --------------- Keys: hmi_cfg ---------------
  constexpr char HMI_KEY_LANG[]        = "lang";
  constexpr char HMI_KEY_SERIAL[]      = "serial";
  constexpr char HMI_KEY_AIR_TEMP[]    = "air_temp";
  constexpr char HMI_KEY_SKIN_TEMP[]   = "skin_temp";
  constexpr char HMI_KEY_HUMIDITY[]    = "humidity";
  constexpr char HMI_KEY_PHOTO_MIN[]   = "photo_min";
  constexpr char HMI_KEY_DARK_MODE[]   = "dark_mode";
  constexpr char HMI_KEY_HUM_EN[]      = "hum_en";
  constexpr char HMI_KEY_VOLUME[]      = "volume";
  constexpr char HMI_KEY_DISP_FREQ[]   = "disp_freq";
  constexpr char HMI_KEY_AA_WEIGHT[]   = "aa_weight";
  constexpr char HMI_KEY_AA_GEST[]     = "aa_gest";
  constexpr char HMI_KEY_AA_AGE_H[]    = "aa_age_h";

  // --------------- Keys: hmi_wifi ---------------
  constexpr char HMI_KEY_SSID[]     = "ssid";
  constexpr char HMI_KEY_PASSWORD[] = "password";

  // --------------- Keys: hmi_gprs ---------------
  constexpr char HMI_KEY_PROVISIONED[] = "provisioned";
  constexpr char HMI_KEY_TOKEN[]       = "token";
  ```

---

## Task 5 — Display_HMI: Rewrite `src/EEPROM.cpp`

**Files:**
- Modify: `Display_HMI/src/EEPROM.cpp`

- [ ] **Step 1: Replace header**

  ```cpp
  #include <Arduino.h>
  #include <Preferences.h>
  #include "EEPROM_defines.h"
  #include "main.h"

  extern char wifi_ssid[64];
  extern char wifi_pass[64];
  ```

- [ ] **Step 2: Rewrite `resetFlash()`**

  ```cpp
  void resetFlash() {
    Preferences p;
    const char* ns[] = { HMI_NS_CFG, HMI_NS_WIFI, HMI_NS_GPRS };
    for (auto n : ns) { p.begin(n, false); p.clear(); p.end(); }
  }
  ```

- [ ] **Step 3: Rewrite `loaddefaultValues()`**

  ```cpp
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
    p.end();
  }
  ```

- [ ] **Step 4: Add `migrateFromEEPROM()`**

  Display_HMI's old EEPROM blob is 263 bytes under namespace `"eeprom"`, key `"data"`. Note the inverted initialization logic in the old code (byte 10 ≠ 0 meant first boot; here we check it's NOT 0 to confirm data was written).

  ```cpp
  static bool migrateFromEEPROM() {
    constexpr int OLD_FIRST_TURN_ON  = 10;   // 0 = initialized in Display_HMI (inverted logic)
    constexpr int OLD_LANG           = 30;
    constexpr int OLD_SERIAL         = 40;
    constexpr int OLD_AIR_TEMP       = 80;
    constexpr int OLD_SKIN_TEMP      = 85;
    constexpr int OLD_HUMIDITY       = 90;
    constexpr int OLD_PHOTO_MIN      = 66;
    constexpr int OLD_DARK_MODE      = 252;
    constexpr int OLD_HUM_EN         = 257;
    constexpr int OLD_WIFI_SSID      = 115;  // 30 bytes
    constexpr int OLD_WIFI_PASS      = 145;  // 25 bytes
    constexpr int OLD_TB_PROV        = 200;
    constexpr int OLD_TB_TOKEN       = 205;  // 21 bytes
    constexpr int OLD_VOLUME         = 251;
    constexpr int OLD_DISP_FREQ      = 253;  // uint32, 4 bytes
    constexpr int OLD_AA_WEIGHT      = 258;  // uint16, 2 bytes
    constexpr int OLD_AA_GEST        = 260;  // uint8
    constexpr int OLD_AA_AGE_H       = 261;  // uint16, 2 bytes

    Preferences old;
    old.begin("eeprom", true);
    uint8_t buf[263] = {};
    size_t len = old.getBytes("data", buf, sizeof(buf));
    old.end();

    // Old Display_HMI logic: byte 10 == 0 means initialized
    if (len < 263 || buf[OLD_FIRST_TURN_ON] != 0) {
      return false;  // not initialized or no data
    }

    auto rf  = [&](int off) { float v;    memcpy(&v, buf + off, 4); return v; };
    auto ru  = [&](int off) { uint32_t v; memcpy(&v, buf + off, 4); return v; };
    auto ru16= [&](int off) { uint16_t v; memcpy(&v, buf + off, 2); return v; };

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
      p.putUShort(HMI_KEY_AA_WEIGHT, ru16(OLD_AA_WEIGHT));
      p.putUChar (HMI_KEY_AA_GEST,   buf[OLD_AA_GEST]);
      p.putUShort(HMI_KEY_AA_AGE_H,  ru16(OLD_AA_AGE_H));
      p.end(); }

    { Preferences p; p.begin(HMI_NS_WIFI, false);
      p.putString(HMI_KEY_SSID,     String((char*)(buf + OLD_WIFI_SSID)));
      p.putString(HMI_KEY_PASSWORD, String((char*)(buf + OLD_WIFI_PASS)));
      p.end(); }

    { Preferences p; p.begin(HMI_NS_GPRS, false);
      p.putUChar (HMI_KEY_PROVISIONED, buf[OLD_TB_PROV]);
      p.putString(HMI_KEY_TOKEN,       String((char*)(buf + OLD_TB_TOKEN)));
      p.end(); }

    old.begin("eeprom", false);
    old.clear();
    old.end();

    ESP_LOGI("APP", "Migración EEPROM → Preferences (HMI) completada");
    return true;
  }
  ```

- [ ] **Step 5: Rewrite `initEEPROM()`**

  ```cpp
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
  ```

  **Note:** The old inverted-logic magic byte is gone. `isKey(HMI_KEY_LANG)` is the canonical initialization check.

- [ ] **Step 6: Rewrite `recapVariables()`**

  ```cpp
  void recapVariables() {
    { Preferences p; p.begin(HMI_NS_CFG, true);
      g_lang             = p.getUChar (HMI_KEY_LANG,      LANG_EN);
      if (g_lang >= LANG_QTY) g_lang = LANG_EN;
      airTempValue       = p.getFloat (HMI_KEY_AIR_TEMP,  DEFAULT_AIR_TEMP);
      if (isnan(airTempValue) || airTempValue < 30 || airTempValue > 40)
        airTempValue = DEFAULT_AIR_TEMP;
      skinTempValue      = p.getFloat (HMI_KEY_SKIN_TEMP, DEFAULT_SKIN_TEMP);
      if (isnan(skinTempValue) || skinTempValue < 30 || skinTempValue > 40)
        skinTempValue = DEFAULT_SKIN_TEMP;
      humValue           = p.getUChar (HMI_KEY_HUMIDITY,  DEFAULT_HUMIDITY);
      photoTimerMinutes  = p.getUChar (HMI_KEY_PHOTO_MIN, PHOTO_TIMER_EEPROM_DEFAULT);
      darkMode           = p.getUChar (HMI_KEY_DARK_MODE, 0) != 0;
      humidityEnabled    = p.getUChar (HMI_KEY_HUM_EN,    0) != 0;
      in3.serialNumber   = p.getInt   (HMI_KEY_SERIAL,    0);
      p.end(); }

    { Preferences p; p.begin(HMI_NS_WIFI, true);
      String s  = p.getString(HMI_KEY_SSID,     "");
      String pw = p.getString(HMI_KEY_PASSWORD, "");
      strncpy(wifi_ssid, s.c_str(),  sizeof(wifi_ssid));
      strncpy(wifi_pass, pw.c_str(), sizeof(wifi_pass));
      p.end(); }
  }
  ```

- [ ] **Step 7: Build Display_HMI**

  Run: `pio run -e main 2>&1 | grep -E "error:" | head -20`

  Expected: errors in UITask, CommTask, Wifi_OTA. EEPROM.cpp should be clean.

---

## Task 6 — Display_HMI: Update write sites in remaining files

**Files:**
- Modify: `src/AudioManager.cpp`, `src/UITask.cpp`, `src/CommTask.cpp`, `src/Wifi_OTA.cpp`

- [ ] **Step 1: AudioManager.cpp — move volume read into `recapVariables()` (Task 5 Step 5)**

  Add to `recapVariables()` in EEPROM.cpp hmi_cfg block:
  ```cpp
  audioVolume = p.getUChar(HMI_KEY_VOLUME, 15);  // default = 15
  ```

  In AudioManager.cpp, replace the EEPROM read with a direct read from the global `audioVolume` variable (already loaded). Remove the EEPROM.h include.

  For volume write (if it exists — check AudioManager.cpp for write site):
  ```cpp
  { Preferences p; p.begin(HMI_NS_CFG, false); p.putUChar(HMI_KEY_VOLUME, newVolume); p.end(); }
  ```

- [ ] **Step 2: CommTask.cpp (~line 344) — serial number write**

  ```cpp
  { Preferences p; p.begin(HMI_NS_CFG, false); p.putInt(HMI_KEY_SERIAL, in3.serialNumber); p.end(); }
  ```

- [ ] **Step 3: UITask.cpp — apply write pattern to all write sites**

  ```cpp
  // ~line 223: display frequency write (inside lcd_set_freq_write — verify write exists before commit)
  { Preferences p; p.begin(HMI_NS_CFG, false); p.putUInt(HMI_KEY_DISP_FREQ, currentFreq); p.end(); }

  // ~line 530: language
  { Preferences p; p.begin(HMI_NS_CFG, false); p.putUChar(HMI_KEY_LANG, g_lang); p.end(); }

  // ~line 1128, 2094, 2130: air temp
  { Preferences p; p.begin(HMI_NS_CFG, false); p.putFloat(HMI_KEY_AIR_TEMP, airTempValue); p.end(); }

  // ~lines 1233–1235, ~line 3706: autoair params
  { Preferences p; p.begin(HMI_NS_CFG, false);
    p.putUShort(HMI_KEY_AA_WEIGHT, g_popupWeight);
    p.putUChar (HMI_KEY_AA_GEST,   g_popupGest);
    p.putUShort(HMI_KEY_AA_AGE_H,  g_popupAgeHours);
    p.end(); }

  // ~lines 1641, 1661: photo timer increment/decrement
  { Preferences p; p.begin(HMI_NS_CFG, false); p.putUChar(HMI_KEY_PHOTO_MIN, photoTimerMinutes); p.end(); }

  // ~line 1936: dark mode
  { Preferences p; p.begin(HMI_NS_CFG, false); p.putUChar(HMI_KEY_DARK_MODE, darkMode ? 1 : 0); p.end(); }

  // ~line 1943: humidity enabled
  { Preferences p; p.begin(HMI_NS_CFG, false); p.putUChar(HMI_KEY_HUM_EN, humidityEnabled ? 1 : 0); p.end(); }

  // ~lines 2100, 2136: skin temp
  { Preferences p; p.begin(HMI_NS_CFG, false); p.putFloat(HMI_KEY_SKIN_TEMP, skinTempValue); p.end(); }

  // ~lines 2168, 2196: humidity
  { Preferences p; p.begin(HMI_NS_CFG, false); p.putUChar(HMI_KEY_HUMIDITY, humValue); p.end(); }
  ```

  Also update reads in UITask (~lines 1461–1462, 3321–3323):
  ```cpp
  // WiFi credentials read
  { Preferences p; p.begin(HMI_NS_WIFI, true);
    savedSSID = p.getString(HMI_KEY_SSID,     "");
    savedPass = p.getString(HMI_KEY_PASSWORD, "");
    p.end(); }

  // Autoair reads
  { Preferences p; p.begin(HMI_NS_CFG, true);
    w = p.getUShort(HMI_KEY_AA_WEIGHT, 0);
    g = p.getUChar (HMI_KEY_AA_GEST,   0);
    a = p.getUShort(HMI_KEY_AA_AGE_H,  0);
    p.end(); }
  ```

- [ ] **Step 4: Wifi_OTA.cpp (~lines 179–180, 294–297, 331–332, 420–422)**

  ```cpp
  // WiFi credential reads
  { Preferences p; p.begin(HMI_NS_WIFI, true);
    ssid = p.getString(HMI_KEY_SSID,     "");
    pass = p.getString(HMI_KEY_PASSWORD, "");
    p.end(); }

  // ThingsBoard reads
  { Preferences p; p.begin(HMI_NS_GPRS, true);
    Wifi_TB.provisioned  = p.getUChar (HMI_KEY_PROVISIONED, 0);
    Wifi_TB.device_token = p.getString(HMI_KEY_TOKEN, "").c_str();
    p.end(); }

  // ThingsBoard writes
  { Preferences p; p.begin(HMI_NS_GPRS, false);
    p.putString(HMI_KEY_TOKEN,       Wifi_TB.device_token);
    p.putUChar (HMI_KEY_PROVISIONED, Wifi_TB.provisioned);
    p.end(); }

  // WiFi credential writes
  { Preferences p; p.begin(HMI_NS_WIFI, false);
    p.putString(HMI_KEY_SSID,     pendingSSID);
    p.putString(HMI_KEY_PASSWORD, pendingPass);
    p.end(); }
  ```

- [ ] **Step 5: Remove all `#include <EEPROM.h>` from every Display_HMI file touched**

- [ ] **Step 6: Full build**

  Run: `pio run -e main 2>&1 | grep -c "error:"`

  Expected: 0 errors.

- [ ] **Step 7: Commit**

  ```bash
  git add Display_HMI/include/EEPROM_defines.h Display_HMI/src/
  git commit -m "feat(Display_HMI): migrate EEPROM to Preferences (named NVS keys)"
  ```

---

## Task 7 — Update platformio.ini files (if EEPROM lib is explicit)

**Files:**
- Check: `motherBoard/platformio.ini`, `Display_HMI/platformio.ini`

- [ ] **Step 1: Check if EEPROM is listed under `lib_deps` in either platformio.ini**

  EEPROM is a built-in Arduino ESP32 library, so it usually doesn't appear in `lib_deps`. Preferences is also built-in. No changes likely needed. Verify:

  ```bash
  grep -i eeprom motherBoard/platformio.ini Display_HMI/platformio.ini
  ```

  Expected: no matches. If found, remove those lines.

- [ ] **Step 2: Commit if any changes**

---

## Task 8 — Update flasher tool NVS writer (bonus — serial number support)

> This task is a prerequisite for the serial number flasher feature. The migration establishes the namespace and key (`"mb_cfg"` / `"serial"`) that the flasher will write.

- [ ] **Step 1: Confirm `KEY_SERIAL = "serial"` in `NS_CFG = "mb_cfg"` namespace matches what the flasher NVS writer will generate**

  This is now the canonical location for the motherBoard serial number. No additional firmware changes needed for the flasher feature.

---

## Migration: Handled in Tasks 2 and 5

`migrateFromEEPROM()` runs automatically on the first boot after a firmware update:

1. Reads the old 512-byte (motherBoard) or 263-byte (Display_HMI) EEPROM blob from NVS namespace `"eeprom"`, key `"data"` — still present after the firmware update.
2. Validates the magic byte / initialization marker.
3. Parses all values using the old byte offsets (kept as local constants inside the function, not in any header).
4. Writes them into the new Preferences namespaces.
5. Clears the old `"eeprom"` namespace so migration doesn't repeat on the next boot.

If no old data is found (truly fresh device), `loaddefaultValues()` runs instead.

**Result:** WiFi credentials, calibration, serial number, and runtime counters all survive the firmware update. Transparent to the user.
