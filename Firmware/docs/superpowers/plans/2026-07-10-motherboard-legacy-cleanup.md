# motherBoard Legacy Cleanup Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove verifiably-dead code left in `motherBoard` after the 2026-07-08 on-board-UI removal — orphaned config defines, functions with zero call sites, dead comment blocks, and (the largest chunk) `#if (HW_NUM<16)` branches that no longer compile under any real build environment.

**Architecture:** Pure subtraction, no new abstractions. Four phases ordered by risk: dead comments (0) → unused functions/globals (1) → dead `HW_NUM` branches (2) → now-orphaned config headers that depended on phase 2 (3). Each phase compiles clean on both real hardware environments before moving to the next.

**Tech Stack:** PlatformIO, Arduino framework, C++17, ESP32-S3.

**Design doc:** `docs/superpowers/specs/2026-07-10-motherboard-legacy-cleanup-design.md`

## Global Constraints

- Scope is `motherBoard` only — do not touch `Display_HMI` or `shared/`.
- `platformio.ini` defines exactly two real environments: `IncuNest_V17` (`HW_NUM=17`) and `IncuNest_V16` (`HW_NUM=16`); `main` extends `IncuNest_V17`. `[env:native]` does not define `HW_NUM` and only builds `modules/control/` — unaffected by this plan.
- Per `.claude/rules/testing.md`: none of the files touched here (`config/*.h`, `main.h`, `main.cpp`, `system/*.cpp`, `tasks/*.cpp`, `modules/comm/CommTask.cpp` — wait, `tasks/CommTask.cpp`, `modules/sensors/sensors_module.cpp`) have Unity coverage. TDD is not exigible. Verification per task is compilation of both real environments, not a failing/passing test.
- Verify command after every task: `pio run -e IncuNest_V16 -e IncuNest_V17` (run from `motherBoard/`). Must compile with no new warnings.
- Commit message scope: `refactor(motherboard): ...` (Conventional Commits, no test/no feat — this is pure removal). One commit per task. Author is Pablo Sánchez Bergasa; never add a Claude/Anthropic co-author line (`.claude/rules/commits.md`).
- `EEPROM.cpp` (`OLD_*` constants, `migrateFromEEPROM()`) is explicitly OUT of scope — do not touch it in any task below.

---

### Task 1: Phase 0 — remove dead comment blocks

**Files:**
- Modify: `motherBoard/src/main.cpp:85`, `:219-226`
- Modify: `motherBoard/src/tasks/Wifi_OTA.cpp:1008-1013`
- Modify: `motherBoard/src/tasks/GPRS.cpp:488`, `:812-813`
- Modify: `motherBoard/src/system/initHardware.cpp:1109`, `:1124`
- Modify: `motherBoard/src/system/security.cpp:282-284`, `:865-868`
- Modify: `motherBoard/src/drivers/IncuNest_humidifier.h:61`

**Interfaces:** None — comment-only removal, zero behavior change, no signatures affected.

- [ ] **Step 1: `main.cpp` — remove the dead alt-driver comment (line 85)**

Before:
```cpp
MAM_IncuNest_Humidifier in3_hum(DEFAULT_ADDRESS);
// Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);
TFT_eSPI tft = TFT_eSPI(); // Invoke custom library
```

After:
```cpp
MAM_IncuNest_Humidifier in3_hum(DEFAULT_ADDRESS);
TFT_eSPI tft = TFT_eSPI(); // Invoke custom library
```

- [ ] **Step 2: `main.cpp` — remove the abandoned task-restart block (lines 219-226)**

Before:
```cpp
        vTaskDelete(taskHandle); // Delete the hung task
        // Serial.println("Task deleted. Restarting task...");

        // // Optionally restart the task
        // while (xTaskCreatePinnedToCore(GPRS_Task, (const char *)"GPRS", 8192,
        //                                NULL, GPRS_TAST_PRIORITY, &taskHandle,
        //                                CORE_ID_FREERTOS) != pdPASS)
        //   ;
        // logI("GPRS task successfully created!\n");
        vTaskDelete(NULL); // Delete the monitor task
```

After:
```cpp
        vTaskDelete(taskHandle); // Delete the hung task
        vTaskDelete(NULL); // Delete the monitor task
```

- [ ] **Step 3: `Wifi_OTA.cpp` — remove the disabled periodic debug log (lines 1008-1013)**

Before:
```cpp
void WifiOTAHandler(void) {
  // static long lastLog = 0;
  // if (millis() - lastLog > 5000) {
  //   ESP_LOGI(TAG, "WifiOTAHandler alive. WiFi status: %d. IP: %s",
  //            WiFi.status(), WiFi.localIP().toString().c_str());
  //   lastLog = millis();
  // }

  if (WIFI_EN && WiFi.status() != WL_CONNECTED) {
```

After:
```cpp
void WifiOTAHandler(void) {
  if (WIFI_EN && WiFi.status() != WL_CONNECTED) {
```

- [ ] **Step 4: `GPRS.cpp` — remove the two dead comment lines (488, 812-813)**

Before (line 488):
```cpp
  if (!updateRequestSent) {
    tb.Start_Firmware_Update(OTAcallback);
    // updateRequestSent = tb.Subscribe_Firmware_Update(callback);
  }
```

After:
```cpp
  if (!updateRequestSent) {
    tb.Start_Firmware_Update(OTAcallback);
  }
```

Before (lines 812-813):
```cpp
      if (millis() - GPRS.lastSent > secsToMillis(GPRS.sendPeriod)) {
        // Send our firmware title and version
        // StaticJsonDocument<JSON_OBJECT_SIZE(2)> TB_telemetries;
        // JsonObject telemetriesObject = TB_telemetries.to<JsonObject>();

        logModemData("[GPRS] -> sendPeriod is " + String(GPRS.sendPeriod) +
```

After:
```cpp
      if (millis() - GPRS.lastSent > secsToMillis(GPRS.sendPeriod)) {
        // Send our firmware title and version
        logModemData("[GPRS] -> sendPeriod is " + String(GPRS.sendPeriod) +
```

- [ ] **Step 5: `initHardware.cpp` — remove the two dead FAN digitalWrite comments (lines 1109, 1124)**

Before (around 1108-1114):
```cpp
  offsetCurrent = measureMeanConsumption(MAIN, FAN_SHUNT_CHANNEL);
// digitalWrite(FAN, HIGH);
#if (HW_NUM >= 8)
  ledcWrite(FAN_PWM_CHANNEL, in3.fanPwrSupplyPWM);
#else
  digitalWrite(FAN, HIGH);
#endif
```

