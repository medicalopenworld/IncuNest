# Alarm system — normative analysis and proposed alarm set

Analysis of the IncuNest alarm system against the standards held in
`IncuNest/Regulatory/`. All citations below were read from those documents;
nothing here is quoted from memory.

- **IEC 60601-1-8:2006+AMD1:2012+AMD2:2020** — collateral standard, alarm systems
- **IEC 60601-2-19:2020 / UNE-EN IEC 60601-2-19:2021** — particular standard, infant incubators
- **IEC 60601-2-50** — particular standard, infant phototherapy: **adds no alarm requirements**
- **ISO 80601-2-61** — pulse oximeters: out of scope, SpO₂ is on hold

Where the particular standard (2-19) and the collateral (1-8) both apply, the
particular standard wins.

## 1. The priority assignment rule

IEC 60601-1-8, 6.1.2 requires the manufacturer to assign each alarm condition
a priority **using Table 1**, and to disclose the result in the instructions
for use. Compliance is checked against the instructions for use *and the risk
management file* — so the derivation has to be written down, not just the
conclusion.

Table 1 is a 3×3 matrix of **potential result of failure to respond** against
**onset of potential harm**:

| Potential result ↓ / Onset → | Immediate | Prompt | Delayed |
|---|---|---|---|
| Death or irreversible injury | **HIGH** | **HIGH** | **MEDIUM** |
| Reversible injury | **HIGH** | **MEDIUM** | **LOW** |
| Minor injury or discomfort | **MEDIUM** | **LOW** | LOW, no alarm condition, or information signal |

The onset columns are defined in the footnotes:

- **Immediate** — the event can develop "within a period of time **not usually
  sufficient** for manual corrective action".
- **Prompt** — "within a period of time **usually sufficient** for manual
  corrective action".
- **Delayed** — "within an unspecified time greater than that given under
  Prompt".

One footnote matters a great deal for an incubator and is easy to miss:

> Where practicable, ME EQUIPMENT with a therapeutic function incorporates
> automatic safety mechanisms to prevent immediate death or irreversible
> injury caused by the ME EQUIPMENT.

IncuNest already has those automatic mechanisms — the trip-off gates that cut
the heater. Their existence is what moves several hazards out of the
*Immediate* column and into *Prompt*: the automatic cut buys the operator the
time that the *Immediate* column says they would not have. **The priority
derivation and the trip-off design are therefore coupled.** Weakening a
trip-off gate does not merely change actuator behaviour; it can push the
associated alarm back into a higher priority column.

## 2. Non-conformances found

Ordered by severity. Items 2.1 to 2.5 are requirements of the particular
standard and are not negotiable design choices.

### 2.1 A blocked air outlet must disconnect the heater

**201.12.3.101** — if the incubator has an air circulation fan, an audible,
visually identifiable alarm **shall** be given **and the supply to the heater
shall be disconnected** before a hazardous situation is created, in the event
of:

- failure of the fan to rotate,
- **blocking of the air outlets from the compartment**, and
- when possible, blocking of the air inlet.

Present state: `FAN_ISSUE_ALARM` complies. `AIR_BLOCKED_ALARM` is notify-only
and **does not** cut the heater, so it does not comply. Blockage of the air
**inlet** is not detected at all; the standard qualifies that one with "when
possible", so it needs a documented feasibility argument rather than an
implementation.

There is a second problem: `AIR_BLOCKED_ALARM` is compiled out by default
(`AIR_BLOCKED_DETECTION_ENABLED`) because `FAN_DUTY_BLOCKED_THRESHOLD` has
never been bench-calibrated. A required alarm cannot ship disabled. The
calibration is on the certification path, not optional polish.

### 2.2 Temperature deviation must switch the heater off, and is ESSENTIAL PERFORMANCE

**201.15.4.2.1 dd)** — after steady temperature conditions in an air
controlled incubator, a deviation of displayed air temperature exceeding
**±3 °C** from the control temperature shall raise an auditory and visual
alarm. **The heater shall switch off if the air temperature exceeds the
control temperature by 3 °C**, and shall remain on if below it.

**201.15.4.2.1 ee)** — for a baby controlled incubator, deviation of displayed
skin temperature exceeding **±1 °C** shall raise an auditory and visual alarm,
and **the heater shall switch off when skin temperature exceeds the control
temperature by more than 1 °C**.

**Table 201.101** lists both of these as **ESSENTIAL PERFORMANCE**. That is the
strongest classification the standard has.

Present state: `TEMPERATURE_ALARM` raises correctly — its ±1.0 °C threshold is
*stricter* than the ±3 °C the standard demands in air mode, which is allowed —
but it has **no effect on any actuator**. The required heater switch-off is
missing in both modes. Note the asymmetry the standard demands: the heater is
cut only on the **hot** side; on the cold side the alarm sounds and the heater
must keep running.

