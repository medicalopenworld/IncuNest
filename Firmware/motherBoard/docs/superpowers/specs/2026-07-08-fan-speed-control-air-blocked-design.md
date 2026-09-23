# Fan speed control (4000 rpm) and air-outlet blockage detection — design

## Context

The previous feature (`feat/fan-speed-feedback`, in PR review at the time of
this writing) added RPM sensing, a fixed-duty fan drive, and a low-RPM
`FAN_ISSUE_ALARM` triggered when RPM drops below `FAN_MIN_RPM` (3000).

That alarm cannot distinguish between causes: a partially blocked air outlet
lowers RPM by only ~100 rpm, an order of magnitude smaller than the RPM swing
caused by the heater turning on (system voltage sag under load) or the
battery discharging. A fixed-duty, open-loop fan makes RPM a function of
*both* mechanical load (what we want to detect) and supply voltage (noise),
with the mechanical signal buried in the electrical one. Comparing RPM
directly against any fixed floor cannot separate them, and there is no way
to establish a per-unit "clean baseline" in the field if the outlet is
already partially blocked the first time the unit is ever powered.

This design closes that gap: drive the fan under closed-loop RPM control so
RPM is no longer the ambiguous, disturbed signal — it is the fixed setpoint.
Supply voltage variation is compensated for automatically by the loop. The
diagnostic signal moves to the control effort (PWM duty) needed to hold the
setpoint, which is a clean, voltage-independent proxy for mechanical load.

## Goals

1. On HW≥16 units with detected fan RPM feedback, drive the fan via closed-loop
   PID control targeting a fixed `FAN_TARGET_RPM` (4000 rpm) instead of a
   fixed PWM duty.
2. Detect sustained above-normal duty (a proxy for a partially blocked air
   outlet) and raise a new, dedicated, non-critical alarm (`AIR_BLOCKED_ALARM`)
   — this must be independent of `FAN_ISSUE_ALARM` (total fan failure) and
   must not disable the heater or the fan.
3. Perform this check at boot as well as continuously during operation, so a
   duct blocked before the unit is ever powered is caught on the very first
   boot rather than requiring a prior "clean" baseline.
4. Keep the existing `FAN_ISSUE_ALARM` (RPM below `FAN_MIN_RPM` despite
   maximum control effort) as the backstop for total fan failure — this
   design does not change its trigger condition, only removes the need to
   also catch partial blockage through it.

## Non-goals

- No change for units without detected RPM feedback (`in3.fanHasSpeedFeedback
  == false`) or for hardware revisions below HW16 (no dedicated
  `FAN_CTL_PWM_CHANNEL` speed-control line) — they keep today's fixed-duty
  behavior unchanged, with no closed-loop control and no
  `AIR_BLOCKED_ALARM`.
- No per-unit factory calibration of the blockage threshold. A single fixed
  duty threshold is used for all units of a given hardware revision, seeded
  from a known factory baseline (137/255 PWM counts to hold ~4000 rpm with a
  clean outlet) plus a margin to be confirmed on the bench.
- No severity tiers or new UI treatment on the Display_HMI side —
  `AIR_BLOCKED_ALARM` renders exactly like every other alarm (red panel,
  blink), matching the HMI's existing all-alarms-equal design.
- No change to `ongoingCriticalAlarm()`, `ongoingCriticalWiringAlarm()`, or
  `ongoingFanCriticalAlarm()` — `AIR_BLOCKED_ALARM` never appears in any of
  them; it is notify-only (sound + display), like `HUMIDITY_ALARM` is today.

## Design

### 1. Fan speed PID loop (motherboard, HW≥16 + feedback units only)

New `PID fanControlPID` object, following the existing pattern in
`src/system/PID.cpp` (`airControlPID`, `skinControlPID`):

- Input: `in3.fan_rpm`. Setpoint: `FAN_TARGET_RPM` (new define, 4000).
  Output: new `fanControlPIDOutput` (double).
- Output limits: `0`–`PWM_MAX_VALUE` (255, matching `FAN_CTL_PWM_CHANNEL`'s
  8-bit resolution).
- Tunings (Kp/Ki/Kd) and sample time are new constants to be bench-tuned —
  not guessable from this design, exactly like the existing air/skin/humidity
  gains. Starting sample time proposal: ~200 ms (matching the humidity loop's
  cadence — fast enough to correct a heater-induced voltage sag within
  roughly a second, slow enough to avoid chasing noise in the
  Butterworth-filtered RPM signal).
