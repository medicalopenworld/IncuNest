# IncuNest Alarm System

The IncuNest alarm system is designed under guidelines for life-critical risk equipment. It features redundancy, prioritizations, and exact synchronization so that no anomaly locates lasting false positives or remains improperly reported by the sound piezoelectric transducer (Buzzer) or the HMI screen.

## 1. Alarm Typologies and Hierarchies

There are **10+1 Main Alarm Identifiers**, defined by the `AlarmId` enum in
`shared/include/alarm_ids.h` and used directly as the index into
`alarmOnGoing[]` on the motherboard and `alarmList[]` on the HMI:

| ID | System Name | Origin/Main Trigger | Risk Type |
|:---|:---|:---|:---|
| 0 | *NO_ALARMS* | - | Neutral |
| 1 | `HUMIDITY_ALARM` | deviation > 10 %RH from setpoint (`HUMIDITY_ERROR`) | Medical/Mild |
| 2 | `TEMPERATURE_ALARM` | deviation > 1.0°C from setpoint (`TEMPERATURE_ERROR`)| Medical/Critical |
| 3 | `AIR_THERMAL_CUTOUT_ALARM`| Chamber measurement over `in3.airTemperatureSetMax` (default **38.0°C**) | HW Fault/Emergency |
| 4 | `SKIN_THERMAL_CUTOUT_ALARM`| Dermis measurement over `in3.skinTemperatureSetMax` (default **37.5°C**) | HW Fault/Emergency |
| 5 | `AIR_SENSOR_ISSUE_ALARM`| Sudden loss of Sensirion I2C bus or Null | HW Fault/Emergency |
| 6 | `SKIN_SENSOR_ISSUE_ALARM`| Dermis cable extracted or spurious ADC voltage | HW Fault/Emergency |
| 7 | `FAN_ISSUE_ALARM` | Loss of motor pulse (Duty Ticks encoder) | HW Fault/Emergency |
| 8 | `HEATER_ISSUE_ALARM`| Sudden drop in amperage consumed in Resistor | HW Fault/Emergency |
| 9 | `POWER_SUPPLY_ALARM`| **Under-voltage only**: `0 V < system_voltage < 8 V`, polled every 2 s (`HW_NUM >= 13`) | Elec Fault/Critical |
| 10 | `AIR_BLOCKED_ALARM` | Sustained above-normal fan PWM duty while holding the closed-loop RPM setpoint (`FAN_TARGET_RPM` = 4000 rpm) | Medical/Mild |

### 1.0 Cutout thresholds are user-adjustable

The thermal cutouts for IDs 3 and 4 are **not compile-time constants**.
`AIR_TEMPERATURE_SET_MAX` / `SKIN_TEMPERATURE_SET_MAX` are only the defaults
for `in3.airTemperatureSetMax` / `in3.skinTemperatureSetMax`, which are
persisted in NVS (`KEY_AIR_T_MAX` / `KEY_SKIN_T_MAX`) and editable at runtime
both over USB (`CommTask.cpp`) and from the WiFi `/config` page
(`Wifi_OTA.cpp`). Any figure quoted in this document is the default, not a
guaranteed trip point.

### 1.0.1 There are three trip-off gates, not one

Contrary to a single unified "critical alarm" flag, the motherboard has three
distinct gates with different scopes and different recovery behaviour:

| Gate | Effect | Members | Recovery |
|:---|:---|:---|:---|
| `ongoingCriticalAlarm()` | heater PWM forced to 0 (`PID.cpp`) | 3, 4, 5, **6 only while `controlMode == CONTROL_SKIN`**, 7, 8, 9 | automatic once cleared |
| `ongoingCriticalWiringAlarm()` | `in3.temperatureControl` switched off (`main.cpp`) | 7, 8, 9 | **manual** — the user must re-arm control |
| `ongoingFanCriticalAlarm()` | fan PWM forced to 0 (`Actuators.cpp`, `PID.cpp`) | 7, 9, and 8 only when `!in3.fanHasSpeedFeedback` | automatic once cleared |