### 2.3 A thermal cut-out may not clear itself silently

**201.15.4.2.1 aa)** and **bb)** — the thermal cut-out shall be either

- non-self-resetting but capable of being manually reset, **or**
- self-resetting between 34 °C and 39 °C, **and then "the alarm shall operate
  continuously until manually reset"**.

Present state: `AIR_THERMAL_CUTOUT_ALARM` and `SKIN_THERMAL_CUTOUT_ALARM`
self-reset on a 0.2 °C hysteresis and **the alarm clears itself with them**.
Neither branch of the requirement is satisfied: the alarm must persist until a
human acknowledges it. In IEC 60601-1-8 vocabulary these must be **LATCHING
ALARM SIGNALS**, and 6.10 restricts the choice between latching and
non-latching to the responsible organization — it may not be a user setting.

The standard also constrains recovery: heater supply shall not be restored
until the cut-out is manually reset or the incubator temperature falls below
39 °C.

### 2.4 The thermal cut-out must be independent of the thermostat, and its threshold must be bounded

**201.15.4.2.1 aa)** — the cut-out "operates **independently of any
THERMOSTAT**", disconnects the heater and warns at an incubator temperature
**not exceeding 38 °C**.

Two present-state problems:

- **Independence.** The cut-out reads `ROOM_DIGITAL_TEMP_SENSOR`, the same
  sensor the control PID uses. A single sensor failure takes out both the
  thermostat and its own cut-out. This is an architectural non-conformance
  and cannot be fixed in firmware alone — it needs a second, independent
  temperature channel.
- **Bounded threshold.** `in3.airTemperatureSetMax` defaults to 38.0 °C but is
  freely writable at runtime over USB and from the WiFi `/config` page, with
  no clamp. Setting it to 45 °C is currently possible and would void the
  requirement. It must be clamped to 38 °C for air control (40 °C for baby
  control, per bb), and the "override up to 39 °C" path described in the
  standard requires a *second* cut-out at 40 °C, which does not exist.

### 2.5 Power supply interruption needs a 10-minute alarm

**201.12.3.103** — audible alarm and visible indication shall warn of
interruption of the power supply, and "shall be provided for a **minimum time
of 10 min**".

Present state: `POWER_SUPPLY_ALARM` only detects under-voltage on the 12 V
rail (`0 V < system_voltage < 8 V`) and needs the INA3221 present. There is no
mains-interruption alarm that survives the loss of mains for 10 minutes; that
requires an energy reserve dedicated to the alarm. This is a hardware
requirement, not a firmware one.

### 2.6 Skin sensor fault detection is incomplete

**201.12.3.102** — for a baby controlled incubator, an audible, visually
identifiable alarm shall sound if the skin temperature sensor connector

- becomes electrically disconnected,
- has **open-circuited** leads, or
- has **short-circuited** leads.

And the heater shall be automatically disconnected, **or** the incubator shall
switch automatically to air control at 36 °C ± 0,5 °C or an operator-set
control temperature.

Present state: detection is a 20-second staleness timeout, which catches
disconnection and open circuit but **not a short circuit** — a shorted probe
returns a plausible-looking reading and updates happily. The heater
disconnection half is satisfied. The standard also requires checking that
there is no intermediate plug position that inhibits the alarm.

### 2.7 The mute survives new alarms

**IEC 60601-1-8, 6.8.1** — "If ALARM SIGNAL inactivation applies to an
individual ALARM CONDITION or a group of ALARM CONDITIONS, the generation of
ALARM SIGNALS from other ALARM CONDITIONS shall be **unaffected**."

Present state: `alarmsMuted` is global and is cleared only when the alarm list
becomes completely empty. Silencing a humidity alarm therefore silences a
thermal cut-out that fires ten seconds later. See `alarms.md` §3.

### 2.8 The audible alarm stops on its own

**IEC 60601-1-8, 6.10** — auditory alarm signals "shall cease being generated
when" the operator has initiated an inactivation state or has reset the alarm.
Those are the only permitted causes.

Present state: the buzzer stops after roughly 250 s because
`buzzerAlarmBeepCount` runs out, with the condition still active and no
operator action.

### 2.9 Short-duration alarm conditions must still be heard

**IEC 60601-1-8, 6.10** — for an alarm condition of short duration, a medium
priority auditory signal "shall complete at least one full BURST" and a high
priority one "shall complete one half of one full BURST", unless the operator
inactivates it.

Present state: not implemented. A condition that appears and clears between
two evaluations produces a truncated or absent sound.

### 2.10 There is no alarm function test

