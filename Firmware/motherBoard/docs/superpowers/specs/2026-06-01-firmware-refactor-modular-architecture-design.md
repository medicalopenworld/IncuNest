# Firmware Refactor — Modular Architecture Design

**Date:** 2026-06-01
**Scope:** IncuNest motherBoard + Display_HMI (HW16 and HW17 only)
**Status:** Approved

---

## 1. Motivation

Four equally-weighted goals drive this refactor:

1. **Maintainability** — `main.h` is a 890-line monolith; adding a feature requires touching too many files; heavy `extern` coupling across all `.cpp` files.
2. **Field reliability** — shared mutable state (`in3`, global variables) accessed from multiple FreeRTOS tasks without consistent mutex protection; sensor error returns not always checked.
3. **Hardware scalability** — `#ifdef HW_NUM` guards are scattered throughout application code; adding HW18 would require changes in dozens of files.
4. **Patient safety** — no formal alarm state machine, no unit tests for critical logic (PID, alarms, calibration).

---

## 2. Scope

- **In scope:** motherBoard firmware + Display_HMI firmware + new shared library (`incunest_shared`).
- **Hardware versions:** HW16 and HW17 only. Legacy versions (HW6–HW15) are dropped from the new architecture.
- **Build system:** PlatformIO is retained. Arduino framework is retained for now (see Phase 9).
- **No time pressure:** correctness and safety over speed.

---

## 3. Architecture Overview

### Principles

1. **Each module is autonomous:** one `.h` (public API), one `.cpp` (implementation), private state. No loose `extern` variables.
2. **HAL is the only layer that knows about HW16/HW17.** Everything else calls `hal_gpio_write()`, `hal_i2c_read()`, etc.
3. **`DeviceState` replaces `in3` and all globals in `main.cpp`.** All access goes through a FreeRTOS mutex via `state_get()` / `state_set()`.
4. **Modules in `modules/` do not include `<Arduino.h>` directly** — only the HAL and drivers do. This makes their logic host-testable without hardware.
5. **`tasks/`** contains only FreeRTOS orchestration: task creation, periods, calls to module functions. Zero business logic.

### File Structure

```
Firmware/
  shared/                        ← incunest_shared library (new)
    library.json
    include/
      protocol.h                 ← all MB↔HMI message types (single source of truth)
      alarm_ids.h                ← ALARMS_ID enum (today duplicated/inconsistent)
      control_types.h            ← actuation modes, control modes, languages

  motherBoard/
    src/
      hal/
        hal.h                    ← hardware abstraction API
        hal_hw16.cpp             ← HW16 implementation
        hal_hw17.cpp             ← HW17 implementation
      drivers/
        drv_sts3x.h/cpp          ← SensirionI2cSts3x wrapper
        drv_shtc3.h/cpp          ← SHTC3 / SHT4x
        drv_ina3221.h/cpp        ← Beastdevices_INA3221
        drv_bq25730.h/cpp        ← BQ25730 charger
        drv_afe4490.h/cpp        ← AFE SpO2
      modules/
        sensors/                 ← acquisition, filtering, calibration
        control/                 ← PID, actuators, security/alarms
        comm/                    ← GPRS, WiFi, ThingsBoard, OTA
        ui/                      ← display, buzzer, backlight, encoder
      tasks/
        tasks.cpp                ← xTaskCreatePinnedToCore + minimal handlers only
      state/
        state.h                  ← DeviceState struct + accessor API
        state.cpp                ← singleton instance + mutex
      main.cpp                   ← very short setup() + loop()

  Display_HMI/
    src/
      hal/                       ← same pattern, HMI-specific pins
      modules/
        comm/                    ← CommTask (uses incunest_shared protocol types)
        ui/                      ← LVGL screens, UITask
        audio/                   ← AudioManager, buzzer
      tasks/
        tasks.cpp
      state/
        state.h/cpp
      main.cpp
```

---

## 4. HAL Design

### API (`hal/hal.h`)

