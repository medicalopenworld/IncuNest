# Firmware Modular Architecture — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` (recommended) or `superpowers:executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Refactor IncuNest motherBoard and Display_HMI firmware into a modular architecture with HAL abstraction (HW16/17), centralized state management, shared protocol library, and unit-tested critical modules.

**Architecture:** Nine sequential phases, each independently compilable and deployable. Phases 1–3: structural only, no behavioral change. Phases 4–7: module extraction on motherBoard. Phase 8: same architecture on Display_HMI. Phase 9: ESP-IDF framework migration (long-term).

**Tech Stack:** ESP32-S3, PlatformIO + Arduino framework, FreeRTOS, Unity (host tests), C++17. Branch: `refactor/modular-architecture`.

**Spec:** `docs/superpowers/specs/2026-06-01-firmware-refactor-modular-architecture-design.md`

---

## Phase 1 — Extract `incunest_shared` Protocol Library

**Goal:** Move all MB↔HMI protocol types, alarm IDs, and control enums into a single PlatformIO library consumed by both firmwares. Zero behavioral change — builds must pass before and after.

**Why first:** The `SKIN_PROBE_*` defines and `COMM_STATUS` enum are duplicated/inconsistent between the two firmwares today. Every subsequent phase needs these types to be canonical before touching CommTask code.

### Files

- Create: `Firmware/shared/library.json`
- Create: `Firmware/shared/include/alarm_ids.h`
- Create: `Firmware/shared/include/control_types.h`
- Create: `Firmware/shared/include/protocol.h`
- Modify: `Firmware/motherBoard/platformio.ini` — add `lib_extra_dirs`
- Modify: `Firmware/Display_HMI/platformio.ini` — add `lib_extra_dirs`
- Modify: `Firmware/motherBoard/include/CommTask.h` — add includes + typedef aliases
- Modify: `Firmware/Display_HMI/include/CommTask.h` — add includes + typedef aliases
- Modify: `Firmware/Display_HMI/include/main.h` — remove duplicate `ACTUATION_*` defines and `COMM_STATUS` enum

---

### Task 1.1 — Create shared library scaffold

**Files:** Create `Firmware/shared/library.json`, `Firmware/shared/include/` (empty dir)

- [ ] Create directory `Firmware/shared/include/`

- [ ] Write `Firmware/shared/library.json`:
```json
{
  "name": "incunest_shared",
  "version": "1.0.0",
  "description": "Shared protocol types and enums for IncuNest MB<->HMI communication",
  "frameworks": ["arduino"],
  "platforms": ["espressif32"]
}
```

- [ ] Verify structure:
```
Firmware/shared/
  library.json
  include/   ← empty for now
```

---

### Task 1.2 — Write `alarm_ids.h`

**Files:** Create `Firmware/shared/include/alarm_ids.h`

- [ ] Write the file:
```cpp
#pragma once

typedef enum {
  NO_ALARMS = 0,
  HUMIDITY_ALARM,
  TEMPERATURE_ALARM,
  AIR_THERMAL_CUTOUT_ALARM,
  SKIN_THERMAL_CUTOUT_ALARM,
  AIR_SENSOR_ISSUE_ALARM,
  SKIN_SENSOR_ISSUE_ALARM,
  FAN_ISSUE_ALARM,
  HEATER_ISSUE_ALARM,
  POWER_SUPPLY_ALARM,
  NUM_ALARMS,
  MAX_ALARM_STRING_SIZE = 255,
} AlarmId;
```

---

### Task 1.3 — Write `control_types.h`

**Files:** Create `Firmware/shared/include/control_types.h`

- [ ] Write the file. Note: consolidates `ACTUATION_*` defines from both `main.h` files, `COMM_STATUS` from both, language enums (MB has `UI_LANGUAGES`, HMI has `ui_lang_t` — both become `Language`):
```cpp
#pragma once

typedef enum {
  ACTUATION_OFF = 0,
  ACTUATION_TEMPERATURE = 1,
  ACTUATION_HUMIDITY = 2,
  ACTUATION_TEMP_AND_HUMIDITY = 3,
} ActuationMode;

typedef enum {
  CONTROL_SKIN_MODE = 0,
  CONTROL_AIR_MODE  = 1,
} ControlMode;

typedef enum {
  SPANISH = 0,
  ENGLISH,
  FRENCH,
  PORTUGUESE,
  NUM_LANGUAGES,
} Language;

typedef enum {
  COMM_STATUS_NONE        = 0,
  COMM_STATUS_GPRS_ONLY   = 1,
  COMM_STATUS_GPRS_SERVER = 2,
  COMM_STATUS_WIFI_ONLY   = 3,
  COMM_STATUS_WIFI_SERVER = 4,
} CommStatus;
```

---

### Task 1.4 — Write `protocol.h`

**Files:** Create `Firmware/shared/include/protocol.h`

- [ ] Write the file. This consolidates the 8+ message types duplicated between the two `CommTask.h` files. Internal flags (`newCommand`, `shouldSendData`, `updated`) stay in each firmware's local CommTask — they are not protocol:
```cpp
#pragma once
#include <stdint.h>
#include "alarm_ids.h"
#include "control_types.h"

// Skin probe contact state — single source of truth.
// Replaces SKIN_PROBE_* #defines in both CommTask.h files.
typedef enum {
  SKIN_PROBE_NOT_CONNECTED = 0,
  SKIN_PROBE_PENDING_VALIDATION,
  SKIN_PROBE_VALID,
  SKIN_PROBE_INVALID,
  SKIN_PROBE_OUT_OF_RANGE,
  SKIN_PROBE_DISCONNECTED_DURING_OPERATION,
  SKIN_PROBE_UNSTABLE,
} SkinProbeState;

// CTRL→HMI: sensor telemetry (1 Hz)
typedef struct {
  double detectedAirTemperature;
  double detectedSkinTemperature;
  double detectedHumidity;
  int    serverCommStatus;
  int    serialNumber;
} Proto_CtrlTelemetry;

// CTRL→HMI: full device state (on change + on HMI boot request)
typedef struct {
  int      actuation;
  int      controlMode;
  double   desiredAirTemperature;
  double   desiredSkinTemperature;
  double   desiredHumidity;
  int      phototherapyMode;
  int      muteAlarm;
  int      serialNumber;
  int      hwNum;
  char     hwRev[2];
  char     fwVer[20];
  int      language;
  int      skinModeEnabled;
  int      serverCommStatus;
  int      photoMinutesRemaining;
  int      photoSecondsRemaining;
  uint32_t alarmBitmask;
  int      skinProbeState;
} Proto_CtrlState;

// CTRL→HMI: alarm event
typedef struct {
  int  id;
  char type[30];
  char description[100];
  bool state;
} Proto_CtrlAlarm;

// CTRL→HMI: PPG waveform sample (25 Hz)
typedef struct {
  uint8_t ppg;   // normalised 0–255
} Proto_CtrlPPG;

// CTRL→HMI: vital signs (1 Hz)
typedef struct {
  uint8_t hr;    // bpm; 0 = no valid signal
  uint8_t spo2;  // 0–100 %; 0 = no valid signal
} Proto_CtrlVitals;

// CTRL→HMI: SpO2 probe contact state
typedef struct {
  SkinProbeState state;
} Proto_CtrlProbe;

// HMI→CTRL: user command (data fields only — internal flags stay local)
typedef struct {
  int    actuation;
  int    controlMode;
  double desiredAirTemperature;
  double desiredSkinTemperature;
  double desiredHumidity;
  int    phototherapyMode;
  int    muteAlarm;
  int    language;
  int    skinModeEnabled;
  int    photoMinutesRemaining;
  int    babyWeightGrams;
  int    babyGestWeeks;
  int    babyAgeDays;
} Proto_HmiCommand;
```