**201.12.3.105** — means shall be provided for the operator to check the
operation of audible **and** visual alarms, and the means shall be described in
the instructions for use. Not implemented; this is a new user-facing feature.

### 2.11 Sound level is specified at 3 m, not 1 m

**201.9.6.2.1.102** — audible alarm signals shall reach at least **65 dB(A) at
3 m** perpendicular to the front of the control unit in a reflecting room. If
operator-adjustable, the floor is 50 dB(A), and the requirement applies to
every selectable frequency.

**201.9.6.2.1.103** — when any alarm sounds, the level **inside the
compartment** shall not exceed **80 dB(A)**.

Present state: unmeasured. Note this is a harder target than the "65 dBA at
1 m" figure that circulates informally — 3 m is roughly 10 dB more demanding.
The two requirements also pull in opposite directions: loud enough at 3 m
outside, quiet enough inside the compartment where the infant is. This
constrains buzzer placement and enclosure acoustics, not just drive level.

## 3. Requirements that the current and proposed design already satisfy

Worth recording, because several of these were open questions.

**Colours and flash rates — Table 2 of IEC 60601-1-8:**

| Alarm category | Indicator colour | Flashing frequency | Duty cycle |
|---|---|---|---|
| HIGH PRIORITY | Red | 1,4 Hz to 2,8 Hz | 20 % to 60 % on |
| MEDIUM PRIORITY | Yellow | 0,4 Hz to 0,8 Hz | 20 % to 60 % on |
| LOW PRIORITY | Cyan or yellow | Constant (on) | 100 % on |

The proposed 2,0 Hz / 0,66 Hz / static and 50 % duty all fall inside these
ranges. Cyan for low priority is explicitly permitted.

**Auditory bursts — Table 3:** high priority is 10 pulses with an interburst
interval of 2,5 s to 15 s; medium is 3 pulses, 2,5 s to 30 s; low is 1 or 2
pulses with an interval greater than 15 s or no repeat. Amplitude difference
between any two pulses at most 10 dB. The proposed patterns fit.

**There is no two-minute cap on silencing.** IEC 60601-1-8, 6.8.5 only requires
that the duration of AUDIO PAUSED "be disclosed in the instructions for use",
and restricts adjustment of the *maximum* interval to the responsible
organization. More usefully, **201.12.3.104** states that AUDIO PAUSED "for an
INFANT INCUBATOR warming up from COLD CONDITION **may be up to 30 min**" —
which is precisely the existing 30-minute stabilization window, and legitimises
it. The same clause requires that silenced alarms "shall have a **maintained
visual indication**" and "shall automatically resume their normal function
within a time specified by the manufacturer".

**Acknowledging may only silence.** 6.8.1: ACKNOWLEDGED "shall inactivate the
auditory ALARM SIGNALS of currently active ALARM CONDITIONS and shall not
affect the ALARM SIGNALS of inactive ALARM CONDITIONS", and "AUDIO PAUSED or
AUDIO OFF **shall not inactivate the 1 m visual ALARM SIGNALS**". They *may*
inactivate the 4 m visual signals, or cause de-escalation. The design decision
to keep the card and the on-screen indication alive while silencing audio is
correct; stopping the flashing indicator would also be permissible.

**Queued presentation of simultaneous alarms is allowed.** 6.3.2.2.2: where
several alarm conditions occur at once, each "shall be visually indicated,
either automatically **or by OPERATOR action**". The single modal with "1 of 3"
navigation complies.

**Alarm logging.** 6.12.2 makes the log optional, but if one is provided it
**shall** log the occurrence and identity of all high and medium priority alarm
conditions, and for each, the date and time, or elapsed time since occurrence,
or elapsed time from start of use. It *should* log every alarm condition with
beginning and end times, the associated alarm limits where operator-adjustable,
and where feasible the data that caused the condition.

This has a direct consequence for the planned 10-entry history: since the
thermal cut-out limits **are** operator-adjustable, the limit in force should
be stored alongside each entry, and so should the measured value that tripped
it. Two extra fields per record.

## 4. Conflict with the lock-screen-only card

**6.3.2.2.1** requires at least one visual alarm signal that indicates the
priority of the highest priority alarm condition and is perceivable at 4 m.
**6.3.2.2.2** requires at least one visual alarm signal that identifies the
**specific alarm condition and its priority**, legible at 1 m.

The agreed design shows the card only on the lock screen, leaving medium and
low priority alarms represented on all other screens by a numeric badge alone.
A count identifies neither the condition nor its priority, so on those screens
neither requirement is met. **201.12.3.104**'s "maintained visual indication"
for silenced alarms has the same problem.