```cpp
// GPIO
void     hal_gpio_write(uint8_t pin, bool value);
bool     hal_gpio_read(uint8_t pin);
void     hal_gpio_set_mode(uint8_t pin, uint8_t mode);

// PWM
void     hal_pwm_init(uint8_t channel, uint32_t freq_hz, uint8_t resolution_bits);
void     hal_pwm_write(uint8_t channel, uint32_t duty);

// I2C
bool     hal_i2c_write(TwoWire *bus, uint8_t addr, const uint8_t *data, size_t len);
bool     hal_i2c_read (TwoWire *bus, uint8_t addr, uint8_t *buf,  size_t len);

// ADC
uint32_t hal_adc_read_mv(uint8_t pin);

// Hardware configuration constants (differ by version)
extern const HalPinConfig  g_hal_pins;   // all GPIO assignments
extern const HalBusConfig  g_hal_buses;  // I2C buses, speeds
```

### Build-time Selection

```ini
[env:IncuNest_V16]
build_flags = -DHW_NUM=16
build_src_filter = +<hal/hal_hw16.cpp>

[env:IncuNest_V17]
build_flags = -DHW_NUM=17
build_src_filter = +<hal/hal_hw17.cpp>
```

All `#ifdef HW_NUM` guards disappear from application code. Only the two HAL implementation files retain them.

---

## 5. State Management (`state/`)

### DeviceState

Replaces `IncuNest_parameters in3` and all loose globals in `main.cpp`.

```cpp
struct DeviceState {
  // Sensors
  float    temperatureSkin;
  float    temperatureAir;
  float    temperatureAirRedundant;
  float    humidity;
  float    ambientTemperature;
  int      skinCapacitance;

  // Control
  bool     actuationActive;
  uint8_t  actuationMode;        // TEMP / HUM / TEMP+HUM
  bool     controlMode;          // SKIN / AIR
  float    desiredTemperature;
  float    desiredHumidity;

  // Phototherapy
  bool     phototherapyActive;
  uint8_t  phototherapyPwm;
  bool     photoFirstRun;
  uint32_t photoTurnOnTime;

  // Alarms — atomic bitmask, no mutex needed for set/clear
  uint32_t alarmBitmask;
  bool     alarmsEnabled;

  // Diagnostics
  uint32_t bootCount;
  float    freeHeapKb;
  uint32_t uptimeSeconds;

  // Communication
  uint8_t  commStatus;           // COMM_STATUS enum
  int      serialNumber;
  int      hwNum;
  char     hwRev[2];
  char     fwVer[20];
};
```

### Accessor API

```cpp
void        state_init();
DeviceState state_get();                      // full copy under mutex
void        state_set(const DeviceState &s);  // full write under mutex

// Individual accessors for hot paths (avoid copying full struct)
float       state_get_skin_temp();
void        state_set_alarm(uint8_t alarm_id, bool active);
bool        state_get_alarm(uint8_t alarm_id);
uint32_t    state_get_alarm_bitmask();
```

`alarmBitmask` uses `atomic_fetch_or` / `atomic_fetch_and` so the security module can set/clear alarms without acquiring the full state mutex.

---

## 6. Shared Library (`incunest_shared`)

Lives at `Firmware/shared/`. Both firmwares include it via `lib_deps` with a local path.

### `protocol.h` — Single source of truth for MB↔HMI protocol

Consolidates the 8+ message types currently duplicated/inconsistent between the two `CommTask.h` files:

```cpp
typedef struct { ... } CtrlTelemetry;          // CTRL→HMI, 1 Hz
typedef struct { ... } CtrlState;              // CTRL→HMI, on change
typedef struct { ... } CtrlAlarm;              // CTRL→HMI, alarm events
typedef struct { ... } CtrlPPG;               // CTRL→HMI, 25 Hz waveform
typedef struct { ... } CtrlVitals;            // CTRL→HMI, 1 Hz vitals
typedef struct { ... } CtrlProbe;             // CTRL→HMI, probe state
typedef struct { ... } HmiCommand;            // HMI→CTRL, user commands

typedef enum {
  SKIN_PROBE_NOT_CONNECTED = 0,
  SKIN_PROBE_PENDING_VALIDATION,
  SKIN_PROBE_VALID,
  SKIN_PROBE_INVALID,
  SKIN_PROBE_OUT_OF_RANGE,
  SKIN_PROBE_DISCONNECTED_DURING_OPERATION,
  SKIN_PROBE_UNSTABLE,
} SkinProbeState;
```