---

### Task 1.5 — Wire shared lib into motherBoard

**Files:** Modify `Firmware/motherBoard/platformio.ini`

- [ ] Add `lib_extra_dirs` at the top level of `[env:main]` (after the existing `lib_deps` block):
```ini
lib_extra_dirs = ../shared
```

- [ ] Build to verify PlatformIO finds the library:
```
cd Firmware/motherBoard
pio run -e main 2>&1 | tail -5
```
Expected: `[SUCCESS]` — nothing references shared headers yet so no compile change.

---

### Task 1.6 — Wire shared lib into Display_HMI

**Files:** Modify `Firmware/Display_HMI/platformio.ini`

- [ ] Add `lib_extra_dirs` inside the shared `[env]` block (inherited by all environments):
```ini
lib_extra_dirs = ../shared
```

- [ ] Build:
```
cd Firmware/Display_HMI
pio run -e main 2>&1 | tail -5
```
Expected: `[SUCCESS]`.

---

### Task 1.7 — Migrate motherBoard `CommTask.h` to shared types

**Files:** Modify `Firmware/motherBoard/include/CommTask.h`

- [ ] Replace the full file content with:
```cpp
#pragma once
#include <Arduino.h>
#include "protocol.h"
#include "control_types.h"
#include "alarm_ids.h"

#if IS_HMI
#define EXPECTED_PREFIX "CTRL"
#else
#define EXPECTED_PREFIX "HMI"
#endif

// Skin probe values — now in SkinProbeState enum in protocol.h.
// These aliases maintain backward compatibility with existing MB code.
#define SKIN_PROBE_NOT_CONNECTED  ((int)::SKIN_PROBE_NOT_CONNECTED)
#define SKIN_PROBE_VALID          ((int)::SKIN_PROBE_VALID)

// Backward-compatibility aliases so callers compile unchanged.
// TelemetryMessage and HMI_CommandMessage are the MB-local wrappers
// that add internal flags on top of the shared protocol types.
typedef struct {
  // Protocol fields (from Proto_CtrlTelemetry)
  double detectedAirTemperature;
  double detectedSkinTemperature;
  double detectedHumidity;
  int    serverCommStatus;
  int    serialNumber;
} TelemetryMessage;

typedef struct {
  // Protocol fields (from Proto_HmiCommand)
  int    actuation;
  int    controlMode;
  double desiredAirTemperature;
  double desiredSkinTemperature;
  double desiredHumidity;
  int    phototherapyMode;
  int    muteAlarm;
  int    language;
  int    skinModeEnabled;
  int    photoMinutesRemaining;
  int    babyWeightGrams;
  int    babyGestWeeks;
  int    babyAgeDays;
  // MB-internal flags (not part of the protocol)
  bool   newBabyData;
  bool   newBabyDataForTelemetry;
  bool   newCommand;
} HMI_CommandMessage;

extern TelemetryMessage   ctrl_tel_msg;
extern HMI_CommandMessage hmi_cmd_msg;

void   CommunicationHost_Init();
void   Communication_Task(void *pv);
void   CommunicationHost_Send(const char *);
void   setHMIConnected(bool connected);
double getRemainingPhotoTime();
void   sendWifiToHMI(const char *ssid, const char *pass);
```

- [ ] Build:
```
cd Firmware/motherBoard
pio run -e main 2>&1 | tail -5
```
Expected: `[SUCCESS]`.

---

### Task 1.8 — Migrate Display_HMI `CommTask.h` to shared types

**Files:** Modify `Firmware/Display_HMI/include/CommTask.h`

- [ ] Replace the full file content with:
```cpp
#ifndef COMM_TASK_H
#define COMM_TASK_H

#include "main.h"
#include <Arduino.h>
#include <lvgl.h>
#include "protocol.h"
#include "control_types.h"
#include "alarm_ids.h"

#define COMMUNICATION_DEBUG true
#if COMMUNICATION_DEBUG
#define COMM_LOG(...) Serial.printf(__VA_ARGS__)
#else
#define COMM_LOG(...)
#endif

#define COMM_SERIAL Serial

// Backward-compatibility aliases for existing HMI code.
// Internal-flag wrappers (shouldSendData / updated) remain local.
typedef Proto_CtrlTelemetry   ControlBoard_Message_Telemetry;
typedef Proto_CtrlState       ControlBoard_Message_State;
typedef Proto_CtrlAlarm       ControlBoard_Message_Alarm;
typedef Proto_HmiCommand      HMI_Message;

// PPG/Vitals/Probe wrappers retain their `updated` flag (HMI-internal)
typedef struct { Proto_CtrlPPG    data; bool updated; } ControlBoard_Message_PPG;
typedef struct { Proto_CtrlVitals data; bool updated; } ControlBoard_Message_VIT;
typedef struct { Proto_CtrlProbe  data; bool updated; } ControlBoard_Message_Probe;

// Legacy name for the probe state enum
typedef SkinProbeState ProbeContactState;
// SPO2_PROBE_* aliases (HMI code uses these names)
#define SPO2_PROBE_DISCONNECTED ((int)SKIN_PROBE_NOT_CONNECTED)
#define SPO2_PROBE_NOT_APPLIED  ((int)SKIN_PROBE_PENDING_VALIDATION)
#define SPO2_PROBE_APPLIED      ((int)SKIN_PROBE_VALID)

// Sensor data message (HMI-internal, not a protocol type)
typedef struct {
  double temperature[3];
  double humidity[2];
  bool   shouldSendData;
} ControlBoard_Message;

extern volatile bool g_pwrOffActive;
extern volatile int  g_pwrOffRemainingMs;
constexpr int PWR_OFF_TOTAL_MS = 3000;

extern HMI_Message                   hmi_msg;
extern ControlBoard_Message          ctrl_msg;
extern ControlBoard_Message_Telemetry ctrl_tel_msg;
extern ControlBoard_Message_Alarm    ctrl_msg_alarm;
extern ControlBoard_Message_State    ctrl_state_msg;
extern ControlBoard_Message_PPG      ctrl_ppg_msg;
extern ControlBoard_Message_VIT      ctrl_vit_msg;
extern ControlBoard_Message_Probe    ctrl_probe_msg;
extern int  g_skinProbeState;
extern bool error;

void CreateCommTask();
void Communication_RequestState(void);
void Communication_UIReady(void);
void Communication_SendBootInfo(void);
void Communication_SendWiFiCredentials(const char *ssid, const char *password);

#endif
```

- [ ] Build:
```
cd Firmware/Display_HMI
pio run -e main 2>&1 | tail -5
```
Expected: `[SUCCESS]`.

---

### Task 1.9 — Remove duplicated enums from HMI `main.h`

**Files:** Modify `Firmware/Display_HMI/include/main.h`

- [ ] Remove the `ACTUATION_*` `#define` block (lines that define `ACTUATION_NONE`, `ACTUATION_TEMPERATURE`, `ACTUATION_HUMIDITY`, `ACTUATION_TEMP_AND_HUMIDITY`) — they are now in `control_types.h`.

- [ ] Remove the `COMM_STATUS` typedef enum — now in `control_types.h`.

- [ ] Add at the top of `main.h` (after existing includes):
```cpp
#include "control_types.h"
#include "alarm_ids.h"
```

- [ ] Build both firmwares:
```
cd Firmware/motherBoard && pio run -e main 2>&1 | tail -3
cd Firmware/Display_HMI && pio run -e main 2>&1 | tail -3
```
Expected: both `[SUCCESS]`.

---

### Task 1.10 — Commit Phase 1

