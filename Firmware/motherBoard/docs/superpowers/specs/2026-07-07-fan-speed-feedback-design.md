# Fan speed feedback (RPM) monitoring — design

## Context

The motherboard PCB routes `FAN_SPEED_FEEDBACK` (tachometer input) for
HW_NUM 9, 13, 14, 15, 16 and 17. The same PCB can be assembled with either
of two fan models:

- A fan **without** a tachometer wire (no RPM feedback possible).
- A fan **with** a tachometer wire (FG signal), which the firmware can use
  to verify the fan is actually spinning.

Today the interrupt-driven RPM measurement pipeline already exists
(`fanEncoderISR()` → `in3.fanEncoderPeriod[]` → `fanSpeedHandler()` →
`in3.fan_rpm`, including a stall timeout that zeroes `fan_rpm` if no pulse
arrives for `FAN_UPDATE_TIME_MIN`), but it is only logged, never used to
raise an alarm. `FAN_ISSUE_ALARM` already exists in the shared alarm system
(`shared/include/alarm_ids.h`) and is documented (`docs/alarms.md`) as
triggered by "Loss of motor pulse (Duty Ticks encoder)" — it is simply not
wired up yet.

This feature closes that gap: detect which fan variant is installed,
persist that fact, validate RPM at boot, monitor it continuously during
operation, and adjust the heater/fan alarm cascade based on whether RPM
feedback is available.

## Goals

1. Auto-detect, at boot, whether the installed fan reports RPM pulses, and
   persist that as the "fan type" for this unit in NVS.
2. Run the boot actuator test at the real default operating speed, not at
   full PWM.
3. Validate RPM against a minimum threshold (3000 rpm) at boot, only when
   the unit has RPM feedback.
4. Continuously monitor RPM during operation and raise `FAN_ISSUE_ALARM` if
   it drops below the threshold while the fan is commanded on — only when
   the unit has RPM feedback.
5. Change the heater-failure cascade: if the fan has no RPM feedback, a
   heater fault must also disable the fan and requires the user to
   power-cycle the unit; if the fan has RPM feedback, a heater fault only
   disables the heater (the fan keeps running and is independently
   monitored).

## Non-goals

- No compile-time "mandatory feedback" flag. The two fan variants are both
  supported at runtime; there is no build-time policy that forbids one of
  them.
- No automatic recovery / retry loop for a failed fan. Once `FAN_ISSUE_ALARM`
  or `HEATER_ISSUE_ALARM` (no-feedback case) latches, recovery requires a
  restart, exactly like the existing `HEATER_ISSUE_ALARM` boot-time
  behavior today (nothing currently clears it either).
- No change to humidifier gating (`turnActuators()` keeps using
  `ongoingCriticalWiringAlarm()` for the humidifier).

## Design

### 1. NVS-persisted fan type (auto-detected)

New preference key in `NS_CFG`: `KEY_FAN_RPM_FEEDBACK` (uchar 0/1), read
into a new field `bool fanHasSpeedFeedback` on `IncuNest_parameters` (main.h),
loaded in `EEPROM.cpp` alongside the other `NS_CFG` reads, default `0`.

Detection happens in `actuatorsTest()` (both the HW≥16 parallel path and
the legacy sequential path), immediately after the existing fan current
check passes (`res.fan` / `testCurrent` within `FAN_CONSUMPTION_MIN/MAX`),
guarded by `#if defined(FAN_SPEED_FEEDBACK)`:

```c
fanSpeedHandler();
in3.fanHasSpeedFeedback = (in3.fan_rpm > 0);
{ Preferences p; p.begin(NS_CFG, false);
  p.putUChar(KEY_FAN_RPM_FEEDBACK, in3.fanHasSpeedFeedback); p.end(); }
logI("[HW] -> Fan type: " +
     String(in3.fanHasSpeedFeedback ? "RPM feedback" : "no RPM feedback") +
     " (" + String(in3.fan_rpm) + " rpm)");
if (in3.fanHasSpeedFeedback && in3.fan_rpm < FAN_MIN_RPM) {
  addErrorToVar(HW_error, FAN_RPM_MIN_ERROR);
  logE("[HW] -> Fail -> Fan RPM too low");
  in3.alarmToReport[FAN_ISSUE_ALARM] = true;
  setAlarm(FAN_ISSUE_ALARM);
}
```

On boards without `FAN_SPEED_FEEDBACK` (HW_NUM ≤ 8), `fanHasSpeedFeedback`
stays `false` unconditionally (no hardware capability).

The value loaded from NVS at the very start of boot (before
`actuatorsTest()` runs) is what's used everywhere else in this feature
(heater cascade decision, runtime monitor gating) — this matters because
`actuatorsTest()` is skipped entirely when `in3.restoreState == true`
(watchdog/panic reboot), so the persisted value from the last successful
detection is the only source of truth on those boots.

### 2. Boot test at default operating speed, not max PWM

`actuatorsTest()` currently hardcodes `ledcWrite(FAN_PWM_CHANNEL, PWM_MAX_VALUE)`
in both paths, ignoring the runtime-configurable `in3.fanPwrSupplyPWM`
(default `FAN_PWR_SUPPLY_PWM`, adjustable via WiFi/USB config). Change both
call sites to use `in3.fanPwrSupplyPWM` instead of the `PWM_MAX_VALUE`
literal. For HW≥16, also explicitly write
`ledcWrite(FAN_CTL_PWM_CHANNEL, in3.fanCtlPWM)` right before enabling the
fan, so the speed-control line is guaranteed to be at its configured
default during the test rather than relying on it being untouched since
`initPWMGPIO()`.