- Mode is driven directly from `turnFans()` (`src/legacy/UI_actuatorsProgress.cpp`),
  not through the existing `startPID`/`stopPID(byte var)` switch (which
  selects between air/skin/humidity control *modes* — the fan has no such
  mode selection, it simply follows `in3.fanCommandedOn`):
  `fanControlPID.SetMode(in3.fanCommandedOn ? AUTOMATIC : MANUAL)`, resetting
  output to 0 on the OFF transition.
- `PIDHandler()` (`src/system/PID.cpp`) gains one new block, gated on
  `fanControlPID.GetMode() == AUTOMATIC`:
  `fanControlPID.Compute(); ledcWrite(FAN_CTL_PWM_CHANNEL, fanControlPIDOutput * !ongoingFanCriticalAlarm());`
  — reusing the same fan-specific critical-alarm gate the previous feature
  already introduced, so a `FAN_ISSUE_ALARM`/`POWER_SUPPLY_ALARM` still cuts
  the fan exactly as it does today.
- On units without feedback or below HW16, this PID object is simply never
  switched to `AUTOMATIC`; `turnFans()` keeps writing the existing fixed
  `in3.fanCtlPWM` value as it does today.

### 2. Boot behavior (`actuatorsTest()`, HW≥16 branch)

Replace the current fixed-duty fan drive with: enable `fanControlPID` at
`FAN_TARGET_RPM`, wait for it to settle (~2 s, on the same principle as the
existing RPM-filter settle loop), then evaluate two independent things:

- **Achieved RPM** — if the closed loop cannot reach anywhere near
  `FAN_TARGET_RPM` (i.e. `in3.fan_rpm` stays below `FAN_MIN_RPM` despite
  the PID output saturating), that is unchanged from today's total-failure
  path: `FAN_ISSUE_ALARM`.
- **Required duty** — if the loop successfully holds `FAN_TARGET_RPM` but
  needs a duty above `FAN_DUTY_BLOCKED_THRESHOLD` (new define, proposed
  starting value ~160, i.e. factory baseline 137 plus a bench-tuned margin)
  sustained for the settle window, raise `AIR_BLOCKED_ALARM`.

This is what makes a duct blocked *before* the unit was ever powered
detectable on the very first boot — the check no longer depends on having
seen a "clean" RPM baseline for this specific unit, only on comparing duty
against the fixed, hardware-revision-wide threshold.

### 3. Runtime monitoring — `checkAirBlockage()` (new, `security.cpp`)

Mirrors the existing `checkFanSpeed()` structure:

- Only evaluates while `fanControlPID.GetMode() == AUTOMATIC` (fan running
  under closed-loop control) **and** `!alarmOnGoing[FAN_ISSUE_ALARM]` — if
  the fan has already failed outright, that alarm takes priority and
  `AIR_BLOCKED_ALARM` is suppressed to avoid redundant/confusing alarms.
- Same duty threshold as the boot check (`FAN_DUTY_BLOCKED_THRESHOLD`), with
  hysteresis (a lower duty must be sustained before clearing) to avoid
  bounce, following the same set/clear pattern as `evaluateAlarm()`.
- Called from `securityCheck()` alongside `checkFanSpeed()`.

### 4. New alarm: `AIR_BLOCKED_ALARM` (shared, cross-board)

- Appended to the end of the `AlarmId` enum in
  `Firmware/shared/include/alarm_ids.h`, **before** `NUM_ALARMS` (so
  existing bit positions used by telemetry/protocol do not shift).
- Motherboard (`security.cpp`): add `AIR_BLOCKED_ALARM` cases to
  `alarmIDtoString()` and the `sendAlarmUSB()` title/description switch
  (Spanish/French/English, matching the existing per-alarm text pattern).
  Raised/cleared via the standard `setAlarm()`/`resetAlarm()` — default
  buzzer behavior (sound + display), same as `HUMIDITY_ALARM` today. Never
  added to `ongoingCriticalAlarm()`, `ongoingCriticalWiringAlarm()`, or
  `ongoingFanCriticalAlarm()`.