- [ ] Stage and commit:
```bash
git add Firmware/shared/ \
        Firmware/motherBoard/platformio.ini \
        Firmware/motherBoard/include/CommTask.h \
        Firmware/Display_HMI/platformio.ini \
        Firmware/Display_HMI/include/CommTask.h \
        Firmware/Display_HMI/include/main.h
git commit -m "feat(shared): extract incunest_shared protocol library (Phase 1)"
```

---

## Phase 2 — HAL Abstraction for HW16 and HW17

**Goal:** Introduce `hal/hal.h` as the single hardware interface. Move all HW-version differences out of application code and into two implementation files. Add separate PlatformIO environments per HW version. No behavioral change.

### Files

- Create: `Firmware/motherBoard/src/hal/hal.h`
- Create: `Firmware/motherBoard/src/hal/hal_hw16.cpp`
- Create: `Firmware/motherBoard/src/hal/hal_hw17.cpp`
- Modify: `Firmware/motherBoard/platformio.ini` — add `IncuNest_V16` and `IncuNest_V17` envs
- Modify: `Firmware/motherBoard/include/board.h` — remove hardcoded `#define HW_NUM 17`

---

### Task 2.1 — Define HAL structs and API in `hal.h`

**Files:** Create `Firmware/motherBoard/src/hal/hal.h`

- [ ] Write the file:
```cpp
#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <stdint.h>

// Pin configuration for one hardware version
typedef struct {
  uint8_t buzzer;
  uint8_t heater;
  uint8_t fan;
  uint8_t fanCtl;
  uint8_t fanSpeedFeedback;
  uint8_t phototherapy;
  uint8_t actuatorsEn;
  uint8_t screenBacklight;
  uint8_t babyNtcPin;
  uint8_t babyTempEn;    // 0xFF = not present
  uint8_t onOffSwitch;   // 0xFF = not present
  uint8_t pwrEn;         // 0xFF = not present
  uint8_t gsmTxPin;
  uint8_t gsmRxPin;
  uint8_t uartMbTx;      // 0xFF = not present
  uint8_t uartMbRx;      // 0xFF = not present
  uint8_t i2cSda;
  uint8_t i2cScl;
  uint8_t i2c2Sda;       // 0xFF = not present (second I2C bus)
  uint8_t i2c2Scl;       // 0xFF = not present
  uint8_t afeMiso;
  uint8_t afeMosi;
  uint8_t afeSck;
  uint8_t afeAdcReady;
  uint8_t afeCs;
  uint8_t afeLedAlm;     // 0xFF = not present
  uint8_t usbEn;         // 0xFF = not present
  uint8_t usbFault;      // 0xFF = not present
} HalPinConfig;

// Bus configuration
typedef struct {
  uint32_t i2cSpeedHz;
  uint32_t i2c2SpeedHz;  // 0 = second bus not present
} HalBusConfig;

// Provided by hal_hwXX.cpp — selected at link time via platformio.ini
extern const HalPinConfig  g_hal_pins;
extern const HalBusConfig  g_hal_buses;

// ── GPIO ─────────────────────────────────────────────────────────────────────
void     hal_gpio_set_mode(uint8_t pin, uint8_t mode);
void     hal_gpio_write(uint8_t pin, bool value);
bool     hal_gpio_read(uint8_t pin);

// ── PWM ──────────────────────────────────────────────────────────────────────
void     hal_pwm_init(uint8_t channel, uint32_t freq_hz, uint8_t resolution_bits, uint8_t pin);
void     hal_pwm_write(uint8_t channel, uint32_t duty);

// ── I2C ──────────────────────────────────────────────────────────────────────
// Returns true on ACK, false on NACK/timeout
bool     hal_i2c_write(TwoWire *bus, uint8_t addr, const uint8_t *data, size_t len);
bool     hal_i2c_read (TwoWire *bus, uint8_t addr, uint8_t *buf,        size_t len);

// ── ADC ──────────────────────────────────────────────────────────────────────
uint32_t hal_adc_read_mv(uint8_t pin);
```

---

### Task 2.2 — Implement `hal_hw17.cpp`

**Files:** Create `Firmware/motherBoard/src/hal/hal_hw17.cpp`

- [ ] Write the file (pin values taken from `board.h` `#elif (HW_NUM >= 16)` block, HW17 specifics):
```cpp
#include "hal.h"
#include <Arduino.h>

const HalPinConfig g_hal_pins = {
  .buzzer           = 1,
  .heater           = 45,
  .fan              = 12,
  .fanCtl           = 11,
  .fanSpeedFeedback = 38,
  .phototherapy     = 13,
  .actuatorsEn      = 14,
  .screenBacklight  = 46,  // FAKE_PIN — backlight handled differently on V17
  .babyNtcPin       = 8,
  .babyTempEn       = 18,
  .onOffSwitch      = 4,
  .pwrEn            = 2,
  .gsmTxPin         = 9,
  .gsmRxPin         = 10,
  .uartMbTx         = 15,
  .uartMbRx         = 16,
  .i2cSda           = 47,
  .i2cScl           = 48,
  .i2c2Sda          = 20,
  .i2c2Scl          = 19,
  .afeMiso          = 37,
  .afeMosi          = 35,
  .afeSck           = 36,
  .afeAdcReady      = 17,
  .afeCs            = 21,
  .afeLedAlm        = 7,
  .usbEn            = 5,
  .usbFault         = 6,
};

const HalBusConfig g_hal_buses = {
  .i2cSpeedHz  = 10000,
  .i2c2SpeedHz = 10000,
};

// ── GPIO ─────────────────────────────────────────────────────────────────────
void hal_gpio_set_mode(uint8_t pin, uint8_t mode) { pinMode(pin, mode); }
void hal_gpio_write(uint8_t pin, bool value)       { digitalWrite(pin, value ? HIGH : LOW); }
bool hal_gpio_read(uint8_t pin)                    { return digitalRead(pin) == HIGH; }

// ── PWM ──────────────────────────────────────────────────────────────────────
void hal_pwm_init(uint8_t ch, uint32_t freq, uint8_t res, uint8_t pin) {
  ledcSetup(ch, freq, res);
  ledcAttachPin(pin, ch);
}
void hal_pwm_write(uint8_t ch, uint32_t duty) { ledcWrite(ch, duty); }

// ── I2C ──────────────────────────────────────────────────────────────────────
bool hal_i2c_write(TwoWire *bus, uint8_t addr, const uint8_t *data, size_t len) {
  bus->beginTransmission(addr);
  bus->write(data, len);
  return bus->endTransmission() == 0;
}
bool hal_i2c_read(TwoWire *bus, uint8_t addr, uint8_t *buf, size_t len) {
  if (bus->requestFrom((uint8_t)addr, (uint8_t)len) != (uint8_t)len) return false;
  for (size_t i = 0; i < len; i++) buf[i] = bus->read();
  return true;
}

// ── ADC ──────────────────────────────────────────────────────────────────────
uint32_t hal_adc_read_mv(uint8_t pin) { return analogReadMilliVolts(pin); }
```

---

### Task 2.3 — Implement `hal_hw16.cpp`

**Files:** Create `Firmware/motherBoard/src/hal/hal_hw16.cpp`