### 3. New HW error code

Add `FAN_RPM_MIN_ERROR` to the `HW_ERROR_ID` enum in `main.h`, after
`FAN_CONSUMPTION_MAX_ERROR`, following the existing numbering/reporting
pattern (`addErrorToVar(HW_error, ...)`).

### 4. Runtime RPM monitoring

New `void checkFanSpeed()` in `security.cpp`, called from `securityCheck()`,
guarded by `#if defined(FAN_SPEED_FEEDBACK)`:

- Requires `in3.fanHasSpeedFeedback == true` (runtime gate — hardware
  capability alone is not enough; the fan actually installed must have
  reported pulses at boot).
- Tracks `in3.fanCommandedOn` (new field, set inside `turnFans()` to
  `mode || in3.phototherapy`, i.e. the *intended* fan state before any
  alarm-driven PWM suppression is applied) to detect the off→on edge and
  start a spin-up grace timer (`FAN_SPINUP_GRACE_MS = 3000`).
- While `fanCommandedOn` and past the grace period:
  - `!alarmOnGoing[FAN_ISSUE_ALARM] && fan_rpm < FAN_MIN_RPM` → `setAlarm(FAN_ISSUE_ALARM)`.
  - `alarmOnGoing[FAN_ISSUE_ALARM] && fan_rpm >= FAN_MIN_RPM + FAN_MIN_RPM_HYSTERESIS` → `resetAlarm(FAN_ISSUE_ALARM)`.
- Note: in practice this rarely self-clears, because `ongoingFanCriticalAlarm()`
  (see below) cuts the fan's PWM as soon as `FAN_ISSUE_ALARM` is active, so
  the fan has no way to spin back up on its own. This is treated as
  intentional — the same behavior a critical wiring fault has today.

New defines (`board.h`): `FAN_MIN_RPM = 3000`, `FAN_MIN_RPM_HYSTERESIS = 300`.
New define (wherever task timings live, e.g. `security.cpp`):
`FAN_SPINUP_GRACE_MS = 3000`.

### 5. Decoupled heater/fan alarm cascade

New predicate in `security.cpp` (declared in `main.h` next to
`ongoingCriticalWiringAlarm()`):

```c
bool ongoingFanCriticalAlarm() {
  return alarmOnGoing[FAN_ISSUE_ALARM] ||
         alarmOnGoing[POWER_SUPPLY_ALARM] ||
         (alarmOnGoing[HEATER_ISSUE_ALARM] && !in3.fanHasSpeedFeedback);
}
```

`turnFans()` (`UI_actuatorsProgress.cpp`) switches its two
`ongoingCriticalWiringAlarm()` checks (the `FAN_PWM_CHANNEL` write and the
legacy `digitalWrite(FAN, ...)` branch) to `ongoingFanCriticalAlarm()`.
Every other use of `ongoingCriticalWiringAlarm()` (heater gating in
`Communication_Receiver`, humidifier gating in `turnActuators()`) is
unchanged.

In `actuatorsTest()`'s heater-failure branches (both HW≥16 and legacy
paths), after the existing `setAlarm(HEATER_ISSUE_ALARM)`:

```c
// Without RPM feedback there is no independent way to confirm the fan is
// still spinning once the heater has failed, so the safest option is to
// disable both actuators and require the user to power-cycle the unit —
// do NOT attempt an automatic/forced restart here.
if (!in3.fanHasSpeedFeedback) {
  in3.alarmToReport[FAN_ISSUE_ALARM] = true;
  setAlarm(FAN_ISSUE_ALARM);
}
digitalWrite(ACTUATORS_EN, LOW);
return true;
```

When `in3.fanHasSpeedFeedback == true`, this branch is skipped: only
`HEATER_ISSUE_ALARM` is raised, the fan stays enabled (protected by
`ongoingFanCriticalAlarm()` no longer including a feedback-capable fan's
heater-only fault), and continues to be independently verified by
`checkFanSpeed()` at runtime.

## Incidental fix

`board.h` defines `FAN_SPEED_FEEDBACK` for `HW_NUM >= 9`, but
`initGPIO()`/`initInterrupts()` only configure the pin and attach the
interrupt for `HW_NUM >= 10` — on HW_NUM==9 the pin is defined but never
initialized, so detection would always read 0 pulses. Align both `#if`
guards in `initHardware.cpp` to `HW_NUM >= 9` to match `board.h`.

## Error handling / edge cases

- Fan current test still fails independently on `FAN_CONSUMPTION_MIN/MAX`
  exactly as today; RPM detection only runs after that check passes.
- If `FAN_SPEED_FEEDBACK` isn't defined for the HW_NUM at all, all RPM code
  compiles out and `fanHasSpeedFeedback` is always `false`.
- `restoreState` boots skip `actuatorsTest()` entirely and rely on the
  NVS-persisted `fanHasSpeedFeedback` from the last normal boot.

## Testing

- Unit/behavioral: extend `test/test_alarms/test_alarm_machine.cpp`-style
  coverage (or equivalent security.cpp tests) for `ongoingFanCriticalAlarm()`
  truth table (fan alarm, power alarm, heater alarm × with/without feedback).
- Manual/hardware: verify on a HW16/17 unit with each fan variant that (a)
  boot correctly detects and persists the type, (b) a simulated RPM drop
  during operation raises `FAN_ISSUE_ALARM` after the spin-up grace period,
  (c) a simulated heater current fault disables only the heater when
  feedback is present, and both heater+fan (with no forced restart) when it
  isn't.