`ongoingAlarms()` is a different thing again: it covers all ten IDs and is
used only to decide whether the buzzer should be silenced.

IDs 1, 2 and 10 have **no effect on any actuator** — they are notify-only.

### 1.0.2 The humidifier is not gated by any alarm

No alarm gate touches `in3.humidityControl`. A thermal cutout, a fan failure
or a power-supply fault will stop the heater and possibly the fan, but the
humidifier keeps injecting vapour.

The humidifier does have its own independent protection, unrelated to the
alarm system: on `HW_NUM >= 16`, `checkUsbFault()` reads the active-low
`USB_FAULT` line and, on a short-circuit or overload, turns the humidifier
off, clears `in3.humidityControl` and stops its PID. This path raises **no**
alarm ID.

### 1.0.3 `POWER_SUPPLY_ALARM` needs the current sensor to be present

The check is guarded by `digitalCurrentSensorPresent[MAIN]`. If the INA3221 is
absent or failed to probe, the alarm can never fire — the absence of the
monitor is not itself reported.

### 1.1 Fan RPM Feedback (Two Supported Fan Variants)

The motherboard PCB routes `FAN_SPEED_FEEDBACK` (tachometer input) on HW_NUM
9, 13–17, but the assembled fan may or may not have that wire connected.
At boot, `actuatorsTest()` measures RPM once the fan's current draw passes
its own check, and persists the result (`in3.fanHasSpeedFeedback`) to NVS
so it survives watchdog-reboot fast paths that skip re-running the test.

- **With RPM feedback**: `FAN_ISSUE_ALARM` is raised at boot if RPM is below
  `FAN_MIN_RPM` (3000 rpm, cleared only above 3000 + `FAN_MIN_RPM_HYSTERESIS`
  = 3300), and continuously during operation (with a 6 s spin-up grace period
  after the fan is commanded on, `FAN_SPINUP_GRACE_MS`). A heater fault
  (`HEATER_ISSUE_ALARM`) does **not** disable the fan — it keeps running and
  is independently verified by the RPM monitor.
- **Without RPM feedback**: RPM cannot be checked at all. A heater fault
  disables *both* the heater and the fan (`FAN_ISSUE_ALARM` is also raised)
  because there is no way to independently confirm the fan is still
  spinning. Recovery requires the user to power-cycle the unit — this is
  not forced automatically.

### 1.2 Closed-Loop RPM Control and Air-Blockage Detection

The motherboard implements closed-loop PID control to maintain the fan at approximately 4000 rpm on HW ≥ 16 (feedback-capable units). As part of this system, `AIR_BLOCKED_ALARM` (ID 10) is raised when the fan's PWM duty cycle remains sustained above normal (`AIR_BLOCKED_SUSTAIN_MS`, past the spin-up grace period) while holding the 4000 rpm setpoint, indicating increased static pressure from a partial air-outlet obstruction. This alarm is **notify-only** — it does not disable the heater or fan. It complements `FAN_ISSUE_ALARM` (which covers total fan failure or stall) by distinguishing partial air-outlet obstruction from transient voltage events (such as heater inrush or battery drain) that could produce similar low-RPM readings.

Detection is compile-time gated by `AIR_BLOCKED_DETECTION_ENABLED` (`board.h`), **disabled by default**: with the heater at max power the supply voltage sags and the PID legitimately raises the duty to hold the setpoint, so `FAN_DUTY_BLOCKED_THRESHOLD` must be bench-calibrated above that worst-case legitimate duty before enabling. The duty needed to hold the setpoint is always logged at boot to collect that calibration data.

The closed-loop control itself is runtime-toggleable via `in3.fanPidEnabled` (default `FAN_PID_ENABLED_DEFAULT`, editable in the WiFi `/config` page as "Fan Speed PID" and over USB with the `FAN_PID_EN` parameter). When disabled, the fan runs at the fixed `fanCtlPWM` duty with the PID bypassed — the same path a unit without RPM feedback takes — and `AIR_BLOCKED_ALARM` is not evaluated. The applied fan control duty (0-255 raw counts) is published to ThingsBoard as `fan_pwm` while the fan is running.