- [ ] Write the file (HW16 pin differences from the `#elif (HW_NUM >= 16)` block in `board.h`; same implementation functions):
```cpp
#include "hal.h"
#include <Arduino.h>

const HalPinConfig g_hal_pins = {
  .buzzer           = 1,
  .heater           = 45,
  .fan              = 12,
  .fanCtl           = 11,
  .fanSpeedFeedback = 38,
  .phototherapy     = 13,
  .actuatorsEn      = 14,
  .screenBacklight  = 46,
  .babyNtcPin       = 8,
  .babyTempEn       = 18,
  .onOffSwitch      = 4,
  .pwrEn            = 2,
  .gsmTxPin         = 9,
  .gsmRxPin         = 10,
  .uartMbTx         = 15,
  .uartMbRx         = 16,
  .i2cSda           = 47,
  .i2cScl           = 48,
  .i2c2Sda          = 20,
  .i2c2Scl          = 19,
  .afeMiso          = 37,
  .afeMosi          = 35,
  .afeSck           = 36,
  .afeAdcReady      = 17,
  .afeCs            = 21,
  .afeLedAlm        = 0xFF,  // not present on HW16
  .usbEn            = 5,
  .usbFault         = 6,
};

const HalBusConfig g_hal_buses = {
  .i2cSpeedHz  = 10000,
  .i2c2SpeedHz = 10000,
};

// Implementation is identical to HW17 — delegate via same bodies
void hal_gpio_set_mode(uint8_t pin, uint8_t mode) { pinMode(pin, mode); }
void hal_gpio_write(uint8_t pin, bool value)       { digitalWrite(pin, value ? HIGH : LOW); }
bool hal_gpio_read(uint8_t pin)                    { return digitalRead(pin) == HIGH; }

void hal_pwm_init(uint8_t ch, uint32_t freq, uint8_t res, uint8_t pin) {
  ledcSetup(ch, freq, res);
  ledcAttachPin(pin, ch);
}
void hal_pwm_write(uint8_t ch, uint32_t duty) { ledcWrite(ch, duty); }

bool hal_i2c_write(TwoWire *bus, uint8_t addr, const uint8_t *data, size_t len) {
  bus->beginTransmission(addr);
  bus->write(data, len);
  return bus->endTransmission() == 0;
}
bool hal_i2c_read(TwoWire *bus, uint8_t addr, uint8_t *buf, size_t len) {
  if (bus->requestFrom((uint8_t)addr, (uint8_t)len) != (uint8_t)len) return false;
  for (size_t i = 0; i < len; i++) buf[i] = bus->read();
  return true;
}

uint32_t hal_adc_read_mv(uint8_t pin) { return analogReadMilliVolts(pin); }
```

---

### Task 2.4 — Add HW16/HW17 environments to `platformio.ini`

**Files:** Modify `Firmware/motherBoard/platformio.ini`

- [ ] Rename the existing `[env:main]` to `[env:IncuNest_V17]` and add a new `[env:IncuNest_V16]`. Move shared settings to a `[common]` section:
```ini
[common]
platform = espressif32@6.6.0
framework = arduino
board = esp32-s3-devkitc-1
board_build.partitions = ESP32S3_OTA_partition_8MB.csv
board_build.f_cpu = 240000000L
board_build.f_flash = 80000000L
board_build.flash_mode = dio
monitor_speed = 115200
monitor_filters = esp32_exception_decoder
build_type = debug
lib_extra_dirs = ../shared
lib_deps =
  https://github.com/br3ttb/Arduino-PID-Library.git
  https://github.com/mathertel/RotaryEncoder.git
  https://github.com/adafruit/Adafruit-GFX-Library.git
  https://github.com/adafruit/Adafruit_BusIO.git
  https://github.com/sparkfun/SparkFun_SHTC3_Arduino_Library.git
  https://github.com/thingsboard/thingsboard-arduino-sdk.git#v0.13.0
  https://github.com/bblanchon/ArduinoStreamUtils.git
  https://github.com/vshymanskyy/TinyGSM.git
  https://github.com/vshymanskyy/StreamDebugger.git
  https://github.com/RobTillaart/TCA9555.git
  https://github.com/adafruit/Adafruit_SHT4X.git
  https://github.com/tttapa/Arduino-Filters.git
  https://github.com/medicalopenworld/incunest_afe4490.git#master
  https://github.com/arduino-libraries/ArduinoHttpClient.git
  https://github.com/Bodmer/TFT_eSPI.git
  https://github.com/ricemices/ArduinoHttpClient.git
  https://github.com/Sensirion/arduino-i2c-sts3x.git
  https://github.com/beast-devices/Arduino-INA3221.git
  bblanchon/ArduinoJson@6.21.5
  thingsboard/TBPubSubClient@2.9.4

[env:IncuNest_V17]
extends = common
build_flags =
    -DHW_NUM=17
    -DCORE_DEBUG_LEVEL=3
    -g3 -O0
    -DUSER_SETUP_LOADED=1
    -DTFT_INVERSION_ON=1
    -DILI9341_2_DRIVER=1
    -DTFT_MISO=19 -DTFT_MOSI=23 -DTFT_SCLK=18
    -DTFT_CS=15 -DTFT_DC=0 -DTFT_RST=-1 -DTOUCH_CS=-1
    -DLOAD_GLCD=1 -DSPI_FREQUENCY=27000000
    -D LV_LVGL_H_INCLUDE_SIMPLE
    -I./include
    -Wno-attributes
build_src_filter = +<*> -<hal/hal_hw16.cpp>

[env:IncuNest_V16]
extends = common
build_flags =
    -DHW_NUM=16
    -DCORE_DEBUG_LEVEL=3
    -g3 -O0
    -DUSER_SETUP_LOADED=1
    -DTFT_INVERSION_ON=1
    -DILI9341_2_DRIVER=1
    -DTFT_MISO=19 -DTFT_MOSI=23 -DTFT_SCLK=18
    -DTFT_CS=15 -DTFT_DC=0 -DTFT_RST=-1 -DTOUCH_CS=-1
    -DLOAD_GLCD=1 -DSPI_FREQUENCY=27000000
    -D LV_LVGL_H_INCLUDE_SIMPLE
    -I./include
    -Wno-attributes
build_src_filter = +<*> -<hal/hal_hw17.cpp>
```

> **Note:** The TFT/display build flags may need tuning based on whether HW16 uses the same display as HW17. Verify with Pablo before tagging HW16 as production-ready.

---

### Task 2.5 — Remove hardcoded `HW_NUM` from `board.h`

**Files:** Modify `Firmware/motherBoard/include/board.h`

- [ ] Remove line 25: `#define HW_NUM 17` (now provided by build flag `-DHW_NUM=17`).

- [ ] Add a compile-time guard at the top of `board.h` to catch missing build flag:
```cpp
#ifndef HW_NUM
#error "HW_NUM must be defined via build_flags in platformio.ini"
#endif
```

- [ ] Build both environments:
```
pio run -e IncuNest_V17 2>&1 | tail -3
pio run -e IncuNest_V16 2>&1 | tail -3
```
Expected: both `[SUCCESS]`.

---

### Task 2.6 — Commit Phase 2

- [ ] Stage and commit:
```bash
git add Firmware/motherBoard/src/hal/ \
        Firmware/motherBoard/platformio.ini \
        Firmware/motherBoard/include/board.h
git commit -m "feat(hal): introduce HAL abstraction for HW16/HW17, add per-version PIO envs (Phase 2)"
```

---

## Phase 3 — Decompose `main.h`

**Goal:** Break the 890-line `main.h` monolith into focused headers. No behavioral change — every `#define` and type keeps its exact value. `main.h` becomes a thin aggregator that includes the new headers.

### Files (new headers)

- Create: `Firmware/motherBoard/include/task_config.h` — task priorities + period constants
- Create: `Firmware/motherBoard/include/telemetry_keys.h` — all `#define KEY_*` and `#define *_KEY` strings
- Create: `Firmware/motherBoard/include/ui_constants.h` — display dimensions, colors, UI enums
- Create: `Firmware/motherBoard/include/preferences_keys.h` — NVS namespaces + key name `constexpr` strings (already well-grouped in `main.h`)
- Modify: `Firmware/motherBoard/include/main.h` — replace extracted content with `#include` directives

