# IncuNest Alarm System

The IncuNest alarm system is designed under guidelines for life-critical risk equipment. It features redundancy, prioritizations, and exact synchronization so that no anomaly locates lasting false positives or remains improperly reported by the sound piezoelectric transducer (Buzzer) or the HMI screen.

## 1. Alarm Typologies and Hierarchies

There are **10+1 Main Alarm Identifiers** in the system mapped through a vector in explicit enumerated numerical space `ALARMS_ID`:

| ID | System Name | Origin/Main Trigger | Risk Type |
|:---|:---|:---|:---|
| 0 | *NO_ALARMS* | - | Neutral |
| 1 | `HUMIDITY_ALARM` | ±12% setpoint exceeded by PID (`HUMIDITY_ERROR`) | Medical/Mild |
| 2 | `TEMPERATURE_ALARM` | ±1.0°C setpoint exceeded by PID (`TEMPERATURE_ERROR`)| Medical/Critical |
| 3 | `AIR_THERMAL_CUTOUT_ALARM`| Actual chamber measurement over 38.5°C | HW Fault/Emergency |
| 4 | `SKIN_THERMAL_CUTOUT_ALARM`| Dermis measurement over 37.5°C | HW Fault/Emergency |
| 5 | `AIR_SENSOR_ISSUE_ALARM`| Sudden loss of Sensirion I2C bus or Null | HW Fault/Emergency |
| 6 | `SKIN_SENSOR_ISSUE_ALARM`| Dermis cable extracted or spurious ADC voltage | HW Fault/Emergency |
| 7 | `FAN_ISSUE_ALARM` | Loss of motor pulse (Duty Ticks encoder) | HW Fault/Emergency |
| 8 | `HEATER_ISSUE_ALARM`| Sudden drop in amperage consumed in Resistor | HW Fault/Emergency |
| 9 | `POWER_SUPPLY_ALARM`| INA3221 detects drop or spike in PSU Vin 12V (Only on `HW_NUM >= 13`) | Elec Fault/Critical |
| 10 | `AIR_BLOCKED_ALARM` | Sustained above-normal fan PWM duty while holding the closed-loop RPM setpoint (4000 rpm) | Medical/Mild |

*Note: IDs 3 to 9 (Hardware Faults + PSU and Cutouts) are managed unified under the software flag `ongoingCriticalAlarm()` which instantly nullifies logical outputs to actuators ("Trip-Off").*

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

### 1.2 Closed-Loop RPM Control and Air-Blockage Detection

The motherboard implements closed-loop PID control to maintain the fan at approximately 4000 rpm on HW ≥ 16 (feedback-capable units). As part of this system, `AIR_BLOCKED_ALARM` (ID 10) is raised when the fan's PWM duty cycle remains sustained above normal (`AIR_BLOCKED_SUSTAIN_MS`, past the spin-up grace period) while holding the 4000 rpm setpoint, indicating increased static pressure from a partial air-outlet obstruction. This alarm is **notify-only** — it does not disable the heater or fan. It complements `FAN_ISSUE_ALARM` (which covers total fan failure or stall) by distinguishing partial air-outlet obstruction from transient voltage events (such as heater inrush or battery drain) that could produce similar low-RPM readings.

Detection is compile-time gated by `AIR_BLOCKED_DETECTION_ENABLED` (`board.h`), **disabled by default**: with the heater at max power the supply voltage sags and the PID legitimately raises the duty to hold the setpoint, so `FAN_DUTY_BLOCKED_THRESHOLD` must be bench-calibrated above that worst-case legitimate duty before enabling. The duty needed to hold the setpoint is always logged at boot to collect that calibration data.

## 2. Life Cycle: Activation and Deactivation

### 2.1 Thresholds and Hysteresis
To prevent a minimum fluctuation or decimal drift from causing alarm "bounces" ("On-Off-On-Off" of sirens), they all possess a strict differential range or hysteresis evaluated natively local in the `securityCheck()`.

**Configured Limits:**
*   `TEMPERATURE_ERROR`: ±1.0 ºC (Reset Hysteresis: 0.05 ºC)
*   `HUMIDITY_ERROR`: ±12 %RH (Reset Hysteresis: 5 %RH)
*   `AIR_THERMAL_CUTOUT`: 37.0 ºC (Reset Hysteresis: 0.2 ºC)
*   `SKIN_THERMAL_CUTOUT`: 37.5 ºC (Reset Hysteresis: 0.2 ºC)

*   **Air Cutout Example**: Critical Thermal Cutout=37.0°C. Actual units trigger at > 37.0°C. A shutdown of the active alarm requires dropping from 37.0 - 0.2 = 36.8°C to be declared remotely resolved in local loop.

### 2.2 Time Delays and Ignitions
*   **Sensor Timeouts**: `MINIMUM_SUCCESSFULL_SENSOR_UPDATE` is set to 20000 ms (20 seconds). Under this time, an unplugged sensor triggers `SENSOR_ISSUE_ALARM`.
*   **Ignition Delays**: On cold-starting the machine, alarms taking long laps of recovery towards fixed targets are temporarily silenced (e.g. `ALARM_TIME_DELAY` = 30 min) to avoid repetitive acoustic noise during initial heating.

### 2.2 Reactions to Local Trigger (Motherboard)
If limits are surpassed:
1. Internal logic bit is set `true` on Motherboard (`alarmOnGoing[ID] = true`).
2. The base PWM pulse is instantly altered through native calls (Disable resistors and coolers in Cutout).
3. The audio layer is invoked for the local Piezo.
4. The software *interrupt trigger* is dispatched (USB Serial) as `CTRL,ALM,<id>....` pushing the texts according to the active language.

## 3. Advanced Synchronization with HMI Display

It is extremely vital to maintain consistency between processors. Two or more alarms can occur simultaneously, or be purged by one board while the other board becomes visually obsolete if a cable is inappropriately touched for a nanosecond.

### Direct Vector to Graphical Index
With the re-architecture, LVGL uses a direct assignment. The actual `ID` returned by the board is exactly its parallel drawing place `alarmList[ID]`. Eliminates for-loops, omits repeated and inserting new alarms without array index offsets upon overflow.

### Cumulative Alarm Bitmask Correction (`0xHex`)
The HMI's `CommTask` monitors an attached special variable from the Motherboard, positionally generated with Binary Shift (`1 << alarm_id`), in every telemetry.
*   **Problem Situation**: An alarm was silenced 15 seconds ago but due to USB interruption the nurse's HMI screen continues to see it vibrating flashing red ("Visual Glitch/Phantom").
*   **Bitmasking Fix**: Every telemetry reception frame (1 time/second or 1/1Hz), the screen parses its visual cache `alarmList[id].state` and checks that the positional bit stays alive in the binary dictated by the Mother Base (e.g., reading 0x62 (1100010 base2) means alarms ID 1, 5, and 6).
*   If the HMI is exposing ID 4 locally but the base sent 'zero' for that bit, HMI auto-extinguishes the visual component of local ID 4 right away by clearing the log without using extra cycles or messages from the Base to confirm erasure ("Self Healing UI").

### Mutting and Silencing Cycles
A `muteAlarm` flag sent from the HMI in its packets is incorporated, suppressing the Piezo on the Main MCU side temporarily, but it does not shut down the red state and visual insistence of LVGL on the interface side if the actual anomaly (T° excess in probe) physically persists. When new alarms arrive (Trigger: "An ID that was false turned True"), the Base forces LVGL to deactivate its "mute" software variable, restoring the roar until conscious repetition of the silencer button and pre-warning by the nurse manually.