## 2. Life Cycle: Activation and Deactivation

### 2.1 Thresholds and Hysteresis
To prevent a minimum fluctuation or decimal drift from causing alarm "bounces" ("On-Off-On-Off" of sirens), they all possess a strict differential range or hysteresis evaluated natively local in the `securityCheck()`.

**Configured Limits:**
*   `TEMPERATURE_ERROR`: ±1.0 ºC (Reset Hysteresis: 0.05 ºC)
*   `HUMIDITY_ERROR`: ±10 %RH (Reset Hysteresis: 5 %RH)
*   `AIR_THERMAL_CUTOUT`: `in3.airTemperatureSetMax`, default 38.0 ºC (Reset Hysteresis: 0.2 ºC)
*   `SKIN_THERMAL_CUTOUT`: `in3.skinTemperatureSetMax`, default 37.5 ºC (Reset Hysteresis: 0.2 ºC)

*   **Air Cutout Example**: with the default cutout of 38.0°C, the alarm trips above 38.0°C and is only declared resolved once the measurement drops below 38.0 - 0.2 = 37.8°C. Both figures shift if the operator changes the cutout (see §1.0).

### 2.2 Time Delays and Ignitions
*   **Sensor Timeouts**: `MINIMUM_SUCCESSFULL_SENSOR_UPDATE` is set to 20000 ms (20 seconds). Under this time, an unplugged sensor triggers `SENSOR_ISSUE_ALARM`.
*   **Ignition Delays**: on cold start, and again whenever actuation is switched on, `alarmTimerStart()` resets a stabilization window so the machine does not shriek while it is still heating towards its target. The mechanics differ per alarm and are easy to misread:
    *   **IDs 1 and 2** (`HUMIDITY_ALARM`, `TEMPERATURE_ALARM`) are the only ones actually delayed. `checkAlarms()` refuses to call `evaluateAlarm()` until `ACTUATORS_ALARM_STABILIZATION_MINS` (30 min) have elapsed, so during the window they are **not evaluated at all** — not merely muted. A crash/watchdog boot that resumes state passes `RESTART_ALARM_GRACE_MINS` instead, shortening the wait.
    *   **IDs 3 and 4** (thermal cutouts) are **never delayed and never muted**. Both `initAlarms()` and `alarmTimerStart()` deliberately back-date `lastAlarmTrigger[]` for them by `-ALARM_TIME_DELAY`, which makes the delay check inside `evaluateAlarm()` fail immediately. They are armed with full audio from the first instant, which is the intended safety behaviour.
    *   **IDs 5 to 10** never pass through `evaluateAlarm()` at all — the sensor-health, fan, heater, power-supply and air-blockage checks call `setAlarm()` directly. No delay, no muting.
*   **The `SILENCED_ALARM` branch is unreachable in practice.** `evaluateAlarm()` contains a path that raises an alarm with `alarmSound = false` while inside `ALARM_TIME_DELAY`, but no alarm can currently reach it: the cutouts have their timestamp back-dated out of the window, and IDs 1–2 are not evaluated until the window has already closed. There is therefore **no such thing today as an alarm that is raised silently** — every alarm that is raised sounds the buzzer. Do not build on the assumption that a silent-raise path exists and works.
*   **No re-announcement**: nothing re-sends an alarm or re-arms its audio once the initial burst has finished (see §2.4).

### 2.3 Reactions to Local Trigger (Motherboard)
If limits are surpassed:
1. Internal logic bit is set `true` on Motherboard (`alarmOnGoing[ID] = true`).
2. The relevant trip-off gate takes effect — see §1.0.1 for which actuator each alarm actually touches. Not every alarm alters a PWM output.
3. The audio layer is invoked for the local Piezo (see §2.4).
4. The software *interrupt trigger* is dispatched (USB Serial) as `CTRL,ALM,<id>....` pushing the texts according to the active language.

### 2.4 Buzzer behaviour and its current limitations