---

### Task 3.1 — Extract `task_config.h`

**Files:** Create `Firmware/motherBoard/include/task_config.h`

- [ ] Cut all task priority and period `#define`s from `main.h` (lines ~373–416) and write them to `task_config.h`:
```cpp
#pragma once

// ── Task priorities (higher = more priority) ─────────────────────────────────
#define POWER_MANAGEMENT_TASK_PRIORITY   1
#define TIME_TRACK_TASK_PRIORITY         2
#define BACKLIGHT_TASK_PRIORITY          3
#define OTA_TASK_PRIORITY                4
#define GPRS_TAST_PRIORITY               5
#define BUZZER_TASK_PRIORITY             6
#define UI_TASK_PRIORITY                 7
#define COMMUNICATION_TASK_PRIORITY      7
#define COMMUNICATION_RECEIVER_PRIORITY  7
#define SENSORS_TASK_PRIORITY            8
#define SPO2_TASK_PRIORITY               8
#define SECURITY_TASK_PRIORITY           9
#define GPRS_MONITOR_TASK_PRIORITY       10

// ── Task periods (ms) ─────────────────────────────────────────────────────────
#define POWER_MANAGEMENT_TASK_PERIOD_MS  50
#define GPRS_TASK_PERIOD_MS              1
#define OTA_TASK_PERIOD_MS               50
#define SENSORS_TASK_PERIOD_MS           1
#define SPO2_TASK_PERIOD_MS              1
#define SKIN_SENSOR_UPDATE_PERIOD_MS     200
#define ROOM_SENSOR_UPDATE_PERIOD_MS     5000
#define ROOM_SENSOR_RECONNECT_MS         500
#define DIGITAL_CURRENT_SENSOR_PERIOD_MS 5
#define BUZZER_TASK_PERIOD_MS            10
#define UI_TASK_PERIOD_MS                10
#define SECURITY_TASK_PERIOD_MS          1
#define COMMUNICATION_TASK_PERIOD_MS     1
#define TIME_TRACK_TASK_PERIOD_MS        100
#define BACKLIGHT_TASK_PERIOD_MS         100
#define FAN_TASK_PERIOD_MS               10
#define LOOP_TASK_PERIOD_MS              1000
#define CALIBRATION_TASK_PERIOD_MS       100
#define GPRS_MONITOR_TASK_PERIOD         5000
#define GPRS_MONITOR_TASK_DELETE         30000

// ── Power management ──────────────────────────────────────────────────────────
#define PWR_HOLD_MS                  3000
#define PWR_OFF_UPDATE_INTERVAL_MS   200

// ── FreeRTOS core assignment ──────────────────────────────────────────────────
#define CORE_MONITOR_FREERTOS  0
#define CORE_ID_FREERTOS       1
```

- [ ] In `main.h`, replace those lines with:
```cpp
#include "task_config.h"
```

- [ ] Build:
```
pio run -e IncuNest_V17 2>&1 | tail -3
```
Expected: `[SUCCESS]`.

---

### Task 3.2 — Extract `telemetry_keys.h`

**Files:** Create `Firmware/motherBoard/include/telemetry_keys.h`

- [ ] Cut all `#define *_KEY "..."` telemetry string constants from `main.h` (lines 250–352, from `#define SN_KEY` through `#define HMI_LAST_RST_KEY`) into `telemetry_keys.h`. The file starts with:
```cpp
#pragma once

// ThingsBoard telemetry key strings — verbatim from main.h lines 250–352
#define SN_KEY                             "SN"
#define HW_NUM_KEY                         "HW_num"
#define HW_REV_KEY                         "HW_revision"
#define FW_VERSION_KEY                     "FW_version"
#define CCID_KEY                           "CCID"
// (copy remaining lines 254–352 of main.h here without modification)
```

- [ ] In `main.h`, replace those lines with:
```cpp
#include "telemetry_keys.h"
```

- [ ] Build and verify: `pio run -e IncuNest_V17 2>&1 | tail -3`

---

### Task 3.3 — Extract `ui_constants.h`

**Files:** Create `Firmware/motherBoard/include/ui_constants.h`

- [ ] Cut all UI-related `#define`s and enum typedefs from `main.h` (lines 132–172 for display dimensions/colors, lines 163–248 for `UI_PAGES`, `MAIN_MENU_UI`, `SETTINGS_MENU_UI`, `CALIBRATION_MENU_UI`, `ALARMS_ID`, `COMM_STATUS`, `UI_EVENTS_ID`) into `ui_constants.h`:
```cpp
#pragma once

// Display layout — verbatim from main.h lines 132–172
#define valuePosition     245
#define separatorPosition 240
#define unitPosition      315
#define textFontSize      2
// (copy remaining display #defines from main.h lines 133–172)

// UI enums — verbatim from main.h lines 163–248
typedef enum { MAIN_MENU_PAGE = 1, ACTUATORS_PROGRESS_PAGE, ... } UI_PAGES;
// (copy remaining enum definitions from main.h lines 164–248)
```

- [ ] In `main.h`, replace with: `#include "ui_constants.h"`

- [ ] Build: `pio run -e IncuNest_V17 2>&1 | tail -3`

---

### Task 3.4 — Extract `preferences_keys.h`

**Files:** Create `Firmware/motherBoard/include/preferences_keys.h`

- [ ] Cut the NVS namespace `constexpr` strings and all `KEY_*` `constexpr` strings from `main.h` (lines ~439–491) into `preferences_keys.h`:
```cpp
#pragma once

constexpr char NS_CFG[]   = "mb_cfg";
constexpr char NS_CAL[]   = "mb_cal";
constexpr char NS_WIFI[]  = "mb_wifi";
constexpr char NS_GPRS[]  = "mb_gprs";
constexpr char NS_RT[]    = "mb_rt";
constexpr char NS_STATE[] = "mb_state";

constexpr char KEY_LANG[]        = "lang";
// ... (all remaining KEY_* constexpr strings verbatim from main.h)
```

- [ ] In `main.h`, replace with: `#include "preferences_keys.h"`

- [ ] Build both environments:
```
pio run -e IncuNest_V17 2>&1 | tail -3
pio run -e IncuNest_V16 2>&1 | tail -3
```
Expected: both `[SUCCESS]`.

---

### Task 3.5 — Commit Phase 3

- [ ] Commit:
```bash
git add Firmware/motherBoard/include/
git commit -m "refactor(headers): decompose main.h into task_config, telemetry_keys, ui_constants, preferences_keys (Phase 3)"
```

---

## Phase 4 — Introduce `DeviceState` (replace `in3` and loose globals)

**Goal:** Replace `IncuNest_parameters in3` and the ~40 loose globals in `main.cpp` with a single `DeviceState` struct accessed through mutex-protected functions. All tasks read/write state through `state_get()` / `state_set()`.

**Why this matters for safety:** Currently multiple FreeRTOS tasks write to `in3` without synchronization. This phase makes concurrent access explicit and safe.

### Files

- Create: `Firmware/motherBoard/src/state/state.h`
- Create: `Firmware/motherBoard/src/state/state.cpp`
- Modify: `Firmware/motherBoard/include/main.h` — remove `IncuNest_parameters` struct, add state.h include
- Modify: `Firmware/motherBoard/src/main.cpp` — replace `in3` declarations with `state_init()`
- Modify: All `.cpp` files that access `in3.*` — replace with `state_get()`/`state_set()` calls

---

### Task 4.1 — Write `state.h`

**Files:** Create `Firmware/motherBoard/src/state/state.h`