After:
```cpp
  offsetCurrent = measureMeanConsumption(MAIN, FAN_SHUNT_CHANNEL);
#if (HW_NUM >= 8)
  ledcWrite(FAN_PWM_CHANNEL, in3.fanPwrSupplyPWM);
#else
  digitalWrite(FAN, HIGH);
#endif
```

Before (around 1123-1129):
```cpp
  in3.fan_current_test = testCurrent;
  // digitalWrite(FAN, LOW);
#if (HW_NUM >= 8)
  ledcWrite(FAN_PWM_CHANNEL, 0);
#else
  digitalWrite(FAN, LOW);
#endif
```

After:
```cpp
  in3.fan_current_test = testCurrent;
#if (HW_NUM >= 8)
  ledcWrite(FAN_PWM_CHANNEL, 0);
#else
  digitalWrite(FAN, LOW);
#endif
```

- [ ] **Step 6: `security.cpp` — remove the two dead comment fragments (282-284, 865-868)**

Before (275-294 area):
```cpp
  if (alarmID)
  {
    // if (xQueueReceive(sharedSensorQueue, &lastSuccesfullSensorUpdate[sensor],
    // portMAX_DELAY))
    // {
    if (millis() - lastSuccesfullSensorUpdate[sensor] >
        MINIMUM_SUCCESSFULL_SENSOR_UPDATE)
```

After:
```cpp
  if (alarmID)
  {
    if (millis() - lastSuccesfullSensorUpdate[sensor] >
        MINIMUM_SUCCESSFULL_SENSOR_UPDATE)
```

Before (864-869 area):
```cpp
  }
  // if (!ongoingAlarms())
  // {
  //   shutBuzzer();
  // }
}
```

After:
```cpp
  }
}
```

- [ ] **Step 7: `IncuNest_humidifier.h` — remove the dead default-constructor comment (line 61)**

Before:
```cpp
  MAM_IncuNest_Humidifier(IncuNestHum_addr_t addr) : _i2c_addr(addr){};
  //    MAM_IncuNest_Humidifier();

  // Initializes i2c humidifier
```

After:
```cpp
  MAM_IncuNest_Humidifier(IncuNestHum_addr_t addr) : _i2c_addr(addr){};

  // Initializes i2c humidifier
```

- [ ] **Step 8: Compile both real environments**

Run (from `motherBoard/`): `pio run -e IncuNest_V16 -e IncuNest_V17`
Expected: both build successfully, no new warnings.

- [ ] **Step 9: Commit**

```bash
git add src/main.cpp src/tasks/Wifi_OTA.cpp src/tasks/GPRS.cpp src/system/initHardware.cpp src/system/security.cpp src/drivers/IncuNest_humidifier.h
git commit -m "refactor(motherboard): remove dead comment blocks (phase 0 of legacy cleanup)"
```

---

### Task 2: Phase 1 — remove functions and globals with zero call sites

**Files:**
- Modify: `motherBoard/include/main.h:444`, `:466-469`, `:477`
- Modify: `motherBoard/src/system/security.cpp:492-514`
- Modify: `motherBoard/src/system/Buzzer.cpp:47-51`
- Modify: `motherBoard/src/tasks/Wifi_OTA.cpp:350`
- Modify: `motherBoard/src/main.cpp` (globals `ypos`, `print_text`, `initialSensorPosition`, `pos_text`)
- Modify: `motherBoard/src/system/initHardware.cpp:109-111` (extern decls)
- Modify: `motherBoard/src/system/ISR.cpp:91-94` (extern decls)
- Modify: `motherBoard/src/system/security.cpp:109-112` (extern decls)

**Interfaces:** None consumed. Produces: `main.h` no longer declares `alarmPendingToDisplay()`, `alarmPendingToClear()`, `clearDisplayedAlarm()`, `clearAlarmPendingToClear()`, `buzzerConstantTone()`, `wifiDisable()` — confirm no later task in this plan calls them (none do).

- [ ] **Step 1: `security.cpp` — delete the four unused alarm-display functions (492-514)**

Before:
```cpp
int alarmPendingToDisplay()
{
  for (int i = 0; i < NUM_ALARMS; i++)
  {
    if (displayAlarm[i])
      return i;
  }
  return 0;
}

void clearDisplayedAlarm(byte alarm) { displayAlarm[alarm] = false; }

void clearAlarmPendingToClear(byte alarm) { clearedAlarm[alarm] = false; }

int alarmPendingToClear()
{
  for (int i = 0; i < NUM_ALARMS; i++)
  {
    if (clearedAlarm[i])
      return i;
  }
  return 0;
}

void sendPendingAlarms()
```

After:
```cpp
void sendPendingAlarms()
```