The piezo is driven from `Buzzer.cpp` via `buzzerTone(beepTimes, timevTaskDelay, freq)`, called by `setAlarm()`. Two properties of the current implementation are worth knowing before relying on the audible signal:

*   **The audible alarm self-terminates after roughly 4 minutes.** `buzzerAlarmBeepCount` is 500 toggles at `buzzerAlarmBeepTime` = 500 ms ≈ 250 s. Once `buzzerBeeps` reaches 0, `buzzerHandler()` stops driving the output and nothing re-arms it while the alarm condition is still active. Only a *new* `setAlarm()` call restarts the sound.
*   **The tone frequency is not configurable.** The third argument of `buzzerTone()` is ignored by the function body; the actual pitch is fixed by `ledcSetup(BUZZER_PWM_CHANNEL, BUZZER_PWM_FREQUENCY, ...)` in `initHardware.cpp`. `buzzerAlarmTone` is therefore dead code, and alarms cannot currently be differentiated by pitch.

The HMI cannot substitute for this: its buzzer is a fixed-tone I2C device on the CrowPanel backlight MCU, `buzzerOn()` is deliberately commented out, and `AudioManager.cpp` is excluded from the build. The motherboard piezo is the only alarm sounder in the system.

## 3. Advanced Synchronization with HMI Display

It is extremely vital to maintain consistency between processors. Two or more alarms can occur simultaneously, or be purged by one board while the other board becomes visually obsolete if a cable is inappropriately touched for a nanosecond.

### Direct Vector to Graphical Index
With the re-architecture, LVGL uses a direct assignment. The actual `ID` returned by the board is exactly its parallel drawing place `alarmList[ID]`. Eliminates for-loops, omits repeated and inserting new alarms without array index offsets upon overflow.

### Cumulative Alarm Bitmask Correction (`0xHex`)
The HMI's `CommTask` monitors an attached special variable from the Motherboard, positionally generated with Binary Shift (`1 << alarm_id`), in every telemetry.
*   **Problem Situation**: An alarm was silenced 15 seconds ago but due to USB interruption the nurse's HMI screen continues to see it vibrating flashing red ("Visual Glitch/Phantom").
*   **Bitmasking Fix**: Every telemetry reception frame (1 time/second or 1/1Hz), the screen parses its visual cache `alarmList[id].state` and checks that the positional bit stays alive in the binary dictated by the Mother Base (e.g., reading 0x62 (1100010 base2) means alarms ID 1, 5, and 6).
*   If the HMI is exposing ID 4 locally but the base sent 'zero' for that bit, HMI auto-extinguishes the visual component of local ID 4 right away by clearing the log without using extra cycles or messages from the Base to confirm erasure ("Self Healing UI").

### Muting and Silencing Cycles
A `muteAlarm` flag sent from the HMI in its packets suppresses the piezo on the
motherboard side. It does not clear the red state or the visual insistence of
LVGL while the underlying anomaly physically persists — that part works as
described.

**The re-arming behaviour, however, does not work as previously documented.**
This section used to claim that a newly-arriving alarm forces the mute off,
"restoring the roar". It does not. `alarmsMuted` is cleared in exactly one
place (`UITask.cpp`, in `update_alarm_panels()`):

```c
if (!anyAlarm) {
  alarmsMuted = false;
}
```

That is, the mute is released only when the alarm list becomes **completely
empty**, and it is the HMI that releases it, not the motherboard. The
consequence is a real and safety-relevant behaviour: if the operator silences a
`HUMIDITY_ALARM` and a `AIR_THERMAL_CUTOUT_ALARM` fires ten seconds later, the
thermal cutout arrives **silent**, because the mute flag is still set and no
alarm has cleared in between.

This also conflicts with IEC 60601-1-8, 6.8.1, which requires that when alarm
signal inactivation applies to an individual alarm condition or a group, the
generation of alarm signals from *other* alarm conditions shall be unaffected.
A global mute that survives the arrival of a higher-priority alarm does not
meet that requirement.

The mute is also global rather than per-alarm: `muteAlarm` is a single bit in
the `HMI,...` command line, so there is no way to silence one alarm and leave
the others audible.