- [ ] Write the file:
```cpp
#pragma once
#include <stdint.h>
#include <stdbool.h>

// Central device state — replaces IncuNest_parameters in3 + loose globals.
// All FreeRTOS tasks must use state_get()/state_set() or the individual
// field accessors. Never access g_device_state directly.
typedef struct {
  // Sensors
  double   temperatureSkin;
  double   temperatureAir;
  double   temperatureAirRedundant;
  double   humidity;
  double   ambientTemperature;
  int      skinCapacitance;
  float    fan_rpm;

  // Control
  int      actuation;
  bool     controlMode;
  bool     temperatureControl;
  bool     humidityControl;
  double   desiredControlTemperature;
  double   desiredControlHumidity;

  // Phototherapy
  bool     phototherapy;
  uint8_t  phototherapy_intensity;
  bool     photoFirstRun;
  long     photoTurnOnTime;

  // Calibration
  double   fineTuneSkinTemperature;
  double   fineTuneAirTemperature;
  bool     calibrationError;

  // Power / current measurements
  double   system_current;
  double   system_voltage;
  double   heater_current;
  double   fan_current;
  double   humidifier_current;
  double   humidifier_voltage;
  double   phototherapy_current;
  double   BATTERY_current;
  double   BATTERY_voltage;
  int      heaterSafeMAXPWM;

  // Alarms
  bool     alarmsEnabled;
  bool     alarmToReport[10];      // indexed by AlarmId
  char     alarmMessage[255];
  bool     previousAlarmReport;

  // Runtime tracking
  float    standby_time;
  float    control_active_time;
  float    heater_active_time;
  float    fan_active_time;
  float    phototherapy_active_time;
  float    humidifier_active_time;
  long     last_check_time;

  // Device identity
  int      serialNumber;
  int      resetReason;
  bool     restoreState;
  uint8_t  language;

  // Settings
  int      fanPwrSupplyPWM;
  int      fanCtlPWM;
  float    heaterMaxPowerAmps;
  float    skinTemperatureSetMax;
  float    airTemperatureSetMax;
  int      actuating_gprs_period;
  int      phototherapy_gprs_period;
  int      standby_gprs_period;

  // Comm
  uint8_t  commStatus;
} DeviceState;

void        state_init(void);
DeviceState state_get(void);
void        state_set(const DeviceState *s);

// Fast single-field accessors (avoid copying the full struct on hot paths)
double      state_get_skin_temp(void);
double      state_get_air_temp(void);
double      state_get_humidity(void);
int         state_get_actuation(void);
bool        state_get_phototherapy(void);
void        state_set_alarm(uint8_t alarm_id, bool active);
bool        state_get_alarm(uint8_t alarm_id);
uint32_t    state_get_alarm_bitmask(void);
void        state_set_commstatus(uint8_t status);
```

---

### Task 4.2 — Write `state.cpp`

**Files:** Create `Firmware/motherBoard/src/state/state.cpp`

- [ ] Write the file:
```cpp
#include "state.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>
#include <stdatomic.h>

static DeviceState       g_state   = {};
static SemaphoreHandle_t g_mutex   = NULL;
static atomic_uint       g_alarms  = 0;  // mirrors alarmToReport as bitmask

void state_init(void) {
  g_mutex = xSemaphoreCreateRecursiveMutex();
  memset(&g_state, 0, sizeof(g_state));
  g_state.heaterSafeMAXPWM      = 255;
  g_state.alarmsEnabled         = true;
  g_state.photoFirstRun         = true;
  g_state.phototherapy_intensity = 255;
}

DeviceState state_get(void) {
  DeviceState copy;
  xSemaphoreTakeRecursive(g_mutex, portMAX_DELAY);
  copy = g_state;
  xSemaphoreGiveRecursive(g_mutex);
  return copy;
}

void state_set(const DeviceState *s) {
  xSemaphoreTakeRecursive(g_mutex, portMAX_DELAY);
  g_state = *s;
  xSemaphoreGiveRecursive(g_mutex);
}

double   state_get_skin_temp(void)    { xSemaphoreTakeRecursive(g_mutex, portMAX_DELAY); double v = g_state.temperatureSkin; xSemaphoreGiveRecursive(g_mutex); return v; }
double   state_get_air_temp(void)     { xSemaphoreTakeRecursive(g_mutex, portMAX_DELAY); double v = g_state.temperatureAir;  xSemaphoreGiveRecursive(g_mutex); return v; }
double   state_get_humidity(void)     { xSemaphoreTakeRecursive(g_mutex, portMAX_DELAY); double v = g_state.humidity;        xSemaphoreGiveRecursive(g_mutex); return v; }
int      state_get_actuation(void)    { xSemaphoreTakeRecursive(g_mutex, portMAX_DELAY); int v    = g_state.actuation;       xSemaphoreGiveRecursive(g_mutex); return v; }
bool     state_get_phototherapy(void) { xSemaphoreTakeRecursive(g_mutex, portMAX_DELAY); bool v   = g_state.phototherapy;    xSemaphoreGiveRecursive(g_mutex); return v; }

void state_set_alarm(uint8_t alarm_id, bool active) {
  if (alarm_id >= 10) return;
  if (active) atomic_fetch_or(&g_alarms,  (1u << alarm_id));
  else        atomic_fetch_and(&g_alarms, ~(1u << alarm_id));
  xSemaphoreTakeRecursive(g_mutex, portMAX_DELAY);
  g_state.alarmToReport[alarm_id] = active;
  xSemaphoreGiveRecursive(g_mutex);
}

bool     state_get_alarm(uint8_t alarm_id)  { return alarm_id < 10 && (atomic_load(&g_alarms) & (1u << alarm_id)) != 0; }
uint32_t state_get_alarm_bitmask(void)       { return atomic_load(&g_alarms); }

void state_set_commstatus(uint8_t status) {
  xSemaphoreTakeRecursive(g_mutex, portMAX_DELAY);
  g_state.commStatus = status;
  xSemaphoreGiveRecursive(g_mutex);
}
```

---

### Task 4.3 — Replace `in3` in `main.cpp` and propagate

**Files:** Modify `Firmware/motherBoard/src/main.cpp` and all files that reference `in3.*`

- [ ] In `main.cpp`, remove the `IncuNest_parameters in3;` declaration. Add `#include "state/state.h"` and call `state_init()` at the top of `setup()` before `initEEPROM()`.

- [ ] Run a grep to find all files referencing `in3.`:
```
grep -r "in3\." src/ --include="*.cpp" -l
```

- [ ] For each file found, replace `in3.fieldName` with either:
  - A `state_get()` call (read): `DeviceState s = state_get(); use(s.fieldName);`
  - A `state_set()` call (write): `DeviceState s = state_get(); s.fieldName = val; state_set(&s);`
  - Or a single-field accessor where one exists (`state_get_skin_temp()`, etc.)

- [ ] Build: `pio run -e IncuNest_V17 2>&1 | tail -5`
Expected: `[SUCCESS]`.

---

### Task 4.4 — Commit Phase 4

```bash
git add Firmware/motherBoard/src/state/ \
        Firmware/motherBoard/src/main.cpp \
        Firmware/motherBoard/src/*.cpp \
        Firmware/motherBoard/include/main.h
git commit -m "refactor(state): introduce DeviceState replacing in3 and loose globals (Phase 4)"
```

---

## Phase 5 — Modularize Sensors and Drivers

**Goal:** Extract sensor acquisition logic into focused modules. Each driver wraps exactly one hardware component. The sensors module owns reading, filtering, and range-checking logic.

### Files