- Telemetry (`GPRS.cpp`, `Wifi_OTA.cpp`, `telemetry_keys.h`): new
  `AIR_BLOCKED_ALARM_KEY` define, and a new `case AIR_BLOCKED_ALARM:` branch
  in both `switchAlarmTelemetryGPRS()`/`switchAlarmTelemetryWIFI()` — the
  existing per-alarm telemetry switch has a `default: return;`, so a new
  alarm ID silently produces no telemetry unless explicitly added here, on
  both channels.
- **Display_HMI fix (required, not optional):** `include/main.h:290` defines
  `constexpr int MAX_ALARMS = 10;` as an independent literal, not tied to
  the shared `NUM_ALARMS`. `CommTask.cpp`'s range check
  (`alarm.id >= MAX_ALARMS`) would silently drop every `AIR_BLOCKED_ALARM`
  message, and `alarmList[MAX_ALARMS]` would have no slot for it. Change
  `MAX_ALARMS` to reference `NUM_ALARMS` directly so this class of bug
  cannot recur for the next alarm added either.
- No other Display_HMI change needed: alarm text arrives pre-formatted from
  the motherboard over `CTRL,ALM` and is displayed generically; there are no
  per-alarm-ID severity tiers, colors, or icons to extend; and
  `isFanHeaterAlarmActive()`/`update_alarm_panels()`'s fan/heater-specific
  UI gating must **not** include `AIR_BLOCKED_ALARM` (confirmed: this alarm
  must not disable temperature control).

### 5. Documentation

Add a row for `AIR_BLOCKED_ALARM` (ID 10) to `Firmware/docs/alarms.md`'s
alarm table, risk tier "Medical/Mild" (non-critical, matches
`HUMIDITY_ALARM`'s tier), trigger description: "Sustained above-normal fan
PWM duty while holding the RPM setpoint (closed-loop control), suggesting a
partially blocked air outlet."

## Error handling / edge cases

- Fan without RPM feedback, or hardware < HW16: entire feature (PID mode,
  both alarms' new logic) is inert; behavior is byte-for-byte what exists
  today.
- Fan commanded off (`in3.fanCommandedOn == false`): `fanControlPID` is
  `MANUAL`, output held at 0; neither `checkFanSpeed()` nor
  `checkAirBlockage()` evaluates (both already gate on `fanCommandedOn` /
  PID `AUTOMATIC` mode).
- Total fan failure (cannot reach setpoint even saturated): `FAN_ISSUE_ALARM`
  fires exactly as it does today; `AIR_BLOCKED_ALARM` is suppressed by the
  `!alarmOnGoing[FAN_ISSUE_ALARM]` guard, so the two never both fire for the
  same failure.
- Restart-after-crash (`in3.restoreState`) boots skip `actuatorsTest()`
  exactly as today — `fanControlPID` still engages normally once
  `turnFans()` is called during restored operation, using whatever
  `in3.fanHasSpeedFeedback` NVS carried over from the last normal boot.

## Testing

- No automated coverage: `PID.cpp`, `security.cpp`, `initHardware.cpp`, and
  `UI_actuatorsProgress.cpp` are all outside the `[env:native]` Unity filter
  on the motherboard side, and Display_HMI has no test environment at all.
  Verification is `pio run` on both `IncuNest_V17`/`IncuNest_V16` (motherboard)
  and a Display_HMI build, plus manual hardware testing.
- Manual/hardware verification required before merge:
  1. Bench-tune `fanControlPID`'s Kp/Ki/Kd and sample time on a real unit —
     confirm it holds 4000 rpm stably across the heater-on/heater-off
     transition without oscillation or excessive overshoot.
  2. Confirm the factory-baseline duty (137) reading is reproducible across
     a few clean units, and choose/confirm `FAN_DUTY_BLOCKED_THRESHOLD`'s
     margin above it from real variance data, not the ~160 placeholder.
  3. Partially obstruct the air outlet on a running unit; confirm
     `AIR_BLOCKED_ALARM` raises within the settle+hysteresis window, the
     heater and fan are *not* disabled, and the HMI displays it like any
     other alarm without touching the temperature control UI.
  4. Power on a unit with the outlet already partially blocked; confirm
     `AIR_BLOCKED_ALARM` raises on first boot (goal 3).
  5. Fully block the outlet (or stall the fan) and confirm `FAN_ISSUE_ALARM`
     still fires instead of (not in addition to) `AIR_BLOCKED_ALARM`.
  6. Confirm the corrected `MAX_ALARMS` on Display_HMI does not regress
     display of the existing 9 alarms.