(If `displayAlarm[]`/`clearedAlarm[]` arrays become unused after this deletion, leave them — confirm with a grep for `displayAlarm\[` / `clearedAlarm\[` before removing them; they are not in the design doc's candidate list and must not be removed speculatively in this task.)

- [ ] **Step 2: `Buzzer.cpp` — delete `buzzerConstantTone()` (47-51)**

Before:
```cpp
void buzzerConstantTone(int freq)
{
  logI("[BUZZER] -> BUZZER activated in constant Mode");
  ledcWrite(BUZZER_PWM_CHANNEL, BUZZER_HALF_PWM);
}

void shutBuzzer()
```

After:
```cpp
void shutBuzzer()
```

- [ ] **Step 3: `Wifi_OTA.cpp` — delete `wifiDisable()` (350)**

Before:
```cpp
void wifiDisable() { WiFi.mode(WIFI_OFF); }

void configWifiServer() {
```

After:
```cpp
void configWifiServer() {
```

- [ ] **Step 4: `main.h` — remove the now-dangling prototypes**

Before:
```cpp
void securityCheck();
void buzzerConstantTone(int freq);
```
After:
```cpp
void securityCheck();
```

Before:
```cpp
void ongoingAlarms();
byte activeAlarm();
void reestartOngoingAlarms();
int alarmPendingToDisplay();
int alarmPendingToClear();
void clearDisplayedAlarm(byte alarm);
void clearAlarmPendingToClear(byte alarm);
char *alarmIDtoString(byte alarmID);
```
(note: `bool ongoingAlarms();` keeps its existing `bool` return type — shown as `void` above only to anchor the surrounding lines; do not change its signature)
After:
```cpp
bool ongoingAlarms();
byte activeAlarm();
void reestartOngoingAlarms();
char *alarmIDtoString(byte alarmID);
```

Before:
```cpp
void wifiInit(void);
void wifiDisable();
```
After:
```cpp
void wifiInit(void);
```

- [ ] **Step 5: Remove the orphaned on-board-UI draw-state globals**

`main.cpp` — before:
```cpp
int separatorTopYPos, separatorMidYPos, separatorBotYPos;
int ypos;
bool print_text;
int initialSensorPosition = separatorPosition - letter_width;
bool pos_text[8];
```
After:
```cpp
int separatorTopYPos, separatorMidYPos, separatorBotYPos;
```

`initHardware.cpp` — before:
```cpp
extern int humidityX;
extern int humidityY;
extern int temperatureX;
extern int temperatureY;
extern int ypos;
extern bool print_text;
extern int initialSensorPosition;
extern bool pos_text[8];
```
After:
```cpp
extern int humidityX;
extern int humidityY;
extern int temperatureX;
extern int temperatureY;
```

`ISR.cpp` — apply the identical before/after as `initHardware.cpp` above (same four `extern` lines, same context).

`security.cpp` — apply the identical before/after as `initHardware.cpp` above (same four `extern` lines, same context).

- [ ] **Step 6: Compile both real environments**

Run: `pio run -e IncuNest_V16 -e IncuNest_V17`
Expected: both build successfully. If the linker/compiler flags `displayAlarm`/`clearedAlarm` as unused after step 1, stop and report back — do not delete them as part of this task (out of the verified scope for this plan).

- [ ] **Step 7: Commit**

```bash
git add include/main.h src/system/security.cpp src/system/Buzzer.cpp src/tasks/Wifi_OTA.cpp src/main.cpp src/system/initHardware.cpp src/system/ISR.cpp
git commit -m "refactor(motherboard): remove functions and globals with zero call sites (phase 1 of legacy cleanup)"
```

---

### Task 3: Phase 2a — collapse `board.h` pin/config chain to `HW_NUM>=16`

**Files:**
- Modify: `motherBoard/include/config/board.h:36-42`, `:44-56`, `:59-313`

**Interfaces:** Produces: `board.h` still defines every pin/config macro Task 4-6 and the rest of the codebase already consume for `HW_NUM=16/17` (`PWR_EN`, `ON_OFF_SWITCH`, `BUZZER`, `GSM_UART_TX_PIN`, `GSM_UART_RX_PIN`, `UART_MB_TX_PIN`, `UART_MB_RX_PIN`, `ACTUATORS_EN`, `HEATER`, `FAN`, `PHOTOTHERAPY`, `FAN_CTL`, `FAN_SPEED_FEEDBACK`, `USB_EN`, `USB_FAULT`, `BABY_NTC_PIN`, `BABY_TEMP_EN`, `ADS1110_I2C_ADDRESS`, `USB_D_MINUS`, `USB_D_PLUS`, `I2C2_SCL`, `I2C2_SDA`, `I2C_SDA`, `I2C_SCL`, `AFE_MISO`, `AFE_MOSI`, `AFE_SCK`, `AFE_ADC_READY`, `AFE44XX_CS`, `AFE_LED_ALM`, `FAKE_PIN`, `SCREENBACKLIGHT`, `AFE44XX_PWDN_PIN`, `GPRS_PWRKEY`, `TFT_DC`, `ENC_SWITCH`, `ENC_A`, `ENC_B`, `TFT_CS`) — unchanged values, just no longer conditional.

- [ ] **Step 1: Delete the dead `ANALOG_TO_AMP_FACTOR`/`CURRENT_MEASURES_AMOUNT` branch (lines 44-47)**

Before:
```cpp
#if (HW_NUM <= 8)
#define ANALOG_TO_AMP_FACTOR 0.2
#define CURRENT_MEASURES_AMOUNT 20
#endif
#if (HW_NUM <= 6)
```
After:
```cpp
#if (HW_NUM <= 6)
```

- [ ] **Step 2: Collapse the pin-definition chain (lines 59-313) to the `HW_NUM>=16` body only**

Before: the full chain — `#if (HW_NUM >= 16)` (body: lines 60-118) `#elif (HW_NUM == 15)` ... `#elif (HW_NUM == 14)` ... `#elif (HW_NUM >= 13)` ... `#elif (HW_NUM >= 9)` ... `#elif (HW_NUM == 8)` ... `#else` ... `#endif` (see design doc for the full dead branches' content — every branch below `HW_NUM>=16` only ever matched hardware revisions that no longer exist).

After: remove the `#if (HW_NUM >= 16)` / `#elif` / `#else` / `#endif` wrapper entirely and keep only its body (lines 61-117, i.e. everything between `#if (HW_NUM >= 16)` and the first `#elif`), unconditional — since `include/main.h`'s `#error` guard combined with `platformio.ini` guarantees `HW_NUM` is always 16 or 17, there is no other branch to preserve. The `#define GPIO_EXP_BASE 100` line (58) stays as-is immediately before it, unconditional (already is).

Concretely, replace:
```cpp
#define GPIO_EXP_BASE 100 // To differentiate with ESP32 GPIO
#if (HW_NUM >= 16)
// Power / control
#define PWR_EN 2
...
#undef TFT_CS
#define TFT_CS FAKE_PIN
#elif (HW_NUM == 15)
...
#endif
```
with:
```cpp
#define GPIO_EXP_BASE 100 // To differentiate with ESP32 GPIO
// Power / control
#define PWR_EN 2
...
#undef TFT_CS
#define TFT_CS FAKE_PIN
```
(the `...` is the exact unchanged body currently between `#if (HW_NUM >= 16)` and `#elif (HW_NUM == 15)` in `board.h:61-117` — copy it verbatim, only the `#if`/`#elif...#endif` wrapper is removed)

- [ ] **Step 3: Delete the now-dead trailing `#else` branch feeding `SYSTEM_SHUNT`/`HEATER_SHUNT`/etc. (lines ~374-380)**

Before:
```cpp
#if (HW_NUM >= 17)
#define HUMIDIFIER_SHUNT 100 // miliohms
...
#elif (HW_NUM >= 16)
#define SYSTEM_SHUNT 3        // miliohms
#define FAN_SHUNT 3           // miliohms
#define PHOTOTHERAPY_SHUNT 15 // miliohms
#define BATTERY_SHUNT 27000   // miliohms
#define USB_SHUNT 3           // miliohms
#define HEATER_SHUNT 3        // miliohms
#else
#define SYSTEM_SHUNT 2        // miliohms
#define BATTERY_SHUNT 27000   // miliohms
#define FAN_SHUNT 100         // miliohms
#define PHOTOTHERAPY_SHUNT 82 // miliohms
#define HEATER_SHUNT 2        // miliohms
#define USB_SHUNT 3           // miliohms
#endif
```
After (drop only the `#else` branch — both `>=17` and `>=16` branches stay, they're both live for the two real environments):
```cpp
#if (HW_NUM >= 17)
#define HUMIDIFIER_SHUNT 100 // miliohms
...
#elif (HW_NUM >= 16)
#define SYSTEM_SHUNT 3        // miliohms
#define FAN_SHUNT 3           // miliohms
#define PHOTOTHERAPY_SHUNT 15 // miliohms
#define BATTERY_SHUNT 27000   // miliohms
#define USB_SHUNT 3           // miliohms
#define HEATER_SHUNT 3        // miliohms
#endif
```
(there are two separate `#if (HW_NUM>=17)/#elif(HW_NUM>=16)/#else/#endif` chains near line 343 and 355 in the current file — apply this same "drop the trailing `#else`, keep both real branches" pattern to both; read the file first to get their exact current line numbers before editing, since Task 1/2 edits shift line numbers.)

- [ ] **Step 4: Compile both real environments**

Run: `pio run -e IncuNest_V16 -e IncuNest_V17`
Expected: both build successfully — this is the highest-risk task in the plan (pin definitions), inspect the build output for any "undefined" macro errors carefully.

- [ ] **Step 5: Commit**

```bash
git add include/config/board.h
git commit -m "refactor(motherboard): collapse board.h pin/config chain to HW_NUM>=16 (phase 2a of legacy cleanup)"
```

---

### Task 4: Phase 2b — drop the USB-CDC/VCP HMI comm path in `CommTask.cpp`

**Files:**
- Modify: `motherBoard/src/tasks/CommTask.cpp:16-22`, `:44-53`, `:65-130`

**Interfaces:** No change to any function this file exposes to other modules — `hmiSerial` becomes unconditionally `Serial1` (already true for both real environments today).

- [ ] **Step 1: Remove the dead USB-host includes (lines 16-22)**

Before:
```cpp
#include <time.h>

#if (HW_NUM < 16)
#include "usb/cdc_acm_host.h"
#include "usb/usb_host.h"
#include "usb/vcp.hpp"
#include "usb/vcp_ch34x.hpp"
using namespace esp_usb;
#endif

static const char *TAG __attribute__((unused)) = "COMM_HOST";
```
After:
```cpp
#include <time.h>

static const char *TAG __attribute__((unused)) = "COMM_HOST";
```

- [ ] **Step 2: Collapse the `hmiSerial`/`vcp` conditional to the `HW_NUM>=16` branch (lines 44-53)**

Before:
```cpp
#if (HW_NUM >= 16)
static HardwareSerial &hmiSerial = Serial1;
#else
static std::unique_ptr<CdcAcmDevice> vcp;
static SemaphoreHandle_t device_disconnected_sem;
static SemaphoreHandle_t vcp_mux;
// Bloquea nuevos TX en cuanto se detecta desconexión para que el close() no
// curse con URBs en vuelo (evita assert en hcd_urb_dequeue del USB host).
static volatile bool vcp_disconnecting = false;
#endif
```
After:
```cpp
static HardwareSerial &hmiSerial = Serial1;
```

- [ ] **Step 3: Delete the dead USB-VCP helper functions (lines 65-130)**

Before:
```cpp
#if (HW_NUM < 16)
static void reset_vcp() {
  ...
}

static bool handle_rx(const uint8_t *data, size_t len, void *arg) {
  ...
}

static void handle_event(const cdc_acm_host_dev_event_data_t *event,
                         void *user_ctx) {
  ...
}

static void usb_lib_task(void *arg) {
  ...
}
#endif // HW_NUM < 16

// ======================================================
//  PHOTOTHERAPY TIMER
// ======================================================
```
After:
```cpp
// ======================================================
//  PHOTOTHERAPY TIMER
// ======================================================
```
(the exact bodies of `reset_vcp`, `handle_rx`, `handle_event`, `usb_lib_task` are the full functions currently in `CommTask.cpp:66-129` — delete the whole `#if (HW_NUM < 16)` … `#endif` block including its contents)

- [ ] **Step 4: Grep the rest of the file for any remaining reference to `vcp`, `device_disconnected_sem`, `vcp_mux`, `vcp_disconnecting`, `reset_vcp`, `handle_rx`, `handle_event`, `usb_lib_task`, or another `HW_NUM < 16` / `HW_NUM<16` guard**

Run: `grep -n "vcp\|device_disconnected_sem\|handle_rx\|handle_event\|usb_lib_task\|HW_NUM < 16\|HW_NUM<16" src/tasks/CommTask.cpp` (from `motherBoard/`)
Expected: no matches remain (everything referencing the removed USB-VCP path was itself only reachable from inside the same dead branch). If a match surfaces outside what Steps 1-3 removed, stop and report back before deleting further — do not guess.

- [ ] **Step 5: Compile both real environments**

Run: `pio run -e IncuNest_V16 -e IncuNest_V17`
Expected: both build successfully.

- [ ] **Step 6: Commit**

```bash
git add src/tasks/CommTask.cpp
git commit -m "refactor(motherboard): drop dead USB-VCP HMI comm path from CommTask.cpp (phase 2b of legacy cleanup)"
```

---

### Task 5: Phase 2c — trim the dead branch in `sensors_module.cpp`

**Files:**
- Modify: `motherBoard/src/modules/sensors/sensors_module.cpp:195-202`

**Interfaces:** `measureMeanConsumption(bool sensor, int shunt)` keeps its existing signature and its `else`-branch behavior unchanged — only the always-false `#if` branch is removed.

- [ ] **Step 1: Remove the dead `HW_NUM>=6 && HW_NUM<=8` branch**

Before:
```cpp
double measureMeanConsumption(bool sensor, int shunt) {
#if (HW_NUM >= 6 && HW_NUM <= 8)
  for (int i = 0; i < CURRENT_MEASURES_AMOUNT; i++) {
    in3.system_current = filter_2(analogReadMilliVolts(SYSTEM_CURRENT_SENSOR) *
                                  ANALOG_TO_AMP_FACTOR);
  }
  return (in3.system_current);
#else
  if (digitalCurrentSensorPresent[sensor]) {
```
After:
```cpp
double measureMeanConsumption(bool sensor, int shunt) {
  if (digitalCurrentSensorPresent[sensor]) {
```

(read the rest of the function body first to find and remove the matching `#endif` — it closes the `#else` opened above, immediately before the function's closing brace; do not remove anything after the `#else` body itself, only the `#if...#else` markers)

- [ ] **Step 2: Compile both real environments**

Run: `pio run -e IncuNest_V16 -e IncuNest_V17`
Expected: both build successfully.

- [ ] **Step 3: Commit**

```bash
git add src/modules/sensors/sensors_module.cpp
git commit -m "refactor(motherboard): drop dead HW_NUM 6-8 branch from measureMeanConsumption (phase 2c of legacy cleanup)"
```

---

### Task 6: Phase 2d — remove dead-for-real-hardware init functions in `initHardware.cpp`

**Files:**
- Modify: `motherBoard/src/system/initHardware.cpp` (`initGPIO()`, `initActuators()`, `initTFT()`, `testDisplay()`, and the dead `drawHardwareErrorMessage()` call site)
- Modify: `motherBoard/include/main.h:427-428`, `:498-500`

**Interfaces:** Produces: `initHardware()` no longer calls `initTFT()`/`testDisplay()`/`drawHardwareErrorMessage()`. Confirm (grep) no other file calls any of these three before deleting their prototypes — `initTFT()` and `testDisplay()` are only called from within `initHardware()` itself per the current read of this file.

- [ ] **Step 1: `initGPIO()` — remove the dead `HW_NUM==6`/`HW_NUM==8` fragment**

Before:
```cpp
void initGPIO() {
  initI2C();
  logI("[HW] -> Initializing GPIOs");
#if (HW_NUM == 6)
  TCA.begin();
  for (int pin = 0; pin < 16; pin++) {
    TCA.setPolarity(pin, false);
  }
  pinMode(UNUSED_GPIO_EXP0, OUTPUT);
  pinMode(UNUSED_GPIO_EXP1, OUTPUT);
  pinMode(UNUSED_GPIO_EXP2, OUTPUT);
  pinMode(UNUSED_GPIO_EXP3, OUTPUT);
  digitalWrite(UNUSED_GPIO_EXP0, HIGH);
  digitalWrite(UNUSED_GPIO_EXP1, HIGH);
  digitalWrite(UNUSED_GPIO_EXP2, HIGH);
  digitalWrite(UNUSED_GPIO_EXP3, HIGH);
  pinMode(GPRS_EN, OUTPUT);
  digitalWrite(GPRS_EN, HIGH);
  pinMode(HUMIDIFIER_CTL, OUTPUT);
  digitalWrite(HUMIDIFIER_CTL, LOW);
  digitalWrite(TFT_CS_EXP, LOW);
#elif (HW_NUM == 8)
  pinMode(HUMIDIFIER_PWM, OUTPUT);
#endif
#if (HW_NUM >= 9)
  pinMode(FAN_SPEED_FEEDBACK, INPUT_PULLUP);
#endif
```
After:
```cpp
void initGPIO() {
  initI2C();
  logI("[HW] -> Initializing GPIOs");
  pinMode(FAN_SPEED_FEEDBACK, INPUT_PULLUP);
```
(the `#if (HW_NUM >= 9)` guard is dropped too since it is always true for `HW_NUM` 16/17)

- [ ] **Step 2: same function — remove the dead `HUMIDIFIER_PWM_CHANNEL` PWM setup (`HW_NUM==8`, earlier in the same function)**

Before:
```cpp
#if (HW_NUM == 8)
  ledcSetup(HUMIDIFIER_PWM_CHANNEL, HUMIDIFIER_PWM_FREQUENCY,
            DEFAULT_PWM_RESOLUTION);
  ledcAttachPin(HUMIDIFIER_CTL, HUMIDIFIER_PWM_CHANNEL);
  ledcWrite(HUMIDIFIER_CTL, 0);
#endif
  logI("[HW] -> PWM GPIOs initialized");
```
After:
```cpp
  logI("[HW] -> PWM GPIOs initialized");
```

- [ ] **Step 3: `initActuators()` — collapse the humidifier-init branch to `HW_NUM>=16`**

Before:
```cpp
bool initActuators() {
#if (HW_NUM <= 6)
  in3_hum.begin(HUMIDIFIER_BINARY, HUMIDIFIER_CTL);
#elif (HW_NUM <= 8)
  in3_hum.begin(HUMIDIFIER_PWM, HUMIDIFIER_CTL);
#elif (HW_NUM >= 16)
  in3_hum.begin(HUMIDIFIER_BINARY, USB_EN);
#else
  in3_hum.begin();
#endif
```
After:
```cpp
bool initActuators() {
  in3_hum.begin(HUMIDIFIER_BINARY, USB_EN);
```

- [ ] **Step 4: Delete `initTFT()` entirely (its body is already a no-op for `HW_NUM>=15`) and its call site**

Before (function, currently `initHardware.cpp:603-617`):
```cpp
void initTFT() {

#if (HW_NUM < 15)
  tft.init();
#if (HW_NUM == 6)
  digitalWrite(TFT_CS_EXP, HIGH);
  vTaskDelay(pdMS_TO_TICKS(5));
  digitalWrite(TFT_CS_EXP, LOW);
#endif
  tft.setRotation(DISPLAY_DEFAULT_ROTATION);
  tft.fillScreen(TFT_BLACK);
  tft_width = tft.width();
  tft_height = tft.height();
#endif
}
```
After: delete the whole function.

Call site — before:
```cpp
  initTFT();
  initInterrupts();
```
After:
```cpp
  initInterrupts();
```

- [ ] **Step 5: Delete `testDisplay()` entirely (its body is already a no-op for `HW_NUM>=15`) and its call site**

Before (function, currently `initHardware.cpp:619-671`): the whole function, whose body from `#if (HW_NUM < 15)` (line 620) to the matching `#endif` (line 670) compiles to nothing for `HW_NUM` 16/17 — delete the entire function including its signature and closing brace.

Call site — before:
```cpp
  if (!in3.restoreState) {
    testStandByCurrent();
    testDisplay();
    testBuzzer();
  }
```
After:
```cpp
  if (!in3.restoreState) {
    testStandByCurrent();
    testBuzzer();
  }
```

- [ ] **Step 6: Remove the dead `drawHardwareErrorMessage()` call block (`HW_NUM<15`)**

Before:
```cpp
  if (printOutputTest || in3.HW_critical_error || in3.calibrationError) {
    logE("[HW] -> PRINTING ERROR TO USER");
#if (HW_NUM < 15)
    drawHardwareErrorMessage(HW_error, in3.HW_critical_error,
                             in3.calibrationError);
    while (GPIORead(ENC_SWITCH))
      ;
#endif
  }
```
After:
```cpp
  if (printOutputTest || in3.HW_critical_error || in3.calibrationError) {
    logE("[HW] -> PRINTING ERROR TO USER");
  }
```

- [ ] **Step 7: `main.h` — remove the now-fully-dangling prototypes**

Before:
```cpp
void initHardware(bool printOutputTest);
void UI_mainMenu();
void userInterfaceHandler(int UI_page);
void updateData();
```
After:
```cpp
void initHardware(bool printOutputTest);
void updateData();
```

Before:
```cpp
void initGPIO();
void initEEPROM();
void drawHardwareErrorMessage(long error, bool criticalError,
                              bool calibrationError);
void initAlarms();
```
After:
```cpp
void initGPIO();
void initEEPROM();
void initAlarms();
```

- [ ] **Step 8: Grep for any remaining reference to the four removed symbols**

Run: `grep -rn "initTFT\|testDisplay\|drawHardwareErrorMessage\|UI_mainMenu\|userInterfaceHandler" src/ include/` (from `motherBoard/`)
Expected: no matches. If any surface, stop and report back before proceeding.

- [ ] **Step 9: Compile both real environments**

Run: `pio run -e IncuNest_V16 -e IncuNest_V17`
Expected: both build successfully.

- [ ] **Step 10: Commit**

```bash
git add src/system/initHardware.cpp include/main.h
git commit -m "refactor(motherboard): remove init functions dead-for-real-hardware and their dangling prototypes (phase 2d of legacy cleanup)"
```

---

### Task 7: Phase 2e — sweep for remaining `HW_NUM<16`-only branches

This task exists because `initHardware.cpp` is 1300+ lines and this plan's tasks 3-6 verified every branch found during design but do not claim to be a complete enumeration of the file. Do not skip this task.

**Files:** Potentially any of `motherBoard/src/system/initHardware.cpp`, `motherBoard/include/config/board.h`, `motherBoard/src/tasks/CommTask.cpp`, `motherBoard/src/modules/sensors/sensors_module.cpp`, `motherBoard/include/main.h`.

**Interfaces:** None known in advance — depends on what the sweep finds.

- [ ] **Step 1: Grep every remaining `HW_NUM` conditional in the five files**

Run (from `motherBoard/`):
```bash
grep -n "HW_NUM" include/config/board.h include/main.h src/system/initHardware.cpp src/tasks/CommTask.cpp src/modules/sensors/sensors_module.cpp
```

- [ ] **Step 2: For each match found, classify it**

For every `#if`/`#elif`/`#else` conditioned on `HW_NUM`, work out which branch is taken when `HW_NUM=16` and which when `HW_NUM=17` (both are real). If a branch is never taken by either value, it is dead — delete it, following the exact same pattern as Tasks 3-6 (keep only reachable branches; drop the conditional wrapper entirely if only one branch survives). If every remaining match is already reachable by 16 or 17 (i.e., genuinely live, revision-differentiating code — like the `SYSTEM_SHUNT`/`HUMIDIFIER_SHUNT` `>=17` vs `>=16` pairs already left alone in Task 3), leave it untouched.

- [ ] **Step 3: Compile both real environments after each file you touch**

Run: `pio run -e IncuNest_V16 -e IncuNest_V17`

- [ ] **Step 4: Commit (only if Step 2 found anything to remove)**

```bash
git add <files touched>
git commit -m "refactor(motherboard): remove remaining dead HW_NUM<16 branches found in sweep (phase 2e of legacy cleanup)"
```

If Step 1's grep turns up nothing beyond what Tasks 3-6 already handled, skip the commit — there is nothing to commit — and note in your final report to the user that the sweep found no additional dead branches.

---

### Task 8: Phase 3 — remove now-orphaned config headers

**Files:**
- Delete: `motherBoard/include/config/ui_constants.h`
- Modify: `motherBoard/include/main.h:142` (its `#include`), `:123` (`UI_MENU_OLD`)
- Modify: `motherBoard/include/config/board.h` (remaining orphaned defines — see Step 2)
- Modify: `motherBoard/include/config/task_config.h` (dead task settings)
- Modify: `motherBoard/include/config/telemetry_keys.h:18`, `:87-88`

**Interfaces:** None — every symbol removed here was already confirmed to have zero references outside its own declaration (design doc + re-verification in Step 1 below).

- [ ] **Step 1: Re-verify zero usage before deleting anything**

Phase 2 may have created new orphans beyond the design doc's list (e.g. `BACKLIGHT_CONTROL`/`DIRECT_BACKLIGHT_CONTROL`/`INVERTED_BACKLIGHT_CONTROL`/`SCREEN_CONSUMPTION_MIN`/`SCREEN_CONSUMPTION_MAX`, which were only ever consumed inside the now-deleted `testDisplay()`). Run (from `motherBoard/`):
```bash
grep -rn "BACKLIGHT_CONTROL\|DIRECT_BACKLIGHT_CONTROL\|INVERTED_BACKLIGHT_CONTROL\|SCREEN_BRIGHTNESS_FACTOR\|BACKLIGHT_POWER_SAFE\|BACKLIGHT_POWER_DEFAULT\|DISPLAY_SPI_CLK\|HUMIDIFIER_INTERFACE\|AFE_LED_ALM\|NTC_QTY\|SDCard\|BL_NORMAL\|BL_POWERSAVE\|maxDACvalue\|UI_MENU_OLD" src/ include/
```
For each symbol, confirm every remaining match is inside `include/config/board.h`/`include/main.h` itself (the declaration), not a real consumer. If a symbol turns up a real consumer you didn't expect, drop it from this task's deletion list and report back — do not delete something still referenced.

- [ ] **Step 2: Delete `ui_constants.h` and its `#include`**

Delete the file `motherBoard/include/config/ui_constants.h` entirely (~109 lines: `valuePosition`, `separatorPosition`, `unitPosition`, `textFontSize`, `width_select`, `TFT_HEIGHT_HEADING`, `TFT_SEPARATOR_HEIGHT`, `width_back`, `side_gap`, `letter_height`, `letter_width`, `logo`, `arrow_height`, `arrow_tail`, `headint_text_height`, `initialSensorsValue`, `barThickness`, `blinkTimeON`, `blinkTimeOFF`, `time_back_draw`, `time_back_wait`, `BACKLIGHT_NO_INTERACTION_TIME`, the 9 colour literals (`BLACK`/`BLUE`/`RED`/`GREEN`/`CYAN`/`MAGENTA`/`YELLOW`/`WHITE`/`ORANGE`), the `COLOUR_*` aliases, `introBackColor`/`introTextColor`/`transitionEffect`, and the 5 typedef enums `UI_PAGES`/`UI_EVENTS_ID`/`UI_EVENTS_ID_POS`/`MAIN_MENU_UI`/`SETTINGS_MENU_UI`/`CALIBRATION_MENU_UI`).

`main.h` — before:
```cpp
#include "ui_constants.h"

// Mutex for protecting the shared variable
```
After:
```cpp
// Mutex for protecting the shared variable
```

- [ ] **Step 3: `main.h` — remove `UI_MENU_OLD`**

Before:
```cpp
#define HOLD_PRESS_TO_GO_TO_SETTINGS 0

#define UI_MENU_OLD false

#define BROWN_OUT_BATTERY_MODE 0
```
After:
```cpp
#define HOLD_PRESS_TO_GO_TO_SETTINGS 0

#define BROWN_OUT_BATTERY_MODE 0
```

- [ ] **Step 4: `board.h` — remove the remaining orphaned defines**

Delete `DISPLAY_SPI_CLK` (the 3-way `#if`/`#elif`/`#elif`/`#endif`, all branches resolve to `SPI_CLOCK_DIV16` and it has zero consumers):
Before:
```cpp
#if (HW_NUM <= 8)
#define DISPLAY_SPI_CLK SPI_CLOCK_DIV16
#elif (HW_NUM == 9)
#define DISPLAY_SPI_CLK SPI_CLOCK_DIV16
#elif (HW_NUM >= 10)
#define DISPLAY_SPI_CLK SPI_CLOCK_DIV16
#endif

#if (HW_NUM <= 6)
```
After:
```cpp
#if (HW_NUM <= 6)
```
(this leaves the `HUMIDIFIER_INTERFACE` chain, which is removed next — the surrounding `#if (HW_NUM <= 6)` in the "Before" above belongs to that chain)

Delete `HUMIDIFIER_INTERFACE` (whole 4-branch chain, zero consumers regardless of value):
Before:
```cpp
#if (HW_NUM <= 6)
#define HUMIDIFIER_INTERFACE HUMIDIFIER_BINARY
#elif (HW_NUM <= 8)
#define HUMIDIFIER_INTERFACE HUMIDIFIER_PWM
#elif (HW_NUM >= 16)
#define HUMIDIFIER_INTERFACE HUMIDIFIER_BINARY // USB_EN GPIO ON/OFF
#else
#define HUMIDIFIER_INTERFACE HUMIDIFIER_I2C
#endif

#define GPIO_EXP_BASE 100 // To differentiate with ESP32 GPIO
```
After:
```cpp
#define GPIO_EXP_BASE 100 // To differentiate with ESP32 GPIO
```

Delete `AFE_LED_ALM` (single line, `#define AFE_LED_ALM 7`, inside the `HW_NUM>=16` block from Task 3 — leave every other line in that block untouched, remove only this one).

Delete `NTC_QTY` (single line, `#define NTC_QTY 1 // number of NTC`, next to the other sensor-numbering defines — leave `SKIN_SENSOR`/`ROOM_DIGITAL_TEMP_SENSOR`/etc. untouched).

Delete `SDCard` (single line, `#define SDCard false`).

Delete `BL_NORMAL`/`BL_POWERSAVE` (two lines):
Before:
```cpp
#define BL_NORMAL 0
#define BL_POWERSAVE 1

#define HEATER_MAX_PWM PWM_MAX_VALUE
```
After:
```cpp
#define HEATER_MAX_PWM PWM_MAX_VALUE
```

Delete `maxDACvalue` (single line, next to `maxADCvalue` which stays — `maxADCvalue` is live, only remove `maxDACvalue`):
Before:
```cpp
#define maxADCvalue 4095
#define maxDACvalue 4095
// #define PWM_MAX_VALUE maxADCvalue
```
After:
```cpp
#define maxADCvalue 4095
// #define PWM_MAX_VALUE maxADCvalue
```

Delete `DIRECT_BACKLIGHT_CONTROL`/`INVERTED_BACKLIGHT_CONTROL`, `SCREEN_BRIGHTNESS_FACTOR`/`BACKLIGHT_POWER_SAFE_PERCENTAGE`/`BACKLIGHT_CONTROL` (both branches, now unused since `testDisplay()` was deleted in Task 6), and `BACKLIGHT_POWER_SAFE` — but **keep `BACKLIGHT_POWER_DEFAULT`**, it is still used (`initHardware.cpp:1286`, `ledcWrite(SCREENBACKLIGHT_PWM_CHANNEL, BACKLIGHT_POWER_DEFAULT);`), and it depends on `SCREEN_BRIGHTNESS_FACTOR`, so `SCREEN_BRIGHTNESS_FACTOR` must stay too (only delete `BACKLIGHT_POWER_SAFE_PERCENTAGE`, `BACKLIGHT_CONTROL`, `DIRECT_BACKLIGHT_CONTROL`, `INVERTED_BACKLIGHT_CONTROL`, `BACKLIGHT_POWER_SAFE`):
Before:
```cpp
#define DIRECT_BACKLIGHT_CONTROL true
#define INVERTED_BACKLIGHT_CONTROL false

#define MIN_SYSTEM_VOLTAGE_TRIGGER 0
#define MAX_SYSTEM_VOLTAGE_TRIGGER 8

#if (HW_NUM <= 8 || (HW_NUM == 9 && HW_REVISION == 'A'))
#define SCREEN_BRIGHTNESS_FACTOR                                               \
  0.1 // Max brightness will be multiplied by this constant
#define BACKLIGHT_POWER_SAFE_PERCENTAGE 0.6
#define BACKLIGHT_CONTROL INVERTED_BACKLIGHT_CONTROL
#else
#define SCREEN_BRIGHTNESS_FACTOR                                               \
  0.7 // Max brightness will be multiplied by this constant
#define BACKLIGHT_POWER_SAFE_PERCENTAGE 0.3
#define BACKLIGHT_CONTROL DIRECT_BACKLIGHT_CONTROL
#endif

#define BACKLIGHT_POWER_SAFE PWM_MAX_VALUE *BACKLIGHT_POWER_SAFE_PERCENTAGE
#define BACKLIGHT_POWER_DEFAULT PWM_MAX_VALUE *SCREEN_BRIGHTNESS_FACTOR
```
After:
```cpp
#define MIN_SYSTEM_VOLTAGE_TRIGGER 0
#define MAX_SYSTEM_VOLTAGE_TRIGGER 8

#if (HW_NUM <= 8 || (HW_NUM == 9 && HW_REVISION == 'A'))
#define SCREEN_BRIGHTNESS_FACTOR                                               \
  0.1 // Max brightness will be multiplied by this constant
#else
#define SCREEN_BRIGHTNESS_FACTOR                                               \
  0.7 // Max brightness will be multiplied by this constant
#endif

#define BACKLIGHT_POWER_DEFAULT PWM_MAX_VALUE *SCREEN_BRIGHTNESS_FACTOR
```

- [ ] **Step 5: `task_config.h` — remove the dead task-config defines**

Before:
```cpp
#define POWER_MANAGEMENT_TASK_PRIORITY 1
#define TIME_TRACK_TASK_PRIORITY 2
#define BACKLIGHT_TASK_PRIORITY 3
#define OTA_TASK_PRIORITY 4
#define GPRS_TAST_PRIORITY 5
#define BUZZER_TASK_PRIORITY 6
#define UI_TASK_PRIORITY 7
#define COMMUNICATION_TASK_PRIORITY 7
```
After:
```cpp
#define POWER_MANAGEMENT_TASK_PRIORITY 1
#define TIME_TRACK_TASK_PRIORITY 2
#define OTA_TASK_PRIORITY 4
#define GPRS_TAST_PRIORITY 5
#define BUZZER_TASK_PRIORITY 6
#define COMMUNICATION_TASK_PRIORITY 7
```

Before:
```cpp
#define BUZZER_TASK_PERIOD_MS 10
#define UI_TASK_PERIOD_MS 10
#define SECURITY_TASK_PERIOD_MS 1
#define COMMUNICATION_TASK_PERIOD_MS 1
#define TIME_TRACK_TASK_PERIOD_MS 100
#define BACKLIGHT_TASK_PERIOD_MS 100
#define FAN_TASK_PERIOD_MS 10
#define LOOP_TASK_PERIOD_MS 1000
#define CALIBRATION_TASK_PERIOD_MS 100
#define GPRS_MONITOR_TASK_PERIOD 5000
```
After:
```cpp
#define BUZZER_TASK_PERIOD_MS 10
#define SECURITY_TASK_PERIOD_MS 1
#define COMMUNICATION_TASK_PERIOD_MS 1
#define TIME_TRACK_TASK_PERIOD_MS 100
#define FAN_TASK_PERIOD_MS 10
#define LOOP_TASK_PERIOD_MS 1000
#define GPRS_MONITOR_TASK_PERIOD 5000
```

- [ ] **Step 6: `telemetry_keys.h` — remove the 3 unused keys**

Before:
```cpp
#define HUMIDIFIER_CURR_KEY "Humidifier_current_test"
#define DISPLAY_CURR_TEST_KEY "Display_current_test"
#define BUZZER_CURR_TEST_KEY "Buzzer_current_test"
```
After:
```cpp
#define HUMIDIFIER_CURR_KEY "Humidifier_current_test"
#define BUZZER_CURR_TEST_KEY "Buzzer_current_test"
```

Before:
```cpp
#define CALIBRATION_RAW_TEMPERATURE_RANGE_SKIN_KEY "Cal_raw_range_skin_temp"
#define CALIBRATION_RAW_TEMPERATURE_LOW_SKIN_KEY "Cal_raw_low_skin_temp"
#define CALIBRATION_RAW_TEMPERATURE_RANGE_AIR_KEY "Cal_raw_range_air_temp"
#define CALIBRATION_RAW_TEMPERATURE_LOW_AIR_KEY "Cal_raw_low_air_temp"
#define CALIBRATION_REFERENCE_TEMPERATURE_RANGE_KEY "Cal_ref_range_temp"
```
After:
```cpp
#define CALIBRATION_RAW_TEMPERATURE_RANGE_SKIN_KEY "Cal_raw_range_skin_temp"
#define CALIBRATION_RAW_TEMPERATURE_LOW_SKIN_KEY "Cal_raw_low_skin_temp"
#define CALIBRATION_REFERENCE_TEMPERATURE_RANGE_KEY "Cal_ref_range_temp"
```

- [ ] **Step 7: Compile both real environments**

Run: `pio run -e IncuNest_V16 -e IncuNest_V17`
Expected: both build successfully — this is where a missed real reference (e.g. `letter_width` used inside `ui_constants.h`'s own now-deleted enums, or `BACKLIGHT_POWER_SAFE` used somewhere Step 1's grep missed) would surface as an "undeclared identifier" compile error. If it does, restore only the specific symbol that's still needed and re-run Step 1's grep to see what you missed — do not restore the whole file.

- [ ] **Step 8: Commit**

```bash
git add include/config/ui_constants.h include/main.h include/config/board.h include/config/task_config.h include/config/telemetry_keys.h
git commit -m "refactor(motherboard): remove orphaned config headers left after HW_NUM<16 removal (phase 3 of legacy cleanup)"
```

---

### Task 9: Final regression check and manual hardware verification

**Files:** None (verification only).

- [ ] **Step 1: Full clean compile of both real environments**

Run: `pio run -e IncuNest_V16 -e IncuNest_V17` (consider `pio run -e IncuNest_V16 -e IncuNest_V17 -t clean` first, then rebuild, to rule out stale incremental-build artifacts hiding a break)
Expected: both succeed, zero warnings introduced by this plan's changes.

- [ ] **Step 2: Native test regression**

Run: `pio test -e native`
Expected: unaffected — this plan never touches `modules/control/alarm_machine.cpp` or `pid_wrapper.cpp` — all tests should still pass exactly as before this plan.

- [ ] **Step 3: Manual hardware checklist (flash a real unit, `IncuNest_V17` if available, else `IncuNest_V16`)**

Document the result of each item as you go — this is the manual-verification record required by `.claude/rules/testing.md` since none of this is Unity-testable:
- [ ] Boots cleanly, no crash/reboot loop, no new error logged over serial at startup.
- [ ] `Display_HMI` connects and shows live sensor readings (temperature, humidity).
- [ ] Actuate from the HMI: toggle temperature control, toggle humidity control, toggle phototherapy — confirm each engages/disengages correctly.
- [ ] Trigger at least one alarm condition (e.g. unplug a sensor) and confirm it surfaces correctly on the HMI and clears when resolved.
- [ ] Confirm ThingsBoard (or the serial telemetry log if ThingsBoard isn't reachable in your test setup) still reports the expected telemetry keys with sane values — specifically confirm nothing under `HW_TEST_KEY`/the current-test keys silently went missing after the `telemetry_keys.h` trim in Task 8.
- [ ] Mute/acknowledge an alarm via the physical encoder button (per `.claude/rules/security.md`, this exact flow was flagged in `docs/known_issues.md` in a past incident — confirm it still works).

- [ ] **Step 4: Report back**

Summarize to the user: what compiled clean, what the native test run showed, and the outcome of each manual checklist item. Do not report this plan as complete until the manual checklist has been run on real hardware — flag clearly if you executed the code tasks but the hardware checklist is still pending, so the user knows this is not yet closed.

---

## Self-Review Notes

- **Spec coverage:** every design-doc phase (0/1/2/3) has a corresponding task (1/2/3-7/8); the "fuera de alcance" `EEPROM.cpp` item is called out explicitly in Global Constraints so no task touches it by accident; the design doc's verification section maps to Task 9.
- **Placeholder scan:** no TBD/TODO — every code-bearing step shows the literal current content and its replacement, or (Tasks 3 Step 3, 5, 6 Step 4, 7) an explicit grep-and-classify procedure with a concrete stop condition, used only where the exact line ranges could shift across prior tasks or the file is large enough that a static enumeration risks going stale.
- **Type/signature consistency:** no function signatures change anywhere in this plan — every task is either whole-symbol deletion or `#if` unwrapping around otherwise-unchanged code.