- Create: `src/drivers/drv_sts3x.h/cpp` — thin wrapper over `SensirionI2cSts3x`
- Create: `src/drivers/drv_shtc3.h/cpp` — wrapper for SHTC3 / SHT4x
- Create: `src/drivers/drv_ina3221.h/cpp` — wrapper for `Beastdevices_INA3221`
- Create: `src/drivers/drv_bq25730.h/cpp` — move `BQ25730.cpp` logic here
- Create: `src/drivers/drv_afe4490.h/cpp` — wrapper over `incunest_afe4490` lib
- Create: `src/modules/sensors/sensors_module.h/cpp` — orchestrates driver reads, filters, updates state
- Modify: `src/main.cpp` / `src/tasks/` — `sensors_Task` calls `sensors_module_update()`

---

### Task 5.1 — Create driver wrappers

For each of the five drivers below, the pattern is the same:

**`drv_sts3x.h`** (example):
```cpp
#pragma once
#include <SensirionI2cSts3x.h>
#include <Wire.h>

typedef struct { float temperature; bool ok; } DrvSts3xResult;

bool          drv_sts3x_init(SensirionI2cSts3x *dev, TwoWire *bus, uint8_t addr);
DrvSts3xResult drv_sts3x_read(SensirionI2cSts3x *dev);
```

**`drv_sts3x.cpp`**:
```cpp
#include "drv_sts3x.h"

bool drv_sts3x_init(SensirionI2cSts3x *dev, TwoWire *bus, uint8_t addr) {
  dev->begin(*bus, addr);
  return true;
}

DrvSts3xResult drv_sts3x_read(SensirionI2cSts3x *dev) {
  DrvSts3xResult r = {};
  float t = 0;
  uint16_t err = dev->measureSingleShot(REPEATABILITY_HIGH, false, t);
  r.ok = (err == 0);
  r.temperature = r.ok ? t : 0.0f;
  return r;
}
```

- [ ] Create `drv_sts3x.h/cpp` as shown above.
- [ ] Create `drv_shtc3.h/cpp` following the same pattern for SHTC3/SHT4x.
- [ ] Create `drv_ina3221.h/cpp` wrapping `Beastdevices_INA3221` with `{float current_ma; float voltage_mv; bool ok;}` result.
- [ ] Create `drv_bq25730.h/cpp` by moving the existing `BQ25730.cpp` logic (keep same public API).
- [ ] Create `drv_afe4490.h/cpp` as a thin wrapper (delegates to `incunest_afe4490` lib).

---

### Task 5.2 — Create `sensors_module`

**Files:** Create `src/modules/sensors/sensors_module.h/cpp`

- [ ] `sensors_module.h`:
```cpp
#pragma once

void sensors_module_init(void);
void sensors_module_update(void);   // called from sensors_Task every tick
```

- [ ] `sensors_module.cpp`: Move the body of `measureSkinSensor()`, `updateRoomSensor()`, `updateAmbientSensor()`, and `powerMonitor()` here. Each function reads from a driver, validates range, and writes to state via `state_set_*` accessors.

- [ ] In `sensors_Task` in `main.cpp`, replace direct function calls with `sensors_module_update()`.

- [ ] Build: `pio run -e IncuNest_V17 2>&1 | tail -5`

---

### Task 5.3 — Commit Phase 5

```bash
git add Firmware/motherBoard/src/drivers/ \
        Firmware/motherBoard/src/modules/sensors/ \
        Firmware/motherBoard/src/main.cpp
git commit -m "refactor(sensors): extract driver wrappers and sensors module (Phase 5)"
```

---

## Phase 6 — Modularize Control and Alarms + Unit Tests

**Goal:** Extract PID, alarm state machine, and security logic into a testable `modules/control/` module. Add Unity host tests for the alarm state machine and PID.

### Files

- Create: `src/modules/control/control_module.h/cpp`
- Create: `src/modules/control/alarm_machine.h/cpp`
- Create: `test/test_alarms/test_alarm_machine.cpp`
- Create: `test/test_pid/test_pid.cpp`
- Modify: `platformio.ini` — add `[env:native]` for host tests

---

### Task 6.1 — Add Unity native test environment

**Files:** Modify `Firmware/motherBoard/platformio.ini`

- [ ] Add at the end of `platformio.ini`:
```ini
[env:native]
platform = native
test_framework = unity
build_flags = -std=c++17 -DNATIVE_TEST
lib_extra_dirs = ../shared
```

- [ ] Create `test/test_alarms/test_alarm_machine.cpp`:
```cpp
#include <unity.h>
#include "modules/control/alarm_machine.h"

void test_alarm_set_clears_correctly(void) {
  alarm_machine_init();
  alarm_machine_set(TEMPERATURE_ALARM, true);
  TEST_ASSERT_TRUE(alarm_machine_get(TEMPERATURE_ALARM));
  alarm_machine_set(TEMPERATURE_ALARM, false);
  TEST_ASSERT_FALSE(alarm_machine_get(TEMPERATURE_ALARM));
}

void test_alarm_bitmask_is_consistent(void) {
  alarm_machine_init();
  alarm_machine_set(HUMIDITY_ALARM, true);
  alarm_machine_set(FAN_ISSUE_ALARM, true);
  uint32_t mask = alarm_machine_bitmask();
  TEST_ASSERT_TRUE(mask & (1u << HUMIDITY_ALARM));
  TEST_ASSERT_TRUE(mask & (1u << FAN_ISSUE_ALARM));
  TEST_ASSERT_FALSE(mask & (1u << TEMPERATURE_ALARM));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_alarm_set_clears_correctly);
  RUN_TEST(test_alarm_bitmask_is_consistent);
  return UNITY_END();
}
```

- [ ] Run to verify the test fails (alarm_machine not yet written):
```
pio test -e native --filter test_alarms 2>&1 | tail -10
```
Expected: compile error — `alarm_machine.h` not found.

---

### Task 6.2 — Write `alarm_machine.h/cpp`

- [ ] `alarm_machine.h`:
```cpp
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "alarm_ids.h"

void     alarm_machine_init(void);
void     alarm_machine_set(AlarmId id, bool active);
bool     alarm_machine_get(AlarmId id);
uint32_t alarm_machine_bitmask(void);
bool     alarm_machine_any_active(void);
bool     alarm_machine_any_critical(void);
```

- [ ] `alarm_machine.cpp`:
```cpp
#include "alarm_machine.h"
#include <string.h>

static uint32_t g_bitmask = 0;

void alarm_machine_init(void)                  { g_bitmask = 0; }
void alarm_machine_set(AlarmId id, bool active) {
  if (id >= NUM_ALARMS) return;
  if (active) g_bitmask |=  (1u << id);
  else        g_bitmask &= ~(1u << id);
}
bool     alarm_machine_get(AlarmId id)    { return id < NUM_ALARMS && (g_bitmask & (1u << id)) != 0; }
uint32_t alarm_machine_bitmask(void)      { return g_bitmask; }
bool     alarm_machine_any_active(void)   { return g_bitmask != 0; }
bool     alarm_machine_any_critical(void) {
  const uint32_t CRITICAL_MASK =
    (1u << TEMPERATURE_ALARM) | (1u << AIR_THERMAL_CUTOUT_ALARM) |
    (1u << SKIN_THERMAL_CUTOUT_ALARM) | (1u << FAN_ISSUE_ALARM);
  return (g_bitmask & CRITICAL_MASK) != 0;
}
```

- [ ] Run tests:
```
pio test -e native --filter test_alarms 2>&1 | tail -10
```
Expected: `OK - 2 tests passed`.

---

### Task 6.3 — Write PID unit tests

**Files:** Create `test/test_pid/test_pid.cpp`