The smallest change that resolves it is to make the card persistent on every
screen rather than lock-screen-only. Keeping it lock-only would require some
other always-present element that names the condition and shows its priority,
which is the same thing under another name.

## 5. Proposed alarm set and priorities

Priorities derived from Table 1. The "result" and "onset" columns are the
inputs; onset is assessed **with** the automatic heater cut in place, per the
Table 1 footnote discussed in §1.

| # | Alarm condition | Result of no response | Onset | **Priority** | Heater cut required |
|---|---|---|---|---|---|
| 1 | Air thermal cut-out (> 38 °C air controlled) | Death / irreversible | Prompt | **HIGH** | yes — required |
| 2 | Skin thermal cut-out (> 40 °C incubator temp) | Death / irreversible | Prompt | **HIGH** | yes — required |
| 3 | Air sensor fault | Death / irreversible (control *and* cut-out reference lost) | Prompt | **HIGH** | yes |
| 4 | Skin sensor fault — disconnect, open **or short** — in SKIN mode | Death / irreversible (closed-loop control lost) | Prompt | **HIGH** | yes — required |
| 5 | Skin sensor fault in AIR mode | Minor / discomfort (sensor unused) | Delayed | **LOW** | no |
| 6 | Fan failure to rotate | Death / irreversible (stratification, hot spots) | Prompt | **HIGH** | yes — required |
| 7 | Air outlet blocked | Death / irreversible | Prompt | **HIGH** | **yes — required, currently missing** |
| 8 | Power supply interruption | Death / irreversible (therapy lost) | Prompt | **HIGH** | n/a — 10 min alarm required |
| 9 | Air temperature deviation > +3 °C (AIR mode) | Reversible → irreversible | Prompt | **MEDIUM** | **yes — required, currently missing** |
| 10 | Air temperature deviation > −3 °C (AIR mode) | Reversible | Prompt | **MEDIUM** | no — heater must stay on |
| 11 | Skin temperature deviation > +1 °C (SKIN mode) | Reversible → irreversible | Prompt | **MEDIUM** | **yes — required, currently missing** |
| 12 | Skin temperature deviation > −1 °C (SKIN mode) | Reversible | Prompt | **MEDIUM** | no — heater must stay on |
| 13 | Heater fault | Reversible (infant cools) | Prompt | **MEDIUM** | yes |
| 14 | Under-voltage on 12 V rail | Reversible | Prompt | **MEDIUM** | no |
| 15 | HMI ↔ motherboard link lost | Reversible (operator blind; control continues) | Prompt | **MEDIUM** | no |
| 16 | Humidity deviation | Minor / discomfort | Delayed | **LOW** | no |

Changes relative to the current ten IDs:

- **Splitting `TEMPERATURE_ALARM` into four conditions** (9–12). The standard
  sets different thresholds per mode (±3 °C air, ±1 °C skin) and different
  actuator behaviour per direction (heater off only on the hot side). One ID
  with a mode-dependent threshold and no direction cannot express that.
- **`SKIN_SENSOR_ISSUE_ALARM` split by mode** (4 and 5) instead of one ID with
  a dynamic priority. Same outcome, but the priority becomes a static property
  of the condition, which is far easier to disclose in the instructions for use
  and to defend in the risk file.
- **New: HMI ↔ motherboard link lost** (15). Not required by either standard,
  but the operator losing all displayed values while therapy continues is a
  hazardous situation the manufacturer has chosen to control with an alarm.
- **Phototherapy timer expiry is an INFORMATION SIGNAL, not an alarm.** 6.1.2
  permits this where onset is delayed and the result of no response is
  discomfort or minor reversible injury. 60601-2-50 adds no alarm requirement.
- **`AIR_BLOCKED_ALARM` rises to HIGH with a mandatory heater cut** (7), which
  reverses the notify-only decision. This is not a matter of preference.

The resulting distribution is **7 HIGH / 7 MEDIUM / 2 LOW**, which is a far
healthier spread than the 9/1/0 previously agreed and materially reduces alarm
fatigue while being *more* defensible, because every assignment now traces to
Table 1 rather than to judgement.

## 6. What must be decided by a human

- Whether to escalate any MEDIUM to HIGH. Escalation above Table 1 is
  conservative and generally accepted, but it must be justified in the risk
  management file, not left implicit. Conditions 9 and 11 are the plausible
  candidates, since they are essential performance.
- Whether IncuNest is declared an air controlled incubator, a baby controlled
  incubator, or both — the cut-out thresholds and several requirements differ.
- The hardware items: the independent cut-out channel (§2.4), the 10-minute
  power-failure alarm reserve (§2.5), short-circuit detection on the skin probe
  (§2.6), and the acoustic targets (§2.11). None of these can be closed in
  firmware.
