# Fan Speed Control (4000 rpm) and Air-Blocked Alarm Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Drive the fan under closed-loop RPM control (fixed 4000 rpm setpoint) on HW≥16 units with detected tach feedback, and raise a new, non-critical, cross-board `AIR_BLOCKED_ALARM` when the duty needed to hold that setpoint stays abnormally high — decoupling air-outlet blockage detection from the heater/battery voltage noise that made a raw RPM floor unreliable.

**Architecture:** A new `fanControlPID` object (same `PID` library pattern as `airControlPID`/`skinControlPID`) replaces the fixed-duty write to `FAN_CTL_PWM_CHANNEL` whenever a unit has RPM feedback, mode driven directly by `in3.fanCommandedOn` inside `turnFans()`. `AIR_BLOCKED_ALARM` is a new entry in the *shared* `AlarmId` enum (consumed by both motherBoard and Display_HMI), raised by a new `checkAirBlockage()` mirroring the existing `checkFanSpeed()`, and — critically — never added to any of the three critical-alarm predicates, so it never disables the heater or the fan.

**Tech Stack:** PlatformIO + Arduino, ESP32-S3, C++17, Arduino-PID-Library (`PID_v1.h`), the existing shared-header cross-board protocol (`Firmware/shared/include/alarm_ids.h`, `Firmware/PROTOCOL.md`).

## Global Constraints