### `alarm_ids.h`

```cpp
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
} AlarmId;
```

### `control_types.h`

```cpp
typedef enum { CONTROL_SKIN = 0, CONTROL_AIR } ControlMode;
typedef enum { ACTUATION_OFF = 0, ACTUATION_TEMPERATURE, ACTUATION_HUMIDITY, ACTUATION_TEMP_AND_HUMIDITY } ActuationMode;
typedef enum { SPANISH = 0, ENGLISH, FRENCH, PORTUGUESE, NUM_LANGUAGES } Language;
```

---

## 7. Testing Strategy

Modules in `modules/` have no direct Arduino dependency, making them testable on the host with Unity (natively supported by PlatformIO).

Tests run in PlatformIO's native environment (Unity on Linux/Windows host, no hardware required).

**High-value test targets:**

| Module | Why |
|--------|-----|
| `control/pid` | Pure math, no hardware — easy to test, critical correctness |
| `control/alarms` | Patient safety state machine — must be exhaustively tested |
| `modules/sensors` (calibration, range validation) | Safety-critical: bad sensor values must be caught |
| `shared/protocol` | Serialization/deserialization — prevents MB↔HMI protocol drift |

---

## 8. Migration Phases

Each phase is independently compilable and deployable. Risk is isolated per phase.

| Phase | Content | Risk |
|-------|---------|------|
| 1 | Extract `incunest_shared`: unify protocol types, alarm IDs, control types | Very low |
| 2 | Create HAL for HW16/HW17; eliminate `#ifdef HW_NUM` from application code | Low |
| 3 | Decompose `main.h` into focused headers (`task_config.h`, `ui_constants.h`, `telemetry_keys.h`, etc.) | Low |
| 4 | Introduce `state/`: replace `in3` struct and all loose globals with `DeviceState` + mutex accessors | Medium |
| 5 | Modularize sensors and drivers (`drv_*`, `modules/sensors/`) | Medium |
| 6 | Modularize control and alarms (`modules/control/`) + add Unity unit tests | Medium-High |
| 7 | Modularize comm (GPRS / WiFi / ThingsBoard) into `modules/comm/` | Medium |
| 8 | Apply same architecture to Display_HMI (reusing `incunest_shared`) | Medium |
| 9 | (Long-term) Migrate HAL/drivers from Arduino APIs to ESP-IDF APIs, replacing Arduino framework; PlatformIO retained as build system | High |

### Phase 9 — Arduino → ESP-IDF Framework Migration

This is a standalone future project enabled by the HAL abstraction introduced in Phase 2. Once all hardware interactions are behind the HAL, replacing `Wire.begin()` with `i2c_master_init()` is a localized change in two files, not a codebase-wide change. Libraries with Arduino dependencies (TFT_eSPI, Adafruit SHT4x, RotaryEncoder) would be replaced with ESP-IDF native equivalents or ported at the driver layer. PlatformIO continues to be used as the build system throughout.

---

## 9. What Does NOT Change

- FreeRTOS task structure — the task topology (GPRS, sensors, security, UI, comm, backlight, etc.) is preserved; only the code inside each task becomes thinner.
- ThingsBoard telemetry keys — all `#define` keys in `main.h` are moved to `telemetry_keys.h` but their values are unchanged.
- CrashReporter and DriveUpload — these modules are already reasonably self-contained and are carried over as-is into `modules/`.
- OTA mechanism — retained as-is, moved into `modules/comm/`.