- [ ] Write the file:
```cpp
#include <unity.h>
#include "modules/control/pid_wrapper.h"

void test_pid_output_zero_when_at_setpoint(void) {
  PidWrapper pid;
  pid_init(&pid, 1.0f, 0.0f, 0.0f, 0.0f, 255.0f);
  float out = pid_compute(&pid, 36.5f, 36.5f, 100);
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, out);
}

void test_pid_positive_output_below_setpoint(void) {
  PidWrapper pid;
  pid_init(&pid, 2.0f, 0.0f, 0.0f, 0.0f, 255.0f);
  float out = pid_compute(&pid, 36.5f, 35.0f, 100);
  TEST_ASSERT_GREATER_THAN_FLOAT(0.0f, out);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_pid_output_zero_when_at_setpoint);
  RUN_TEST(test_pid_positive_output_below_setpoint);
  return UNITY_END();
}
```

- [ ] Create `src/modules/control/pid_wrapper.h` with the interface the test expects:
```cpp
#pragma once
#include <stdbool.h>

typedef struct {
  float kp, ki, kd;
  float output_min, output_max;
  float integral;
  float prev_error;
} PidWrapper;

void  pid_init(PidWrapper *pid, float kp, float ki, float kd,
               float out_min, float out_max);
float pid_compute(PidWrapper *pid, float setpoint, float measured,
                  unsigned long dt_ms);
void  pid_reset(PidWrapper *pid);
```

- [ ] Create `src/modules/control/pid_wrapper.cpp` implementing those functions by delegating to the existing `Arduino-PID-Library` (or reimplementing the three-term formula directly — the library is a single-class wrapper around the same math).

- [ ] Run: `pio test -e native --filter test_pid 2>&1 | tail -10`
Expected: `OK - 2 tests passed`.

---

### Task 6.4 — Create `control_module`

- [ ] Create `src/modules/control/control_module.h/cpp` that:
  - Calls `alarm_machine_*` for all alarm logic (moved from `security.cpp`)
  - Calls `pid_wrapper_*` for PID computation (moved from `PID.cpp`)
  - Exposes `control_module_update()` called from `security_Task` and `Communication_Receiver`

- [ ] Build: `pio run -e IncuNest_V17 2>&1 | tail -5`

---

### Task 6.5 — Commit Phase 6

```bash
git add Firmware/motherBoard/src/modules/control/ \
        Firmware/motherBoard/test/ \
        Firmware/motherBoard/platformio.ini
git commit -m "refactor(control): extract control/alarm modules, add Unity host tests (Phase 6)"
```

---

## Phase 7 — Modularize Comm (GPRS / WiFi / ThingsBoard)

**Goal:** Move `GPRS.cpp`, `Wifi_OTA.cpp`, and `CommTask.cpp` into `src/modules/comm/`. The telemetry publish functions read from state via `state_get()` — no more direct access to `in3`.

### Files

- Create: `src/modules/comm/comm_gprs.h/cpp` — wraps `GPRS.cpp`
- Create: `src/modules/comm/comm_wifi.h/cpp` — wraps `Wifi_OTA.cpp`
- Create: `src/modules/comm/comm_task.h/cpp` — wraps `CommTask.cpp`
- Delete (after migration): `src/GPRS.cpp`, `src/Wifi_OTA.cpp` (moved into modules)

---

### Task 7.1 — Move GPRS and WiFi into modules

- [ ] Move `src/GPRS.cpp` → `src/modules/comm/comm_gprs.cpp`, update include paths.
- [ ] Move `src/Wifi_OTA.cpp` → `src/modules/comm/comm_wifi.cpp`, update include paths.
- [ ] Create `src/modules/comm/comm_module.h`:
```cpp
#pragma once

void comm_module_gprs_init(void);
void comm_module_gprs_handler(void);
bool comm_module_gprs_connected(void);

void comm_module_wifi_init(void);
void comm_module_wifi_handler(void);
bool comm_module_wifi_connected(void);
```
- [ ] In `tasks/`, update `GPRS_Task` and `OTA_WIFI_Task` to call `comm_module_*` functions.
- [ ] Build: `pio run -e IncuNest_V17 2>&1 | tail -5`

---

### Task 7.2 — Replace direct `in3` access in telemetry publishes

- [ ] In `comm_gprs.cpp` and `comm_wifi.cpp`, grep for remaining `in3.` references:
```
grep -n "in3\." src/modules/comm/ --include="*.cpp"
```
- [ ] Replace each with a `state_get()` call or single-field accessor.
- [ ] Build and verify.

---

### Task 7.3 — Commit Phase 7

```bash
git add Firmware/motherBoard/src/modules/comm/ \
        Firmware/motherBoard/src/tasks/
git commit -m "refactor(comm): move GPRS/WiFi/CommTask into modules/comm (Phase 7)"
```

---

## Phase 8 — Apply Same Architecture to Display_HMI

**Goal:** Apply the module/HAL/state pattern to Display_HMI following the exact same structure as the motherBoard. The HMI already has `incunest_shared` wired in (Phase 1), so Phases 2–7 are repeated for the HMI codebase.

### Sub-phases (mirror of MB phases 2–7)

- **8.1** HAL for HMI hardware (GPIO expander PCA9557, I2C, display SPI)
- **8.2** Decompose HMI `main.h` into `task_config.h`, `ui_constants.h`
- **8.3** Introduce `HmiState` (replaces HMI globals: `hmi_msg`, `ctrl_state_msg`, etc.)
- **8.4** Modularize `CommTask.cpp` → `modules/comm/`
- **8.5** Modularize `UITask.cpp` + LVGL → `modules/ui/`
- **8.6** Modularize `AudioManager.cpp` / `buzzer.cpp` → `modules/audio/`

Each sub-phase follows the same task structure (write code, build, commit) as the motherBoard phases. Write a dedicated plan for this phase before starting: `docs/superpowers/plans/2026-XX-XX-hmi-modular-architecture.md`.

---

## Phase 9 — ESP-IDF Framework Migration (Long-term)

**Goal:** Replace the Arduino framework with ESP-IDF within PlatformIO. Enabled by the HAL abstraction from Phase 2 — hardware interactions are already isolated to two files per firmware.

### Prerequisite

All of Phases 1–8 must be complete. The HAL is the migration surface.

### Sub-phases

- **9.1** Change `platformio.ini` framework from `arduino` to `espidf` for one environment (`IncuNest_V17`). Fix all Arduino-specific calls that now fail to compile.
- **9.2** Replace `Wire`-based I2C in `hal_hw17.cpp` with `i2c_master_init()` / `i2c_master_transmit()`.
- **9.3** Replace `ledcSetup/ledcWrite` with `ledc_timer_config()` / `ledc_channel_config()`.
- **9.4** Replace `analogReadMilliVolts` with `esp_adc_cali_*` / `adc_oneshot_*`.
- **9.5** Replace Arduino libraries that depend on Arduino framework (TFT_eSPI, Adafruit SHT4x, RotaryEncoder) with ESP-IDF equivalents or ported implementations.
- **9.6** Apply same migration to `IncuNest_V16` and Display_HMI.

Write a dedicated plan for this phase when Phase 8 is complete.

---

## Appendix: Build Commands Reference

```bash
# motherBoard
cd Firmware/motherBoard
pio run -e IncuNest_V17           # build HW17
pio run -e IncuNest_V16           # build HW16
pio test -e native                 # run host unit tests (all)
pio test -e native --filter test_alarms  # run alarm tests only
pio test -e native --filter test_pid     # run PID tests only

# Display_HMI
cd Firmware/Display_HMI
pio run -e main                    # build HMI
pio run -e crash_test_hmi          # build HMI crash-test variant

# Flash + monitor
pio run -e IncuNest_V17 -t upload && pio device monitor
```
