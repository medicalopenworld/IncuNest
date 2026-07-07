# Fan Speed Feedback (RPM) Monitoring Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Detect at boot whether the installed fan reports RPM (tachometer feedback), persist that fact in NVS, validate and continuously monitor RPM against a 3000 rpm floor, and make a heater fault disable the fan too (requiring a manual restart) only on units without RPM feedback.

**Architecture:** All new logic hooks into existing, already-wired infrastructure: the interrupt-driven RPM pipeline (`fanEncoderISR` → `fanSpeedHandler` → `in3.fan_rpm`), the existing `FAN_ISSUE_ALARM` (already critical, already documented for exactly this purpose), and the existing boot actuator test (`actuatorsTest()`). No new tasks, no new communication protocol fields — everything rides on `alarmOnGoing[]`/`setAlarm()`/`resetAlarm()` and the existing telemetry bitmask.

**Tech Stack:** PlatformIO + Arduino framework, ESP32-S3, C++17. Preferences (NVS) for persistence.

## Global Constraints

- `FAN_MIN_RPM = 3000` (minimum acceptable RPM), `FAN_MIN_RPM_HYSTERESIS = 300` (recovery hysteresis), `FAN_SPINUP_GRACE_MS = 3000` (grace period after the fan is commanded on before RPM is evaluated) — from the approved design.
- All new RPM logic must be gated by both `#if defined(FAN_SPEED_FEEDBACK)` (compile-time: does this HW_NUM even route the pin) and `in3.fanHasSpeedFeedback` (runtime: did *this* unit's assembled fan actually report pulses).
- No forced `esp_restart()`. A heater fault on a no-feedback unit disables both actuators and relies on the alarm's UI-blocking behavior (already existing for `HEATER_ISSUE_ALARM`) — the user restarts manually.
- Per `Firmware/.claude/rules/testing.md`, `[env:native]` Unity tests only cover `modules/control/alarm_machine.cpp` and `pid_wrapper.cpp`. None of the files touched here are in that filter, so **no automated tests are added** — verification is `pio run -e IncuNest_V17` / `pio run -e IncuNest_V16` (compile) plus a manual hardware checklist in the final task. Do not fake coverage that doesn't exist.
- Per `Firmware/.claude/rules/commits.md`: Conventional Commits (`tipo(scope): descripción`), scope `motherboard`, single author (no `Co-Authored-By` trailer), one coherent change per commit.
- Working branch: `feat/fan-speed-feedback` (already created from `dev`).

---

## File Map

| File | Change |
|---|---|
| `include/config/preferences_keys.h` | New NVS key `KEY_FAN_RPM_FEEDBACK` |
| `include/main.h` | New `HW_ERROR_ID` value, new `IncuNest_parameters` fields, new function prototype |
| `src/system/EEPROM.cpp` | Load `in3.fanHasSpeedFeedback` from NVS |
| `include/config/board.h` | New `FAN_MIN_RPM` / `FAN_MIN_RPM_HYSTERESIS` defines |
| `src/system/initHardware.cpp` | Fix HW9 init guard; default-speed PWM + RPM detection/validation + heater/fan cascade in `actuatorsTest()` (both hardware paths) |
| `src/system/security.cpp` | New `ongoingFanCriticalAlarm()`, new `checkFanSpeed()`, wired into `securityCheck()` |
| `src/legacy/UI_actuatorsProgress.cpp` | `turnFans()` tracks `in3.fanCommandedOn` and uses `ongoingFanCriticalAlarm()` |
| `Firmware/docs/alarms.md` | Document the new RPM trigger and heater/fan cascade nuance |

---

### Task 1: Data plumbing — NVS key, struct fields, error code, prototype

**Files:**
- Modify: `include/config/preferences_keys.h:24`
- Modify: `include/main.h:158` (enum), `include/main.h:369` and `:394` (struct), `include/main.h:451` (prototype)
- Modify: `src/system/EEPROM.cpp:342`

**Interfaces:**
- Produces: `KEY_FAN_RPM_FEEDBACK` (NVS key, `NS_CFG` namespace), `FAN_RPM_MIN_ERROR` (new `HW_ERROR_ID` enum value), `in3.fanHasSpeedFeedback` (bool, default `false`), `in3.fanCommandedOn` (bool, default `false`), `bool ongoingFanCriticalAlarm();` (prototype only — implemented in Task 3).

- [ ] **Step 1: Add the NVS key**

In `include/config/preferences_keys.h`, right after line 24 (`constexpr char KEY_FAN_CTL_PWM[] = "fan_ctl_pwm";`), add:

```cpp
constexpr char KEY_FAN_RPM_FEEDBACK[] = "fan_rpm_fb";
```

- [ ] **Step 2: Add the new HW error code**

In `include/main.h`, the `HW_ERROR_ID` enum currently ends with:

```cpp
  DEFECTIVE_CURRENT_SENSOR,
  UNCALIBRATED_SENSOR,
} HW_ERROR_ID;
```

Change it to append the new value at the end (do not insert it in the middle — other bit positions are referenced by cloud telemetry and must not shift):

```cpp
  DEFECTIVE_CURRENT_SENSOR,
  UNCALIBRATED_SENSOR,
  FAN_RPM_MIN_ERROR,
} HW_ERROR_ID;
```

- [ ] **Step 3: Add the new struct fields**

In `include/main.h`, find:

```cpp
  int fanPwrSupplyPWM = FAN_PWR_SUPPLY_PWM;
  int fanCtlPWM = FAN_CTL_PWM_DEFAULT;
```

Change to:

```cpp
  int fanPwrSupplyPWM = FAN_PWR_SUPPLY_PWM;
  int fanCtlPWM = FAN_CTL_PWM_DEFAULT;
  bool fanHasSpeedFeedback = false;
```

Find:

```cpp
  float fan_rpm = false;
  bool fanEncoderUpdate = false;
  long fanEncoderPeriod[2] = {false, false};
```

Change to:

```cpp
  float fan_rpm = false;
  bool fanEncoderUpdate = false;
  long fanEncoderPeriod[2] = {false, false};
  bool fanCommandedOn = false;
```

- [ ] **Step 4: Add the new function prototype**

In `include/main.h`, find:

```cpp
bool ongoingCriticalAlarm();
bool ongoingCriticalWiringAlarm();
```

Change to:

```cpp
bool ongoingCriticalAlarm();
bool ongoingCriticalWiringAlarm();
bool ongoingFanCriticalAlarm();
```

- [ ] **Step 5: Load the new preference in EEPROM.cpp**

In `src/system/EEPROM.cpp`, find (inside `recapVariables()`):

```cpp
    in3.fanCtlPWM = p.getInt(KEY_FAN_CTL_PWM, FAN_CTL_PWM_DEFAULT);
    if (in3.fanCtlPWM <= 0 || in3.fanCtlPWM > 255)
      in3.fanCtlPWM = FAN_CTL_PWM_DEFAULT;
    p.end();
```

Change to:

```cpp
    in3.fanCtlPWM = p.getInt(KEY_FAN_CTL_PWM, FAN_CTL_PWM_DEFAULT);
    if (in3.fanCtlPWM <= 0 || in3.fanCtlPWM > 255)
      in3.fanCtlPWM = FAN_CTL_PWM_DEFAULT;
    in3.fanHasSpeedFeedback = p.getUChar(KEY_FAN_RPM_FEEDBACK, 0);
    p.end();
```

- [ ] **Step 6: Compile to verify (no behavior change yet)**

Run: `pio run -e IncuNest_V17`
Expected: `SUCCESS` — this task only adds unused-so-far fields/prototypes, no logic references them yet except the prototype (which has no definition yet, but nothing calls it yet either, so the linker won't complain).

- [ ] **Step 7: Commit**

```bash
git add include/config/preferences_keys.h include/main.h src/system/EEPROM.cpp
git commit -m "feat(motherboard): add fan RPM feedback NVS key, struct fields and error code"
```

---

### Task 2: RPM thresholds + HW9 init guard fix

**Files:**
- Modify: `include/config/board.h` (near `FAN_CTL_PWM_DEFAULT`)
- Modify: `src/system/initHardware.cpp:310` and `:345`

**Interfaces:**
- Consumes: nothing new.
- Produces: `FAN_MIN_RPM`, `FAN_MIN_RPM_HYSTERESIS` (defines used by Task 3 and Task 5/6).

- [ ] **Step 1: Add the RPM threshold defines**

In `include/config/board.h`, find:

```c
#define FAN_PWR_SUPPLY_PWM PWM_MAX_VALUE
#define FAN_CTL_PWM_DEFAULT 130
```

Change to:

```c
#define FAN_PWR_SUPPLY_PWM PWM_MAX_VALUE
#define FAN_CTL_PWM_DEFAULT 130
#define FAN_MIN_RPM 3000            // minimum acceptable fan RPM when speed feedback is present
#define FAN_MIN_RPM_HYSTERESIS 300  // rpm above FAN_MIN_RPM required to clear FAN_ISSUE_ALARM
```

- [ ] **Step 2: Fix the HW9 feedback init guard**

`board.h` defines `FAN_SPEED_FEEDBACK` starting at `HW_NUM >= 9`, but `initGPIO()`/`initInterrupts()` only wire the pin/interrupt from `HW_NUM >= 10`, so HW9 units would never actually see any pulses even though the pin is "defined". Fix both guards.

In `src/system/initHardware.cpp`, find (inside `initGPIO()`):

```cpp
#if (HW_NUM >= 10)
  pinMode(FAN_SPEED_FEEDBACK, INPUT_PULLUP);
#endif
```

Change to:

```cpp
#if (HW_NUM >= 9)
  pinMode(FAN_SPEED_FEEDBACK, INPUT_PULLUP);
#endif
```

Find (inside `initInterrupts()`):

```cpp
#if (HW_NUM >= 10)
  attachInterrupt(FAN_SPEED_FEEDBACK, fanEncoderISR, CHANGE);
#endif
```

Change to:

```cpp
#if (HW_NUM >= 9)
  attachInterrupt(FAN_SPEED_FEEDBACK, fanEncoderISR, CHANGE);
#endif
```

- [ ] **Step 3: Compile to verify**

Run: `pio run -e IncuNest_V17`
Expected: `SUCCESS` (HW_NUM=17 already satisfies both old and new guard, so this is a no-op for the currently built revisions — the fix only changes behavior for a hypothetical HW9 build).

- [ ] **Step 4: Commit**

```bash
git add include/config/board.h src/system/initHardware.cpp
git commit -m "fix(motherboard): align fan speed feedback init guard with board.h pin definition"
```

---

### Task 3: Fan-specific critical alarm predicate + runtime RPM monitor

**Files:**
- Modify: `src/system/security.cpp` (add two functions, wire one into `securityCheck()`)

**Interfaces:**
- Consumes: `in3.fanHasSpeedFeedback`, `in3.fanCommandedOn` (Task 1), `FAN_MIN_RPM`, `FAN_MIN_RPM_HYSTERESIS` (Task 2), `alarmOnGoing[]`, `setAlarm()`, `resetAlarm()` (existing).
- Produces: `bool ongoingFanCriticalAlarm()` (implements the Task 1 prototype), `void checkFanSpeed()` (internal to this file, called from `securityCheck()`).

- [ ] **Step 1: Add `ongoingFanCriticalAlarm()`**

In `src/system/security.cpp`, find:

```cpp
bool ongoingCriticalWiringAlarm()
{
  return (alarmOnGoing[HEATER_ISSUE_ALARM] || alarmOnGoing[FAN_ISSUE_ALARM] ||
          alarmOnGoing[POWER_SUPPLY_ALARM]);
}
```

Right after it, add:

```cpp
// Unlike ongoingCriticalWiringAlarm() (used for heater/humidifier gating),
// a heater fault only has to take the fan down with it when this unit has
// no independent way (RPM feedback) to verify the fan is still spinning.
bool ongoingFanCriticalAlarm()
{
  return (alarmOnGoing[FAN_ISSUE_ALARM] || alarmOnGoing[POWER_SUPPLY_ALARM] ||
          (alarmOnGoing[HEATER_ISSUE_ALARM] && !in3.fanHasSpeedFeedback));
}
```

- [ ] **Step 2: Add `checkFanSpeed()`**

In `src/system/security.cpp`, find:

```cpp
void powerSupplyCheck()
{
```

Right *before* it (so `checkFanSpeed()` is defined ahead of its use in `securityCheck()` further down the file), add:

```cpp
#if defined(FAN_SPEED_FEEDBACK)
#define FAN_SPINUP_GRACE_MS 3000 // time allowed for the fan to reach FAN_MIN_RPM after being commanded on

void checkFanSpeed()
{
  static bool wasFanCommandedOn = false;
  static long fanCommandedOnSince = 0;

  if (!in3.fanHasSpeedFeedback)
  {
    return; // this unit's fan has no tachometer signal — nothing to check
  }

  if (in3.fanCommandedOn && !wasFanCommandedOn)
  {
    fanCommandedOnSince = millis();
  }
  wasFanCommandedOn = in3.fanCommandedOn;

  if (!in3.fanCommandedOn)
  {
    return; // fan intentionally off — no RPM expected
  }
  if (millis() - fanCommandedOnSince < FAN_SPINUP_GRACE_MS)
  {
    return; // still spinning up
  }

  if (!alarmOnGoing[FAN_ISSUE_ALARM] && in3.fan_rpm < FAN_MIN_RPM)
  {
    in3.alarmToReport[FAN_ISSUE_ALARM] = true;
    setAlarm(FAN_ISSUE_ALARM);
  }
  else if (alarmOnGoing[FAN_ISSUE_ALARM] &&
           in3.fan_rpm >= FAN_MIN_RPM + FAN_MIN_RPM_HYSTERESIS)
  {
    in3.alarmToReport[FAN_ISSUE_ALARM] = false;
    resetAlarm(FAN_ISSUE_ALARM);
  }
}
#endif

void powerSupplyCheck()
{
```

- [ ] **Step 3: Wire `checkFanSpeed()` into `securityCheck()`**

Find:

```cpp
void securityCheck()
{
  if (in3.actuation)
  {
    checkThermalCutOuts();
  }
  checkAlarms();
  sensorHealthMonitor();
  powerSupplyCheck();
#if (HW_NUM >= 16)
  checkUsbFault();
#endif
}
```

Change to:

```cpp
void securityCheck()
{
  if (in3.actuation)
  {
    checkThermalCutOuts();
  }
  checkAlarms();
  sensorHealthMonitor();
  powerSupplyCheck();
#if defined(FAN_SPEED_FEEDBACK)
  checkFanSpeed();
#endif
#if (HW_NUM >= 16)
  checkUsbFault();
#endif
}
```

- [ ] **Step 4: Compile to verify**

Run: `pio run -e IncuNest_V17`
Expected: `SUCCESS`. `ongoingFanCriticalAlarm()` is defined but still unused outside this file until Task 4 — that's fine, it's a normal (non-static) function so the linker won't warn.

- [ ] **Step 5: Commit**

```bash
git add src/system/security.cpp
git commit -m "feat(motherboard): add fan-specific critical alarm predicate and RPM runtime monitor"
```

---

### Task 4: Decouple fan shutoff from heater alarm in `turnFans()`

**Files:**
- Modify: `src/legacy/UI_actuatorsProgress.cpp:145-165`

**Interfaces:**
- Consumes: `ongoingFanCriticalAlarm()` (Task 3), `in3.fanCommandedOn` (Task 1).
- Produces: `in3.fanCommandedOn` now kept up to date on every `turnFans()` call — this is what `checkFanSpeed()` (Task 3) reads.

- [ ] **Step 1: Update `turnFans()`**

In `src/legacy/UI_actuatorsProgress.cpp`, find:

```cpp
void turnFans(bool mode) {
  digitalWrite(ACTUATORS_EN, mode || in3.phototherapy);
#if (HW_NUM >= 8)
  // ledcWrite(HEATER_PWM_CHANNEL, mode * HEATER_MAX_PWM);
  ledcWrite(FAN_PWM_CHANNEL,
            (mode && !ongoingCriticalWiringAlarm()) * in3.fanPwrSupplyPWM);
  ledcWrite(FAN_CTL_PWM_CHANNEL, mode * in3.fanCtlPWM);
#else
  digitalWrite(FAN, in3.phototherapy || mode && !ongoingCriticalWiringAlarm());
#endif
}
```

Change to:

```cpp
void turnFans(bool mode) {
  in3.fanCommandedOn = mode || in3.phototherapy;
  digitalWrite(ACTUATORS_EN, mode || in3.phototherapy);
#if (HW_NUM >= 8)
  // ledcWrite(HEATER_PWM_CHANNEL, mode * HEATER_MAX_PWM);
  ledcWrite(FAN_PWM_CHANNEL,
            (mode && !ongoingFanCriticalAlarm()) * in3.fanPwrSupplyPWM);
  ledcWrite(FAN_CTL_PWM_CHANNEL, mode * in3.fanCtlPWM);
#else
  digitalWrite(FAN, in3.phototherapy || mode && !ongoingFanCriticalAlarm());
#endif
}
```

Note: `turnActuators()` (just above this function) keeps its own `ongoingCriticalWiringAlarm()` call for humidifier gating — do not touch that one.

- [ ] **Step 2: Compile to verify**

Run: `pio run -e IncuNest_V17`
Expected: `SUCCESS`.

- [ ] **Step 3: Commit**

```bash
git add src/legacy/UI_actuatorsProgress.cpp
git commit -m "feat(motherboard): decouple fan shutoff from heater-only alarms"
```

---

### Task 5: Boot RPM detection, validation and heater cascade — HW≥16 path

**Files:**
- Modify: `src/system/initHardware.cpp` (`actuatorsTest()`, HW≥16 branch, plus a small shared helper)

**Interfaces:**
- Consumes: `in3.fanHasSpeedFeedback`, `FAN_RPM_MIN_ERROR`, `FAN_MIN_RPM`, `KEY_FAN_RPM_FEEDBACK`, `addErrorToVar()`, `fanSpeedHandler()` (existing, declared via `main.h`/`legacy` sensors), `setAlarm()`.
- Produces: `static void disableFanOnUnverifiedHeaterFault()` (shared helper, reused by Task 6).

- [ ] **Step 1: Add the shared heater/fan cascade helper**

In `src/system/initHardware.cpp`, find:

```cpp
void addErrorToVar(long &errorVar, int error) { errorVar |= (1 << error); }
```

Right after it, add:

```cpp
// Without RPM feedback there is no independent way to confirm the fan is
// still spinning once the heater has failed, so the safest option is to
// disable both actuators. This does NOT force a restart: the operator must
// power-cycle the unit manually, same as the existing (unrecoverable within
// the session) HEATER_ISSUE_ALARM behavior today.
static void disableFanOnUnverifiedHeaterFault() {
  if (!in3.fanHasSpeedFeedback) {
    in3.alarmToReport[FAN_ISSUE_ALARM] = true;
    setAlarm(FAN_ISSUE_ALARM);
  }
}
```

- [ ] **Step 2: Run the fan at its default operating speed during the test**

Find:

```cpp
  // Turn on heater and phototherapy; delay to let first INA3221 samples arrive,
  // then turn on fan so its spin-up overlaps with heater/photo thermal ramp.
  ledcWrite(HEATER_PWM_CHANNEL,       PWM_MAX_VALUE);
  ledcWrite(PHOTOTHERAPY_PWM_CHANNEL, PHOTOTHERAPY_TEST_PWM);
  vTaskDelay(pdMS_TO_TICKS(INA3221_ONE_CYCLE_SETTLE_MS));
  ledcWrite(FAN_PWM_CHANNEL, PWM_MAX_VALUE);
  vTaskDelay(pdMS_TO_TICKS(220));
  logI("[HW] -> Heater + Phototherapy + Fan ON, measuring in parallel...");
```

Change to:

```cpp
  // Turn on heater and phototherapy; delay to let first INA3221 samples arrive,
  // then turn on fan so its spin-up overlaps with heater/photo thermal ramp.
  // The fan runs at its configured default operating speed (not full PWM) so
  // the current and RPM measured below reflect real operating conditions.
  ledcWrite(HEATER_PWM_CHANNEL,       PWM_MAX_VALUE);
  ledcWrite(PHOTOTHERAPY_PWM_CHANNEL, PHOTOTHERAPY_TEST_PWM);
  vTaskDelay(pdMS_TO_TICKS(INA3221_ONE_CYCLE_SETTLE_MS));
  ledcWrite(FAN_CTL_PWM_CHANNEL, in3.fanCtlPWM);
  ledcWrite(FAN_PWM_CHANNEL, in3.fanPwrSupplyPWM);
  vTaskDelay(pdMS_TO_TICKS(220));
  logI("[HW] -> Heater + Phototherapy + Fan ON, measuring in parallel...");
```

- [ ] **Step 3: Apply the cascade on heater failure**

Find:

```cpp
  // Heater checks (critical — abort on failure)
  if (res.heater < HEATER_CONSUMPTION_MIN) {
    addErrorToVar(HW_error, HEATER_CONSUMPTION_MIN_ERROR);
    logE("[HW] -> Fail -> Heater current too low");
    in3.alarmToReport[HEATER_ISSUE_ALARM] = true;
    setAlarm(HEATER_ISSUE_ALARM);
    // Fan measured in parallel — report it too if also bad
    if (res.fan < FAN_CONSUMPTION_MIN) {
      addErrorToVar(HW_error, FAN_CONSUMPTION_MIN_ERROR);
      logE("[HW] -> Fail -> Fan current also too low (wiring error)");
      in3.alarmToReport[FAN_ISSUE_ALARM] = true;
      setAlarm(FAN_ISSUE_ALARM);
    }
    digitalWrite(ACTUATORS_EN, LOW);
    return true;
  }
  if (res.heater > HEATER_CONSUMPTION_MAX) {
    addErrorToVar(HW_error, HEATER_CONSUMPTION_MAX_ERROR);
    logE("[HW] -> Fail -> Heater current too high");
    in3.alarmToReport[HEATER_ISSUE_ALARM] = true;
    setAlarm(HEATER_ISSUE_ALARM);
    digitalWrite(ACTUATORS_EN, LOW);
    return true;
  }
```

Change to:

```cpp
  // Heater checks (critical — abort on failure)
  if (res.heater < HEATER_CONSUMPTION_MIN) {
    addErrorToVar(HW_error, HEATER_CONSUMPTION_MIN_ERROR);
    logE("[HW] -> Fail -> Heater current too low");
    in3.alarmToReport[HEATER_ISSUE_ALARM] = true;
    setAlarm(HEATER_ISSUE_ALARM);
    disableFanOnUnverifiedHeaterFault();
    digitalWrite(ACTUATORS_EN, LOW);
    return true;
  }
  if (res.heater > HEATER_CONSUMPTION_MAX) {
    addErrorToVar(HW_error, HEATER_CONSUMPTION_MAX_ERROR);
    logE("[HW] -> Fail -> Heater current too high");
    in3.alarmToReport[HEATER_ISSUE_ALARM] = true;
    setAlarm(HEATER_ISSUE_ALARM);
    disableFanOnUnverifiedHeaterFault();
    digitalWrite(ACTUATORS_EN, LOW);
    return true;
  }
```

(This replaces the old ad-hoc "also report fan if its current is bad" diagnostic — the fan's own current check further down is unaffected and still runs independently.)

- [ ] **Step 4: Detect and validate RPM after the fan current check passes**

Find:

```cpp
  // Fan checks
  if (res.fan < FAN_CONSUMPTION_MIN) {
    addErrorToVar(HW_error, FAN_CONSUMPTION_MIN_ERROR);
    logE("[HW] -> Fail -> Fan current too low");
    in3.alarmToReport[FAN_ISSUE_ALARM] = true;
    setAlarm(FAN_ISSUE_ALARM);
    digitalWrite(ACTUATORS_EN, LOW);
    return true;
  }
  if (res.fan > FAN_CONSUMPTION_MAX &&
      res.fan > FAN_MAX_CURRENT_OVERRIDE * FAN_CONSUMPTION_MAX * 2) {
    addErrorToVar(HW_error, FAN_CONSUMPTION_MAX_ERROR);
    logE("[HW] -> Fail -> Fan current too high");
    in3.alarmToReport[FAN_ISSUE_ALARM] = true;
    setAlarm(FAN_ISSUE_ALARM);
    digitalWrite(ACTUATORS_EN, LOW);
    return true;
  }

  // Humidifier: GPIO fault-pin check only (HW>=16 has no INA3221 on USB channel)
```

Change to:

```cpp
  // Fan checks
  if (res.fan < FAN_CONSUMPTION_MIN) {
    addErrorToVar(HW_error, FAN_CONSUMPTION_MIN_ERROR);
    logE("[HW] -> Fail -> Fan current too low");
    in3.alarmToReport[FAN_ISSUE_ALARM] = true;
    setAlarm(FAN_ISSUE_ALARM);
    digitalWrite(ACTUATORS_EN, LOW);
    return true;
  }
  if (res.fan > FAN_CONSUMPTION_MAX &&
      res.fan > FAN_MAX_CURRENT_OVERRIDE * FAN_CONSUMPTION_MAX * 2) {
    addErrorToVar(HW_error, FAN_CONSUMPTION_MAX_ERROR);
    logE("[HW] -> Fail -> Fan current too high");
    in3.alarmToReport[FAN_ISSUE_ALARM] = true;
    setAlarm(FAN_ISSUE_ALARM);
    digitalWrite(ACTUATORS_EN, LOW);
    return true;
  }

  // Fan type detection: does this unit's assembled fan report RPM pulses?
  // Persisted so restoreState boots (which skip this whole test) still know.
#if defined(FAN_SPEED_FEEDBACK)
  fanSpeedHandler();
  in3.fanHasSpeedFeedback = (in3.fan_rpm > 0);
  { Preferences p; p.begin(NS_CFG, false);
    p.putUChar(KEY_FAN_RPM_FEEDBACK, in3.fanHasSpeedFeedback); p.end(); }
  logI("[HW] -> Fan type: " +
       String(in3.fanHasSpeedFeedback ? "RPM feedback" : "no RPM feedback") +
       " (" + String(in3.fan_rpm) + " rpm)");
  if (in3.fanHasSpeedFeedback && in3.fan_rpm < FAN_MIN_RPM) {
    addErrorToVar(HW_error, FAN_RPM_MIN_ERROR);
    logE("[HW] -> Fail -> Fan RPM too low (" + String(in3.fan_rpm) +
         " < " + String(FAN_MIN_RPM) + ")");
    in3.alarmToReport[FAN_ISSUE_ALARM] = true;
    setAlarm(FAN_ISSUE_ALARM);
  }
#else
  in3.fanHasSpeedFeedback = false;
#endif

  // Humidifier: GPIO fault-pin check only (HW>=16 has no INA3221 on USB channel)
```

- [ ] **Step 5: Compile to verify**

Run: `pio run -e IncuNest_V17`
Expected: `SUCCESS`.

- [ ] **Step 6: Commit**

```bash
git add src/system/initHardware.cpp
git commit -m "feat(motherboard): detect and validate fan RPM feedback at boot (HW16+)"
```

---

### Task 6: Boot RPM detection, validation and heater cascade — legacy (<16) path

**Files:**
- Modify: `src/system/initHardware.cpp` (`actuatorsTest()`, legacy `#else` branch)

**Interfaces:**
- Consumes: `disableFanOnUnverifiedHeaterFault()` (Task 5).

- [ ] **Step 1: Apply the cascade on heater failure**

Find:

```cpp
  if (testCurrent < HEATER_CONSUMPTION_MIN) {
    addErrorToVar(HW_error, HEATER_CONSUMPTION_MIN_ERROR);
    logE("[HW] -> Fail -> Heater current consumption is too low");
    in3.alarmToReport[HEATER_ISSUE_ALARM] = true;
    setAlarm(HEATER_ISSUE_ALARM);
    digitalWrite(ACTUATORS_EN, LOW);
    return (true);
  }
  if (testCurrent > HEATER_CONSUMPTION_MAX) {
    addErrorToVar(HW_error, HEATER_CONSUMPTION_MAX_ERROR);
    logE("[HW] -> Fail -> Heater current consumption is too high");
    in3.alarmToReport[HEATER_ISSUE_ALARM] = true;
    setAlarm(HEATER_ISSUE_ALARM);
    digitalWrite(ACTUATORS_EN, LOW);
    return (true);
  }
```

Change to:

```cpp
  if (testCurrent < HEATER_CONSUMPTION_MIN) {
    addErrorToVar(HW_error, HEATER_CONSUMPTION_MIN_ERROR);
    logE("[HW] -> Fail -> Heater current consumption is too low");
    in3.alarmToReport[HEATER_ISSUE_ALARM] = true;
    setAlarm(HEATER_ISSUE_ALARM);
    disableFanOnUnverifiedHeaterFault();
    digitalWrite(ACTUATORS_EN, LOW);
    return (true);
  }
  if (testCurrent > HEATER_CONSUMPTION_MAX) {
    addErrorToVar(HW_error, HEATER_CONSUMPTION_MAX_ERROR);
    logE("[HW] -> Fail -> Heater current consumption is too high");
    in3.alarmToReport[HEATER_ISSUE_ALARM] = true;
    setAlarm(HEATER_ISSUE_ALARM);
    disableFanOnUnverifiedHeaterFault();
    digitalWrite(ACTUATORS_EN, LOW);
    return (true);
  }
```

- [ ] **Step 2: Run the fan at its default operating speed during the test**

Find:

```cpp
  offsetCurrent = measureMeanConsumption(MAIN, FAN_SHUNT_CHANNEL);
// digitalWrite(FAN, HIGH);
#if (HW_NUM >= 8)
  ledcWrite(FAN_PWM_CHANNEL, PWM_MAX_VALUE);
#else
  digitalWrite(FAN, HIGH);
#endif
```

Change to:

```cpp
  offsetCurrent = measureMeanConsumption(MAIN, FAN_SHUNT_CHANNEL);
// digitalWrite(FAN, HIGH);
#if (HW_NUM >= 8)
  ledcWrite(FAN_PWM_CHANNEL, in3.fanPwrSupplyPWM);
#else
  digitalWrite(FAN, HIGH);
#endif
```

(HW_NUM < 8 has no PWM speed control at all — full ON is already its only "default operating speed", so that branch is unchanged.)

- [ ] **Step 3: Detect and validate RPM after the existing fan current check passes**

Find:

```cpp
  if (testCurrent < FAN_CONSUMPTION_MIN) {
    addErrorToVar(HW_error, FAN_CONSUMPTION_MIN_ERROR);
    logE("[HW] -> Fail -> Fan current consumption is too low");
    in3.alarmToReport[FAN_ISSUE_ALARM] = true;
    setAlarm(FAN_ISSUE_ALARM);
  }

  if (testCurrent > FAN_CONSUMPTION_MAX &&
      testCurrent > FAN_MAX_CURRENT_OVERRIDE * FAN_CONSUMPTION_MAX * 2) {
    addErrorToVar(HW_error, FAN_CONSUMPTION_MAX_ERROR);
    logE("[HW] -> Fail -> Fan current consumption is too high");
    in3.alarmToReport[FAN_ISSUE_ALARM] = true;
    setAlarm(FAN_ISSUE_ALARM);
  }
```

Change to:

```cpp
  if (testCurrent < FAN_CONSUMPTION_MIN) {
    addErrorToVar(HW_error, FAN_CONSUMPTION_MIN_ERROR);
    logE("[HW] -> Fail -> Fan current consumption is too low");
    in3.alarmToReport[FAN_ISSUE_ALARM] = true;
    setAlarm(FAN_ISSUE_ALARM);
  }

  if (testCurrent > FAN_CONSUMPTION_MAX &&
      testCurrent > FAN_MAX_CURRENT_OVERRIDE * FAN_CONSUMPTION_MAX * 2) {
    addErrorToVar(HW_error, FAN_CONSUMPTION_MAX_ERROR);
    logE("[HW] -> Fail -> Fan current consumption is too high");
    in3.alarmToReport[FAN_ISSUE_ALARM] = true;
    setAlarm(FAN_ISSUE_ALARM);
  }

  // Fan type detection: does this unit's assembled fan report RPM pulses?
#if defined(FAN_SPEED_FEEDBACK)
  fanSpeedHandler();
  in3.fanHasSpeedFeedback = (in3.fan_rpm > 0);
  { Preferences p; p.begin(NS_CFG, false);
    p.putUChar(KEY_FAN_RPM_FEEDBACK, in3.fanHasSpeedFeedback); p.end(); }
  logI("[HW] -> Fan type: " +
       String(in3.fanHasSpeedFeedback ? "RPM feedback" : "no RPM feedback") +
       " (" + String(in3.fan_rpm) + " rpm)");
  if (in3.fanHasSpeedFeedback && in3.fan_rpm < FAN_MIN_RPM) {
    addErrorToVar(HW_error, FAN_RPM_MIN_ERROR);
    logE("[HW] -> Fail -> Fan RPM too low (" + String(in3.fan_rpm) +
         " < " + String(FAN_MIN_RPM) + ")");
    in3.alarmToReport[FAN_ISSUE_ALARM] = true;
    setAlarm(FAN_ISSUE_ALARM);
  }
#else
  in3.fanHasSpeedFeedback = false;
#endif
```

- [ ] **Step 4: Compile to verify both hardware paths**

Run: `pio run -e IncuNest_V17`
Expected: `SUCCESS`.
Run: `pio run -e IncuNest_V16`
Expected: `SUCCESS`.

- [ ] **Step 5: Commit**

```bash
git add src/system/initHardware.cpp
git commit -m "feat(motherboard): detect and validate fan RPM feedback at boot (legacy path)"
```

---

### Task 7: Documentation + final verification

**Files:**
- Modify: `Firmware/docs/alarms.md`

- [ ] **Step 1: Update the alarms doc**

In `Firmware/docs/alarms.md`, find the table row:

```markdown
| 7 | `FAN_ISSUE_ALARM` | Loss of motor pulse (Duty Ticks encoder) | HW Fault/Emergency |
```

Leave the row as-is (it was already accurate), but add a new subsection right after section "1. Alarm Typologies and Hierarchies" documenting the new behavior. Insert after the `*Note: IDs 3 to 9...*` paragraph (end of section 1):

```markdown

### 1.1 Fan RPM Feedback (Two Supported Fan Variants)

The motherboard PCB routes `FAN_SPEED_FEEDBACK` (tachometer input) on HW_NUM
9, 13–17, but the assembled fan may or may not have that wire connected.
At boot, `actuatorsTest()` measures RPM once the fan's current draw passes
its own check, and persists the result (`in3.fanHasSpeedFeedback`) to NVS
so it survives watchdog-reboot fast paths that skip re-running the test.

- **With RPM feedback**: `FAN_ISSUE_ALARM` is raised at boot if RPM is below
  `FAN_MIN_RPM` (3000 rpm), and continuously during operation (with a 3 s
  spin-up grace period after the fan is commanded on). A heater fault
  (`HEATER_ISSUE_ALARM`) does **not** disable the fan — it keeps running and
  is independently verified by the RPM monitor.
- **Without RPM feedback**: RPM cannot be checked at all. A heater fault
  disables *both* the heater and the fan (`FAN_ISSUE_ALARM` is also raised)
  because there is no way to independently confirm the fan is still
  spinning. Recovery requires the user to power-cycle the unit — this is
  not forced automatically.
```

- [ ] **Step 2: Full build verification**

Run: `pio run -e IncuNest_V17`
Expected: `SUCCESS`
Run: `pio run -e IncuNest_V16`
Expected: `SUCCESS`

- [ ] **Step 3: Commit the docs change**

```bash
git add Firmware/docs/alarms.md
git commit -m "docs(motherboard): document fan RPM feedback detection and heater/fan cascade"
```

- [ ] **Step 4: Manual hardware verification checklist (record results, do not skip)**

This cannot be automated — perform on real HW16/17 hardware before merging:

1. Boot a unit with a **feedback-capable** fan installed. Confirm the log shows `Fan type: RPM feedback (NNNN rpm)` with `NNNN >= 3000`, and `in3.fanHasSpeedFeedback` reads back `1` from NVS after a reboot with `restoreState` forced (e.g. trigger a watchdog reset).
2. With that same unit running, physically stall/disconnect the fan. Confirm `FAN_ISSUE_ALARM` raises within `FAN_SPINUP_GRACE_MS` + one `securityCheck()` cycle, and that the fan PWM drops to 0.
3. Simulate a heater current fault on that unit (e.g. disconnect the heater during `actuatorsTest()`). Confirm only `HEATER_ISSUE_ALARM` is raised, and the fan keeps spinning.
4. Boot a unit with a **non-feedback** fan installed (or physically disconnect the tach wire). Confirm the log shows `Fan type: no RPM feedback (0 rpm)`.
5. Simulate a heater current fault on that unit. Confirm both `HEATER_ISSUE_ALARM` and `FAN_ISSUE_ALARM` raise, the fan stops, and no automatic restart occurs — the unit stays halted until manually power-cycled.