- Applies **only** to HW≥16 units where `in3.fanHasSpeedFeedback == true`. Everywhere else (older hardware, or a unit whose fan never reported pulses), behavior is unchanged: `turnFans()` keeps writing the fixed `in3.fanCtlPWM` duty, `fanControlPID` is simply never switched to `AUTOMATIC`, and `checkAirBlockage()` never evaluates.
- `FAN_TARGET_RPM = 4000`. Factory baseline duty to hold it with a clean outlet: `137`/255. Starting threshold for "blocked" (to be confirmed on the bench, not guessed further): `FAN_DUTY_BLOCKED_THRESHOLD = 160`.
- `AIR_BLOCKED_ALARM` must **never** appear in `ongoingCriticalAlarm()`, `ongoingCriticalWiringAlarm()`, or `ongoingFanCriticalAlarm()` (motherboard) — it is notify-only (sound + display via the standard `setAlarm()`), and must **not** be added to `isFanHeaterAlarmActive()` / `update_alarm_panels()`'s fan/heater UI-gating checks on Display_HMI.
- New alarm ID is appended at the **end** of the enum in `Firmware/shared/include/alarm_ids.h`, before `NUM_ALARMS` — existing bit positions must not shift (they're referenced by cloud telemetry and the HMI's `alarmBitmask`).
- Per `Firmware/.claude/rules/embedded-shared.md`: a change to `shared/**` must update **both** consumers (motherBoard and Display_HMI) in the same logical change, and the commit documents what was updated on each side.
- No automated tests exist for any of the touched files on either board (`PID.cpp`, `security.cpp`, `initHardware.cpp`, `Actuators.cpp` are outside motherBoard's `[env:native]` Unity filter; Display_HMI has no test environment at all). Verification is `pio run` on `IncuNest_V17`/`IncuNest_V16` (motherboard) and a Display_HMI build, plus a manual hardware checklist in the final task.
- Commits: Conventional Commits, scope `motherboard`/`hmi`/`shared` as appropriate, single author, no `Co-Authored-By` trailer (per `Firmware/.claude/rules/commits.md`). Cross-board (`shared/**`) changes get scope `(shared)`.
- Working branch: `feat/fan-speed-feedback` (already checked out — this is now the de facto shared integration branch for this round of fan work; do not create a new branch).

---

## File Map

| File | Change |
|---|---|
| `include/config/board.h` | New `FAN_TARGET_RPM`, `FAN_DUTY_BLOCKED_THRESHOLD` defines |
| `include/tasks/PID.h` | New `KP_FAN`/`KI_FAN`/`KD_FAN`/`PID_FAN_SAMPLE_TIME` defines |
| `src/system/PID.cpp` | New `fanControlPID` object; `PIDHandler()` drives it |
| `src/system/Actuators.cpp` | `turnFans()` sets `fanControlPID` mode instead of a fixed CTL duty |
| `src/system/initHardware.cpp` | Boot RPM check (both `actuatorsTest()` branches) enables closed-loop control and checks duty, not just RPM |
| `src/system/security.cpp` | New `checkAirBlockage()`; alarm text for `AIR_BLOCKED_ALARM` |
| `Firmware/shared/include/alarm_ids.h` | New `AIR_BLOCKED_ALARM` enum value |
| `include/config/telemetry_keys.h`, `src/tasks/GPRS.cpp`, `src/tasks/Wifi_OTA.cpp` | New telemetry key + switch case for the new alarm |
| `Firmware/Display_HMI/include/main.h` | Fix `MAX_ALARMS` to track `NUM_ALARMS` |
| `Firmware/docs/alarms.md` | New alarm table row |

---

### Task 1: Shared alarm ID + Display_HMI fix

**Files:**
- Modify: `Firmware/shared/include/alarm_ids.h`
- Modify: `Firmware/Display_HMI/include/main.h:290`

**Interfaces:**
- Produces: `AIR_BLOCKED_ALARM` (new `AlarmId` enum value, consumed by Task 2 on the motherboard side).

- [ ] **Step 1: Add the new alarm ID**

In `Firmware/shared/include/alarm_ids.h`, find:

```c
  FAN_ISSUE_ALARM,
  HEATER_ISSUE_ALARM,
  POWER_SUPPLY_ALARM,
  NUM_ALARMS,
```

Change to:

```c
  FAN_ISSUE_ALARM,
  HEATER_ISSUE_ALARM,
  POWER_SUPPLY_ALARM,
  AIR_BLOCKED_ALARM,
  NUM_ALARMS,
```

- [ ] **Step 2: Fix `MAX_ALARMS` on Display_HMI**

In `Firmware/Display_HMI/include/main.h`, find:

```cpp
constexpr int MAX_ALARMS = 10;
```

Change to:

```cpp
constexpr int MAX_ALARMS = NUM_ALARMS;
```

This is not optional: `Display_HMI/src/tasks/CommTask.cpp`'s range check (`alarm.id >= MAX_ALARMS`) would otherwise silently drop every `AIR_BLOCKED_ALARM` message, and `alarmList[MAX_ALARMS]` would have no slot for it.

- [ ] **Step 3: Compile both boards**

Run (from `Firmware/motherBoard`): `pio run -e IncuNest_V17` — Expected: `SUCCESS` (this task alone doesn't add any motherboard logic yet, just the enum value flowing through unused).
Run (from `Firmware/Display_HMI`): `pio run` (or the project's default env — check `Firmware/Display_HMI/platformio.ini` for the env name if `pio run` without `-e` fails) — Expected: `SUCCESS`.

- [ ] **Step 4: Commit**

```bash
git add Firmware/shared/include/alarm_ids.h Firmware/Display_HMI/include/main.h
git commit -m "feat(shared): add AIR_BLOCKED_ALARM and fix HMI MAX_ALARMS to track it

motherboard: no consumer yet (added in a later commit).
hmi: MAX_ALARMS now tracks NUM_ALARMS instead of a stale hardcoded 10,
so this and any future new alarm ID isn't silently dropped."
```

---

### Task 2: Fan speed PID loop (motherboard)

**Files:**
- Modify: `include/main.h` (widen `in3.fan_rpm` to `double`)
- Modify: `include/config/board.h` (near `FAN_MIN_RPM`)
- Modify: `include/tasks/PID.h`
- Modify: `src/system/PID.cpp`
- Modify: `src/system/Actuators.cpp` (`turnFans()`)

**Correction (discovered during implementation):** `in3.fan_rpm` is declared
`float` in `include/main.h`, but the `PID_v1` library's constructor requires
`double*` for Input/Output/Setpoint — every other field the 3 existing PID
loops bind to (`in3.temperature[...]`, `in3.humidity[...]`,
`in3.desiredControlTemperature`, `in3.desiredControlHumidity`) is already
`double`, so widening `fan_rpm` to match is the consistent fix, not a new
pattern. All consumers (`sensors_module.cpp`, `initHardware.cpp`,
`security.cpp`, `GPRS.cpp`, `Wifi_OTA.cpp`) only assign/compare/print it —
none require it to stay `float`, so this is a safe, mechanical widening.

**Interfaces:**
- Consumes: `in3.fan_rpm`, `in3.fanCommandedOn`, `in3.fanHasSpeedFeedback`, `FAN_CTL_PWM_CHANNEL`, `ongoingFanCriticalAlarm()` (all pre-existing).
- Produces: `PID fanControlPID` (global object, extern-accessible), `double fanControlPIDOutput`, `FAN_TARGET_RPM`, `FAN_DUTY_BLOCKED_THRESHOLD` (consumed by Task 3).

- [ ] **Step 0: Widen `in3.fan_rpm` to `double`**

In `include/main.h`, find:

```cpp
  float fan_rpm = false;
```

Change to:

```cpp
  double fan_rpm = false;
```

`include/main.h` currently has an unrelated pre-existing uncommitted change
in the working tree (an `AIR_TEMPERATURE_SET_MAX` value tweak). Use the same
stash technique as Task 1 to keep it out of this commit:
`git stash push -- include/main.h` before this edit, make the edit, commit
(Step 8), then `git stash pop` afterward to restore the unrelated change as
an uncommitted diff again. If the pop reports a conflict, stop and report it
rather than resolving it yourself.

- [ ] **Step 1: Add the RPM target and blockage-threshold defines**

In `include/config/board.h`, find:

```c
#define FAN_MIN_RPM 3000            // minimum acceptable fan RPM when speed feedback is present
#define FAN_MIN_RPM_HYSTERESIS 300  // rpm above FAN_MIN_RPM required to clear FAN_ISSUE_ALARM
```

Change to:

```c
#define FAN_MIN_RPM 3000            // minimum acceptable fan RPM when speed feedback is present
#define FAN_MIN_RPM_HYSTERESIS 300  // rpm above FAN_MIN_RPM required to clear FAN_ISSUE_ALARM

// Closed-loop fan speed control (HW>=16, feedback-capable units only).
#define FAN_TARGET_RPM 4000
// Factory baseline duty (0-255) to hold FAN_TARGET_RPM with a clean air
// outlet is 137. This threshold (margin above baseline) must be confirmed
// on the bench against real unit-to-unit variance, not guessed further.
#define FAN_DUTY_BLOCKED_THRESHOLD 160
#define FAN_DUTY_BLOCKED_HYSTERESIS 15 // duty below (threshold - this) required to clear AIR_BLOCKED_ALARM
```

- [ ] **Step 2: Add fan PID tuning constants**

In `include/tasks/PID.h`, find:

```cpp
#define KP_HUMIDITY 200
#define KI_HUMIDITY 2
#define KD_HUMIDITY 20
#define AWO_HUMIDITY 5
```

Change to:

```cpp
#define KP_HUMIDITY 200
#define KI_HUMIDITY 2
#define KD_HUMIDITY 20
#define AWO_HUMIDITY 5

// Fan RPM closed-loop control. Not part of the numPID-indexed arrays above
// (air/skin/humidity are mutually-exclusive control *modes*; the fan loop
// simply follows in3.fanCommandedOn independently of which mode is active).
// These are starting values — must be bench-tuned on real hardware, exactly
// like the gains above were.
#define KP_FAN 0.5
#define KI_FAN 0.3
#define KD_FAN 0.0
#define PID_FAN_SAMPLE_TIME 200
```

- [ ] **Step 3: Add the `fanControlPID` object**

In `src/system/PID.cpp`, find:

```cpp
double HeaterPIDOutput;
double skinControlPIDInput;
double airControlPIDInput;
double humidityControlPIDOutput;
int humidifierTimeCycle = 5000;
unsigned long windowStartTime;
```

Change to:

```cpp
double HeaterPIDOutput;
double skinControlPIDInput;
double airControlPIDInput;
double humidityControlPIDOutput;
int humidifierTimeCycle = 5000;
unsigned long windowStartTime;
double fanControlPIDOutput;
double fanTargetRPM = FAN_TARGET_RPM;
```

Then find:

```cpp
PID humidityControlPID(&in3.humidity[ROOM_DIGITAL_HUM_SENSOR],
                       &humidityControlPIDOutput, &in3.desiredControlHumidity,
                       Kp[humidityPID], Ki[humidityPID], Kd[humidityPID],
                       P_ON_E, DIRECT);
```

Right after it, add:

```cpp
PID fanControlPID(&in3.fan_rpm, &fanControlPIDOutput, &fanTargetRPM, KP_FAN,
                  KI_FAN, KD_FAN, P_ON_E, DIRECT);
```

- [ ] **Step 4: Initialize the PID mode**

In `src/system/PID.cpp`, find:

```cpp
void PIDInit()
{
  airControlPID.SetMode(MANUAL);
  skinControlPID.SetMode(MANUAL);
  humidityControlPID.SetMode(MANUAL);
}
```

Change to:

```cpp
void PIDInit()
{
  airControlPID.SetMode(MANUAL);
  skinControlPID.SetMode(MANUAL);
  humidityControlPID.SetMode(MANUAL);
  fanControlPID.SetMode(MANUAL);
  fanControlPID.SetOutputLimits(0, PWM_MAX_VALUE);
  fanControlPID.SetSampleTime(PID_FAN_SAMPLE_TIME);
}
```

- [ ] **Step 5: Drive the loop from `PIDHandler()`**

In `src/system/PID.cpp`, find the end of `PIDHandler()` — the closing brace right after the `humidityControlPID` block (the `}` that ends the `if (humidityControlPID.GetMode() == AUTOMATIC)` block, followed by the function's own closing `}`). Add a new block just before `PIDHandler()`'s final closing brace:

```cpp
  if (fanControlPID.GetMode() == AUTOMATIC)
  {
    fanControlPID.Compute();
    ledcWrite(FAN_CTL_PWM_CHANNEL,
              fanControlPIDOutput * !ongoingFanCriticalAlarm());
  }
}
```

(i.e. the existing final `}` of `PIDHandler()` becomes the closing brace of this new `if` block, and a fresh `}` closes the function — read the current end of `PIDHandler()` carefully before editing so you don't end up with mismatched braces.)

- [ ] **Step 6: Wire the mode into `turnFans()`**

This codebase declares `extern PID <name>;` locally in each `.cpp` file that
needs it (see `src/system/security.cpp:148`'s `extern PID airControlPID;`),
rather than centralizing in a header — follow that convention here.

In `src/system/Actuators.cpp`, find:

```cpp
extern IncuNest_parameters in3;
```

Change to:

```cpp
extern IncuNest_parameters in3;
extern PID fanControlPID;
extern double fanControlPIDOutput;
```

Then find:

```cpp
void turnFans(bool mode) {
  in3.fanCommandedOn = mode || in3.phototherapy;
  digitalWrite(ACTUATORS_EN, mode || in3.phototherapy);
#if (HW_NUM >= 8)
  ledcWrite(FAN_PWM_CHANNEL,
            (mode && !ongoingFanCriticalAlarm()) * in3.fanPwrSupplyPWM);
  ledcWrite(FAN_CTL_PWM_CHANNEL, mode * in3.fanCtlPWM);
#else
  digitalWrite(FAN, in3.phototherapy || mode && !ongoingFanCriticalAlarm());
#endif
}
```

Change to:

```cpp
void turnFans(bool mode) {
  in3.fanCommandedOn = mode || in3.phototherapy;
  digitalWrite(ACTUATORS_EN, mode || in3.phototherapy);
#if (HW_NUM >= 8)
  ledcWrite(FAN_PWM_CHANNEL,
            (mode && !ongoingFanCriticalAlarm()) * in3.fanPwrSupplyPWM);
#if defined(FAN_SPEED_FEEDBACK)
  if (in3.fanHasSpeedFeedback) {
    fanControlPID.SetMode(in3.fanCommandedOn ? AUTOMATIC : MANUAL);
    if (!in3.fanCommandedOn) {
      fanControlPIDOutput = 0;
      ledcWrite(FAN_CTL_PWM_CHANNEL, 0);
    }
  } else
#endif
  {
    ledcWrite(FAN_CTL_PWM_CHANNEL, mode * in3.fanCtlPWM);
  }
#else
  digitalWrite(FAN, in3.phototherapy || mode && !ongoingFanCriticalAlarm());
#endif
}
```

(When `fanControlPID` is `AUTOMATIC`, `PIDHandler()`'s new block from Step 5 owns writing `FAN_CTL_PWM_CHANNEL` on every cycle — the plain `ledcWrite` in the `else` branch here only applies to units without feedback, which keep today's fixed-duty behavior untouched.)

- [ ] **Step 7: Compile to verify**

Run: `pio run -e IncuNest_V17` — Expected: `SUCCESS`.
Run: `pio run -e IncuNest_V16` — Expected: `SUCCESS` (HW16 also has `FAN_CTL_PWM_CHANNEL`; confirm `FAN_SPEED_FEEDBACK` is defined for it in `board.h` — it is, per the existing HW≥16 pin block).

- [ ] **Step 8: Commit**

```bash
git add include/main.h include/config/board.h include/tasks/PID.h src/system/PID.cpp src/system/Actuators.cpp
git commit -m "feat(motherboard): add closed-loop fan RPM control at 4000 rpm

Widens in3.fan_rpm from float to double: PID_v1 requires double* for its
Input/Output/Setpoint, matching every other field the existing PID loops
already bind to."
```

---

### Task 3: Boot-time closed-loop check + `AIR_BLOCKED_ALARM` runtime monitor

**Files:**
- Modify: `src/system/initHardware.cpp` (HW≥16 branch of `actuatorsTest()`, around the existing RPM-detection block)
- Modify: `src/system/security.cpp` (new `checkAirBlockage()`, alarm text)

**Interfaces:**
- Consumes: `fanControlPID`, `fanControlPIDOutput`, `FAN_TARGET_RPM`, `FAN_DUTY_BLOCKED_THRESHOLD`, `FAN_DUTY_BLOCKED_HYSTERESIS` (Task 2), `AIR_BLOCKED_ALARM` (Task 1).

- [ ] **Step 0: Add the PID externs**

Find:

```cpp
extern PID airControlPID;
```

Right after it, add:

```cpp
extern PID fanControlPID;
extern double fanControlPIDOutput;
```

- [ ] **Step 1: Read the current boot RPM-detection block before editing**

Read `src/system/initHardware.cpp` around the comment `// Fan type detection: does this unit's assembled fan report RPM pulses?` in the HW≥16 branch (inside the `#if defined(FAN_SPEED_FEEDBACK)` guard, roughly lines 897-930 as of this plan's writing — confirm the exact current lines, since earlier tasks in this same file may have shifted them). This is the block with the `FAN_RPM_SETTLE_ITERATIONS` loop and the `if (in3.fanHasSpeedFeedback && in3.fan_rpm < FAN_MIN_RPM)` check.

- [ ] **Step 2: Add closed-loop engagement and duty check after the existing RPM-floor check**

Immediately after the existing block's `if (in3.fanHasSpeedFeedback && in3.fan_rpm < FAN_MIN_RPM) { ... }` (which still stands, unchanged — it's still the correct "total failure" check), add:

```cpp
  if (in3.fanHasSpeedFeedback && in3.fan_rpm >= FAN_MIN_RPM) {
    // Engage closed-loop control and let it settle at the real target
    // before checking how much duty it took to get there.
    fanControlPID.SetMode(AUTOMATIC);
    for (int i = 0; i < FAN_RPM_SETTLE_ITERATIONS; i++) {
      vTaskDelay(pdMS_TO_TICKS(FAN_RPM_SETTLE_INTERVAL_MS));
      fanSpeedHandler();
      fanControlPID.Compute();
      ledcWrite(FAN_CTL_PWM_CHANNEL, fanControlPIDOutput);
    }
    logI("[HW] -> Fan duty to hold " + String(FAN_TARGET_RPM) + " rpm: " +
         String(fanControlPIDOutput) + " (rpm=" + String(in3.fan_rpm) + ")");
    if (fanControlPIDOutput > FAN_DUTY_BLOCKED_THRESHOLD) {
      logE("[HW] -> Warning -> Fan duty too high, possible air outlet blockage");
      in3.alarmToReport[AIR_BLOCKED_ALARM] = true;
      setAlarm(AIR_BLOCKED_ALARM);
    }
  }
```

This does not replace the existing `FAN_RPM_MIN_ERROR`/`FAN_ISSUE_ALARM` check above it — both run; a fan that can't reach `FAN_MIN_RPM` at all never reaches this new block (guarded by `fan_rpm >= FAN_MIN_RPM`), so `FAN_ISSUE_ALARM` and `AIR_BLOCKED_ALARM` cannot both fire from the boot test for the same failure.

- [ ] **Step 3: Add `checkAirBlockage()` in `security.cpp`**

Find:

```cpp
extern PID airControlPID;
```

Right after it, add:

```cpp
extern PID fanControlPID;
extern double fanControlPIDOutput;
```

Then find the existing `checkFanSpeed()` function (inside its `#if defined(FAN_SPEED_FEEDBACK)` guard). Immediately after its closing `}` (and before the guard's `#endif`), add:

```cpp

void checkAirBlockage()
{
  if (fanControlPID.GetMode() != AUTOMATIC)
  {
    return; // not under closed-loop control — no duty signal to evaluate
  }
  if (alarmOnGoing[FAN_ISSUE_ALARM])
  {
    return; // total fan failure already reported — don't also report this
  }
  if (!alarmOnGoing[AIR_BLOCKED_ALARM] &&
      fanControlPIDOutput > FAN_DUTY_BLOCKED_THRESHOLD)
  {
    in3.alarmToReport[AIR_BLOCKED_ALARM] = true;
    setAlarm(AIR_BLOCKED_ALARM);
  }
  else if (alarmOnGoing[AIR_BLOCKED_ALARM] &&
           fanControlPIDOutput <=
               FAN_DUTY_BLOCKED_THRESHOLD - FAN_DUTY_BLOCKED_HYSTERESIS)
  {
    in3.alarmToReport[AIR_BLOCKED_ALARM] = false;
    resetAlarm(AIR_BLOCKED_ALARM);
  }
}
```

- [ ] **Step 4: Wire it into `securityCheck()`**

Find:

```cpp
void securityCheck()
{
  checkThermalCutOuts();
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

Change to:

```cpp
void securityCheck()
{
  checkThermalCutOuts();
  checkAlarms();
  sensorHealthMonitor();
  powerSupplyCheck();
#if defined(FAN_SPEED_FEEDBACK)
  checkFanSpeed();
  checkAirBlockage();
#endif
#if (HW_NUM >= 16)
  checkUsbFault();
#endif
}
```

- [ ] **Step 5: Add alarm text**

Find the `case FAN_ISSUE_ALARM:` block inside `alarmIDtoString()` in `security.cpp`. Add a new case right after it (matching whatever exact language-branching style — SPANISH/FRENCH/default — the surrounding cases use):

```cpp
    case AIR_BLOCKED_ALARM:
      if (in3.language == SPANISH) return (char *)("SALIDA DE AIRE OBSTRUIDA");
      if (in3.language == FRENCH) return (char *)("SORTIE D'AIR OBSTRUEE");
      return (char *)("AIR OUTLET BLOCKED");
```

Find the equivalent `case FAN_ISSUE_ALARM:` block inside `sendAlarmUSB()`'s title/description switch. Add a matching case with a title and a longer description, following that function's exact existing style (read a neighboring case first — e.g. `HUMIDITY_ALARM`'s, since like `AIR_BLOCKED_ALARM` it is non-critical — before writing this one, to match its structure precisely).

- [ ] **Step 6: Compile to verify**

Run: `pio run -e IncuNest_V17` — Expected: `SUCCESS`.
Run: `pio run -e IncuNest_V16` — Expected: `SUCCESS`.

- [ ] **Step 7: Commit**

```bash
git add src/system/initHardware.cpp src/system/security.cpp
git commit -m "feat(motherboard): detect air outlet blockage via fan duty at boot and runtime"
```

---

### Task 4: Telemetry for `AIR_BLOCKED_ALARM`

**Files:**
- Modify: `include/config/telemetry_keys.h`
- Modify: `src/tasks/GPRS.cpp`
- Modify: `src/tasks/Wifi_OTA.cpp`

- [ ] **Step 1: Add the telemetry key**

In `include/config/telemetry_keys.h`, find:

```c
#define POWER_SUPPLY_ALARM_KEY "power_alarm"
```

Change to:

```c
#define POWER_SUPPLY_ALARM_KEY "power_alarm"
#define AIR_BLOCKED_ALARM_KEY "air_blocked_alarm"
```

- [ ] **Step 2: Add the GPRS switch case**

In `src/tasks/GPRS.cpp`, find the `switchAlarmTelemetryGPRS()` function's `case POWER_SUPPLY_ALARM:` line. Add right after it:

```cpp
  case AIR_BLOCKED_ALARM:        alarmKey = AIR_BLOCKED_ALARM_KEY; break;
```

(match the exact alignment/style of the surrounding cases in that switch).

- [ ] **Step 3: Add the WiFi switch case**

In `src/tasks/Wifi_OTA.cpp`, find `switchAlarmTelemetryWIFI()`'s `case POWER_SUPPLY_ALARM:` line and add the equivalent case right after it, matching GPRS.cpp's wording.

- [ ] **Step 4: Compile to verify**

Run: `pio run -e IncuNest_V17` — Expected: `SUCCESS`.
Run: `pio run -e IncuNest_V16` — Expected: `SUCCESS`.

- [ ] **Step 5: Commit**

```bash
git add include/config/telemetry_keys.h src/tasks/GPRS.cpp src/tasks/Wifi_OTA.cpp
git commit -m "feat(motherboard): send AIR_BLOCKED_ALARM state to ThingsBoard"
```

---

### Task 5: Documentation + final verification

**Files:**
- Modify: `Firmware/docs/alarms.md`

- [ ] **Step 1: Add the new alarm to the table**

In `Firmware/docs/alarms.md`, find the table row for `POWER_SUPPLY_ALARM` (the last row, ID 9). Add a new row after it:

```markdown
| 10 | `AIR_BLOCKED_ALARM` | Sustained above-normal fan PWM duty while holding the closed-loop RPM setpoint (4000 rpm) | Medical/Mild |
```

Add a short paragraph after the table (or extend the existing "Fan RPM Feedback" subsection if the fan-speed-feedback design's docs update already added one — check first) explaining: this alarm is notify-only (does not disable heater or fan), exists only for HW≥16 feedback-capable units, and complements `FAN_ISSUE_ALARM` (which still covers total fan failure) by catching partial air-outlet obstruction that a raw RPM floor could not distinguish from heater/battery-induced voltage sag.

- [ ] **Step 2: Full build verification, both boards**

Run (from `Firmware/motherBoard`): `pio run -e IncuNest_V17` and `pio run -e IncuNest_V16` — Expected: `SUCCESS`.
Run (from `Firmware/Display_HMI`): its build command — Expected: `SUCCESS`.

- [ ] **Step 3: Commit**

```bash
git add Firmware/docs/alarms.md
git commit -m "docs: document AIR_BLOCKED_ALARM and the fan speed control design"
```

- [ ] **Step 4: Manual hardware verification checklist (record results, do not skip)**

None of this is automatable:

1. Bench-tune `fanControlPID`'s `KP_FAN`/`KI_FAN`/`KD_FAN`/`PID_FAN_SAMPLE_TIME` on a real HW16/17 unit — confirm it holds ~4000 rpm stably through a heater on/off cycle, without oscillation or excessive overshoot.
2. Confirm the 137 factory-baseline duty reading is reproducible across a few clean units; set `FAN_DUTY_BLOCKED_THRESHOLD`/`FAN_DUTY_BLOCKED_HYSTERESIS` from real variance data.
3. Partially obstruct the air outlet on a running unit; confirm `AIR_BLOCKED_ALARM` raises, the heater and fan are **not** disabled, and it displays on the HMI like any other alarm.
4. Power on a unit with the outlet already partially blocked; confirm `AIR_BLOCKED_ALARM` raises on the very first boot.
5. Fully block the outlet (or stall the fan); confirm `FAN_ISSUE_ALARM` fires instead of (not alongside) `AIR_BLOCKED_ALARM`.
6. Confirm the corrected `MAX_ALARMS` on Display_HMI doesn't regress display of the existing 9 alarms (trigger a few of them, confirm they still show and clear correctly).
