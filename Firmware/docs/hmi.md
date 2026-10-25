# HMI Interface (Human-Machine Interface)

The visual and interactive layer of IncuNest is embedded into the **LVGL** (Light and Versatile Graphics Library) ecosystem, one of the most prolific embedded graphics libraries. The HMI is designed to be direct, highly responsive for touch control with medical gloves, and consume minimal cognitive bandwidth.

## 1. Visual Design and Layout

The main screen is partitioned into informational quadrants and side plus ceiling/floor toolbars (HUD - Heads Up Display) providing situational awareness in tenths of a second.

### Permanent HUD (Top Zone)
It continuously witnesses key and constant states on whichever screen the doctor navigates to:
*   Help button (`?`, `ui_HelpButton`) at the leftmost slot, to the left of the clock. Opens the help menu (see §6). Shown only on `ui_ScreenMain`; the `ui_ScreenLock` heading replica keeps clock and connectivity at the same x position but has no help button.
*   The "Babies" button, which with a live therapy session shows the baby's name and corrected gestational age / days of life instead of its label (see §9). `ui_ScreenLock` replicates it to the right of the connectivity indicator.
*   Connectivity icons (Server, WiFi, or Null state).
*   Environment indicator (SensorBoard units only): **door open** (door pictogram + "Open") and a qualitative **noise level** (speaker + Low/Mid/High). It sits over the free strip at the right of the clock button (`ui_ClockButton`, x=195..261 on screen, 66 px) — the time ends at x=101 and the date at x=109 of the button's 180 px, because the default LVGL theme gives the button 20 px of horizontal padding — because the rest of the heading has no room (19 px gaps) and other work reserves the Babies / Alarms / Settings slots. It is a child of the screen, not of the button, and is not clickable, so touches reach the clock button. The same indicator is replicated at the right of the padlock on `ui_ScreenLock` (`ui_HeadingEnvMain` / `ui_HeadingEnvLock`, refreshed by `sb_env_heading_update()` in `UITask.cpp` with the `clock_update()` pattern).
    *   **Never a dB or lux number.** The ICS-41350 microphone and the ALS-PT19 are not calibrated (the microphone reads ~60 dB at rest). Noise is classified on the raw scale as Low `<65`, Mid `65..74`, High `>=75`; the thresholds are **provisional** and live in one place (`SBENV_NOISE_*_DBA` in `src/modules/comm/sb_env.h`) — replace them when the microphone is calibrated. Light is received (`CTRL,ENV`) but not shown.
    *   The door is shown **only when open** (yellow). Closed is the normal state and paints nothing; a door **fault** paints nothing either, because `ALARM_SENSORBOARD_DOOR_FAULT` already signals it. The door is informational: it does not enter the thermal loop.
    *   Nothing is painted on units without SensorBoard (no `CTRL,ENV` ever arrives), when the data is older than 15 s, when the board link is lost, or when the SensorBoard link is down (`ALARM_SENSORBOARD_LINK_LOST` covers that). On `ui_ScreenMain` it is also hidden while the clock shows "No time" (the 26 pt text would overlap the strip).
    *   What is painted is decided by `sbenv_view()` (pure, host-tested in `Display_HMI/test/test_sb_env`); the LVGL drawing itself has no test environment and is verified on the bench. Protocol: `PROTOCOL.md` → `CTRL,ENV`.
*   "On battery" indicator (amber): shown while bit 7 (`ALARM_MAINS_INTERRUPTION`) of `alarmBitmask` is set, and hidden without a board link. On `ui_ScreenMain` it is **only a battery icon** (14 pt) inside the clock button, on the date row and to its right — a word did not fit without covering the date; on `ui_ScreenLock` it is the icon plus a short word in the active language, in the help-button slot, which is empty there. Refreshed by `on_battery_heading_update()` (`UITask.cpp`); no protocol field. It follows the *signalled* alarm, which the board retires once it confirms mains is back (see `alarms.md` §9).
*   Realistic icon indicators about hardware resource usage in background ("Heater", "Wind/Cooling", etc).
*   Section reserved for sliding priority alarms like marquees if they warrant overflow due to severity.

### General Sensors HUD: Cabin and Skin
*   Massive font display area for interior air vs target (AIR Temp and SET Temp).
*   Enablable parallel second group for actual/target dermis meter (SKIN Temp).
*   Relative Humidity module (Actual Hum. and Set).

## 2. Main Navigation and Tabs

Interactivity takes place in hidden windows (`LV_OBJ_FLAG_HIDDEN`) organized within the macro object of the "Base Panel", handling internal states in code or invoking pure native animations from Squareline Studio exported via C-Array:

1.  **"Main/Dash" Tab / View**: Neural center where dials oscillate, vector graphical views (Line Charts) of tracking air and skin (filled thanks to static buffers throughout time passage).
2.  **"Alarms" Tab / Alerts**: Overlaid display of active *Alarm List*. If the machine triggers, this is where long descriptions are read carrying raw variables. Features a universal top Mute button.
3.  **Config / Menu Tab**: Opened by pressing the general block/lock icon, enabled after a prolonged one-second touch period (`TouchHold()`) displacing child/accidental touch locks and opening the depths of system calibrations, Wifi, and deep Settings.
4.  **Pop-UP Numeric Settings Menus**: By pressing on a touch meter area on the main menu (Temperature, Humidity, Light), the central Arrows ('Up'/'Down') Panel pops up to manipulate the target in temporary variables and writes it back through serial to the main processor via a `Button_SAVE` (emits predefined UART Message).

## 3. State Management and Refresh in UI

In order to retain high Frame Rates (> 20fps guaranteed on SPI/HW Parallel) the LVGL implements a **Smart Refresh (Smart Diffs)** technique.

*   Texts and numeric *Labels* are not rendered every 10ms (usual loop).
*   The HMI firmware internally checks whether `valorRecibidoNuevo` is greater/less than its `valorEnLabelAnterior`. It only crops the perimeter box (*Invalidate area*) redrawing it with the base color if the value really changed ("Numeric Filter").
*   This technique de-stresses DMA by about 40-50% compared to typical continuous refresh, making the display ultra-smooth using a regular 240Mhz microcontroller.

## 4. Phototherapy Mode (UV Light)

Unlike basic thermal parameters, Phototherapy adds a temporal dimension (Countdown Timer):

*   The user sets via the panel a continuous target, or one limited by minutes ("18.00 mins").
*   Upon validation by the HMI, it commands the Motherboard. The Motherboard takes "Host/Charge" of tracking hardware clocks (`millis()`/`FreeRTOS_ticks`) in `[Minutes:Seconds]` format.
*   Motherboard sends the subtraction in packaged floating format e.g., `17.43`.
*   LVGL merely acts as a projector, parsing floating to two numerics, and pushing the transfer to the `UI_Text_Timer_Photo` Label. The 00:00 closure is automatically executed in native central MCU extinguishing native LEDs.

## 5. Language

Every visible string lives in a single catalogue, `ui/i18n_strings.def`, one
`UI_STR(id, es, en, fr, pt)` line per text. The file is expanded twice with
the same macro (X-macro): once in `ui/i18n.h` for the `ui_str_id_t` enum, once
in `ui/i18n.cpp` for the table — so id and row can never drift apart. Call
sites read `TR(STR_X)` for the active language (`g_lang`) or
`UI_StrIn(id, lang)` when the language arrives as a parameter, as in
`UI_ApplyLanguage()`. Adding a language is one extra column in the `.def` plus
one entry in `ui_lang_t`; no call site changes. Four are shipped: Spanish,
English, French and Portuguese (pt-PT), with English as the fallback when a
cell is empty.

The bundled LVGL Montserrat fonts only carry ASCII 32-126, so every
translation is written **without accents** and a `static_assert` in
`i18n.cpp` turns any non-ASCII cell into a compile error rather than an empty
box on screen.

One exception, worth knowing before trusting the sentence above: the training
courses' **content** is not in the catalogue. Those 218 paragraphs are read by
a single module and only make sense next to the step they explain, so they
live in `LessonTxt` tables inside `src/ui/training/lessons_*.cpp`, with their
own language columns and their own ASCII guard
(`LESSON_TABLE_IS_ASCII`, `ui/training/lesson_types.h`). Adding a language is
therefore *two* places: the `.def` and those tables.

The baseboard also needs to know which language the user is reading in order
to re-send the batch of alarm strings. Both converge through the initial UART
packet sent from the display.

## 6. Help

The heading `?` button (see §1) opens a modal help menu (`HelpDialog`,
`Display_HMI/src/ui/HelpDialog.cpp`) with three entries:

1. **Training courses** (`Training_OpenSelector()`,
   `Display_HMI/src/ui/training/*.cpp`, spec `hmi-training-courses`,
   `docs/adr/0002-modo-formacion-en-el-lado-hmi-del-protocolo.md`): a
   selector (`training_selector.cpp`, modal over `ui_ScreenMain`) offers two
   courses, **Nursing** and **Technician**. Picking a course asks for the
   student's name/initials (letters keyboard, up to 24 chars) or offers
   "Continue as X (n/N)" / "New student" if there is saved progress; it then
   lists the course's lessons with their state (done / pending /
   demonstration).
   - **Lesson engine** (`training_engine.cpp`, declarative `Step`/`Lesson`/
     `Course` tables, one table per lesson in `lessons_*.cpp`): a full-screen
     overlay on `lv_layer_top()` with the same spotlight (4 px amber frame,
     four dimming shades) and bubble as the old guided tour, plus a 32 px
     amber strip pinned to the bottom of the screen. Three kinds of step:
     **explain** (read and press NEXT), **do** (the student touches the real
     control through a gap in the shades sized to its actual touch area —
     the rest of the screen stays locked and dimmed; the step is marked done
     when its goal is true on the *next* UI loop pass, not on the click
     event, so an objective already met when the step starts is skipped
     automatically), and **question** with three options: a wrong answer
     shows the explanation, counts an attempt and repeats the question; a
     right one lets the student continue. A **free** step (used when a full
     assistant like `BabyWizard` must run to completion) hides the shades
     and frame and folds the bubble into the bottom strip with just the
     instruction and EXIT.
   - **Training mode** (`training_mode.{h,cpp}`, `src/state/`): while an
     *interactive* lesson runs, **actuation is real** — setpoints, toggles and
     phototherapy go to the motherBoard as in normal operation and the
     heater and lamp really switch on (the cabin is empty: the clinical gate
     requires no active therapy and no real baby) — but the **baby is
     virtual**: the profile list contains the practice baby **ZOE**
     (`seq 0xFFFF`, 32 weeks, 1500 g) and, once the student has registered a
     baby from the Babies screen during the lesson, that second practice baby
     (`seq 0xFFFE`, with the typed name and weeks; a second registration in
     the same lesson replaces it). The registration from Babies is answered
     locally with that seq; in the assistant NEW BABY and SKIP are refused
     with a toast so the student must select one of the two (registration is
     taught from Babies), and selection / weight / age are answered locally
     for both (the NTE range uses the same `shared/include/nte_table.h` the
     board uses), and so is the weight curve of a practice baby. `CommTask`
     never sends any profile frame (new, select, weight, age, discharge,
     kangaroo), time-set or WiFi credentials during a lesson, and never lets
     a practice seq out even after training ends, so neither practice baby
     reaches the board, the history or ThingsBoard (only the read-only
     archived-history queries still go out); alarm silence and alarm test
     still go out (they act on the alarm system, not on therapy). Nothing
     changed during the lesson is written to NVS (the maintenance module
     also skips its patient-swap tracking while training), and the WiFi
     CONNECT / DISCONNECT buttons are refused with a toast. Leaving the
     lesson (SALIR, an abort, or finishing it) restores the local snapshot
     (setpoints, panel, switches, dark mode, humidity, language,
     phototherapy), then `Training_Exit()` restores `hmi_msg` from the
     snapshot taken on entry **and forces its send**, so the board switches
     off whatever the lesson switched on within one `CommTask` tick; if a
     practice baby was selected (`Training_IsPracticeSeq()`) the profile the
     assistant remembered before the lesson is put back
     (`BabyWizard_SetSession()`).
   - **Clinical gate and demonstration**: an interactive lesson only starts
     in training mode if there is no therapy currently active
     (`UI_AnyControlActive()`), no active alarm, the board link is up, no
     baby profile is active and no shutdown is in progress. If the gate
     fails, the lesson runs as a **demonstration**: its "do" steps are shown
     as "explain" instead, nothing reaches the board (training mode is not
     entered) and it does **not** count as passed.
   - While training mode is active, the bottom strip reads "MODO FORMACION:
     la incubadora no recibe ordenes" (or "DEMOSTRACION..." in demo mode).
     The lesson **aborts** and restores the real state on any active alarm,
     a lost board link, a shutdown in progress, or 3 minutes without a touch
     (`HELP_IDLE_TIMEOUT_MS`, same cap as the help menu below). An abort
     leaves the clinical screen clean: the course selector is not reopened.
     The selector itself yields on the same conditions and has the same idle
     cap (`TrainingSelector_Poll()`).
   - **Progress and certificate**: passed lessons, the student name and the
     accumulated wrong-answer count are kept in NVS (`hmi_train` namespace,
     `training_progress.cpp`), written from the UI task outside
     `LVGL_Lock()` like the rest of the settings. A course counts as passed
     once every interactive lesson in it is passed (a demonstration never
     counts). Passing a course shows a certificate screen with a `mailto:`
     QR addressed to `TRAINING_EMAIL` (`Credentials_public.h`, default
     `SUPPORT_EMAIL`), subject `IncuNest SN <serial> - Certificado <curso> -
     <nombre>` and a body with date, lessons and attempts; past certificates
     (a 16-slot ring) can be listed from the selector.
   - **Content** (`openspec/changes/hmi-cursos-formacion`, tables in
     `src/ui/training/lessons_*.cpp`). Nursing, 12 lessons: E0 interface
     introduction (the old guided-tour steps, passive), E1 registering and
     following a baby (full assistant with the training baby `seq 0xFFFF`),
     E2 air temperature (one free step per assistant screen explaining why
     weight, gestational weeks and days of life are asked and how the
     proposed setpoint follows), E3 skin control and probe (needs the real
     probe; without it the SKIN step is skipped), E4 humidity, E5 safe
     phototherapy (eye-protection pop-up in a free step; the timer is
     explained, not started), E6 handling an alarm, E7 baby exit (the exit
     dialog is allowed only in that step), E8 screen lock (real padlock, tap, long press), E9
     trend from the lock screen, E10 setting the time (simulated `TIME_ACK`),
     E11 contacting support. Technician, 9 lessons: T0 intro, T1 information
     and versions, T2 WiFi and server (network buttons disabled in training),
     T3 language and modes (real language change, restored on exit), T4 time,
     T5 technical alarms and what to check, T6 firmware update via the local
     web server, T7 support report, T8 safe shutdown. Every lesson except the
     intro runs in training mode; a lesson that ends with the screen really
     locked leaves it locked and the selector reopens once back on the main
     screen. A lesson can declare an availability function (`Lesson.available`):
     skin and humidity are listed only while their option is enabled in
     Settings > Modes, and hidden lessons are neither numbered, counted nor
     required for the certificate.
   - **HMI-only**: training mode is a pure display-side sandbox. It does not
     add or change any `CTRL,`/`HMI,` message — `Firmware/PROTOCOL.md` and
     the motherBoard firmware are untouched (see ADR-0002).
2. **Video tutorial**: a QR code (`lv_qrcode`) built from
   `SUPPORT_TUTORIAL_URL` (`include/protocol/Credentials_public.h`), plus the
   same URL as plain text underneath.
3. **Contact support**: a 340 px `mailto:` QR the operator scans with their
   phone. The email opens in the phone's mail app addressed to
   `SUPPORT_EMAIL`, with subject `IncuNest SN <serial> - Solicitud de
   soporte` and a debug report in the body built by
   `support_report_build()`
   (`Display_HMI/src/modules/support/support_report.cpp`), in this order:
   identity/versions (`sn`, `hmi`, `mb`, `hw`), boot count and last reset
   reason, uptime, WiFi/IP/ThingsBoard connection status, serial link state
   and language, control mode/actuation/setpoints, current air/skin/humidity
   readings and probe state, phototherapy mode, active/silenced alarm
   bitmasks, the titles of the active alarms, and free internal heap / PSRAM.
   The operator types their question above the report and sends it from
   their own account: **the device sends nothing over the network**, so it
   works the same with or without WiFi. A NO REPORT / WITH REPORT button
   regenerates the QR without or with the report (a denser QR may not be
   readable by every phone). If the content with the report does not fit
   the QR, it falls back to recipient + subject and says so on screen.

Common rules: the help menu and the training courses (selector or a lesson,
`Training_IsOpen()`) are exempt from the 20 s auto-lock timeout while open,
but only up to `HELP_IDLE_TIMEOUT_MS` (3 min without any touch): past that
they close themselves and the normal auto-lock resumes from zero (the alarm
banner is only drawn on `ui_ScreenLock`, so a forgotten help view must never
block the way to it). Any active alarm (`UI_IsAnyAlarmActive()`, regardless
of priority) or a lost board link (`Display_IsBoardLinkLost()`) closes both
and returns to `ui_ScreenMain` — the same yield rule as `TelemetryHistory`
and the maintenance reminder (§8); an interactive lesson also aborts on a
shutdown in progress. The lesson overlay is created before the alarm banner
and the AUDIO PAUSED icon in `lv_layer_top()` and stays below them, except
that during a free step it temporarily raises above
`AlarmCenter`/`TelemetryHistory` if the lesson opens one of them, so its
bottom strip stays visible; it drops back to its original place once that
modal closes. Every text is available in ES/EN/FR (`g_lang`). The debug
report contains no patient data (no baby name or profile, no SSID, password
or token).

## 7. Hardware Test (Settings row)

The only entry point is the **Hardware test** row at the bottom of
`ui_ScreenSettings` (below Info), reachable only through the long-press lock
icon like the rest of Settings. The row is enabled only while thermal control
and phototherapy are off; otherwise it is greyed out with "Turn control off to
test". "No" on the empty-unit warning returns to Settings. While the test
screen is open the 20 s inactivity lock is suspended.

It lives in Settings rather than in the help menu or the boot splash on
purpose: the help overlays are for clinical staff and yield to any alarm,
whereas the test inhibits alarms and drives actuators open-loop; and a splash
button (tried first) was one tap away from anyone rebooting the unit and got
triggered by the GT911's phantom corner touch during init. Without a service
PIN the barrier is long-press + control off + explicit confirmation (residual
risk noted in the design).

The test screen is a full-screen overlay (`src/ui/FactoryTest.cpp`, same
pattern as `AlarmCenter`) showing a **paged 3-column grid of buttons**, one
per test, sorted FAIL → WARNING → running → PASS; skipped tests are not shown.
Pages are turned with `<` / `>` (with an `i/n` indicator) instead of
scrolling, and the page follows the running test unless the operator paged
by hand. Colour code everywhere (buttons, detail panel, verdict): **white**
running, **green** OK, **yellow** WARNING (the test could not be completed for
lack of environment: no AP, no coverage, no signal, no server, no network
time), **red** FAIL. Tapping a button opens a detail panel with a one-line
description of what the test checks, its status, the measured value and Retry
when it applies. At the bottom a **progress bar** (finished / expected tests)
and the **verdict**: `HW OK` in green when no test failed, `HW ERROR` in red
when any did or when the board did not answer, rejected the test or was lost
mid-battery. Warnings never turn the verdict red. The flow is:

0. **Entry gate**: a centred modal pop-up warning that the unit must be EMPTY
   (no patient) because actuators will be driven open-loop, with big Yes / No
   buttons; the grid and progress bar stay hidden behind it. "No" returns to
   the normal splash flow (or to Settings when opened from there). This is a
   confirmation, not an authentication.
1. **Local tests**, run inside `UI_Task` by polling: flash/PSRAM/heap, I2C
   (decided by the boot-time init results of the GT911 touch and of the
   STC8H1K28 backlight controller; the address probe of 0x14/0x30/0x18 is
   informative only, an empty `endTransmission()` is not reliable evidence
   with those chips), WiFi MAC and connection or scan, NVS write/read, and the
   UART link with the motherBoard. The panel colour, touch-target, buzzer and
   speaker tests were removed after the bench runs: they lengthened the
   battery without adding value on the current assembly line, and the audio
   ones cannot be verified without a microphone in the jig.
2. **Remote tests**: the display sends `HMI,FTEST,START` and adds a row for
   each `CTRL,FTEST` result (see `Firmware/PROTOCOL.md` § 3). When the board
   reports `WAIT` the row shows the operator instruction for that test
   ("Open and close the door", "Cover the light sensor"); when it reports
   `CONFIRM` the row shows the question and Yes/No buttons, whose answer goes
   back as `HMI,FTEST,CONFIRM` (no current test uses it: the board buzzer
   check skips itself when there is no SensorBoard microphone instead of
   asking). If the board rejects the test (control active, test already
   running) the reason is displayed. If nothing arrives within 10 s the
   section is marked "board without support". Results arrive through a
   32-entry ring drained every UI pass regardless of what is on screen; the
   ring's critical section only copies data (a log inside it once tripped the
   interrupt watchdog and rebooted the display), and any dropped result is
   counted and shown in the "Link" detail. The error / warning / OK header and
   the persisted result are always computed from the rows themselves, so they
   can never disagree with the red buttons or the verdict.
3. **Summary** header with error, warning and OK counts for both boards
   (skipped tests are not counted). Retry lives in the detail panel of each
   failed or warned button (a local test re-runs; a board test sends
   `HMI,FTEST,RUN,<id>`). **Exit** aborts a running board battery, closes the
   overlay and loads the main screen.

The result is persisted in NVS namespace `hmi_ftest` (epoch, local PASS/FAIL
masks, board counters, verdict, firmware version) for traceability. Operator
questions time out after 60 s as FAIL, a board row that does not change state
for 100 s is marked FAIL `timeout` (the board has its own cooperative 90 s
per-test timeout and should report first), 120 s without any `CTRL,FTEST*`
line closes the board section as "link lost", and the summary closes itself
after 10 min of inactivity, so an unattended unit never stays exempt from the
inactivity lock.
The display keeps sending its 1 Hz keepalive while the screen is open: the
motherBoard aborts the battery if the display goes silent for 5 s.
## 8. Maintenance reminder

The incubator reminds the staff to clean and disinfect it. The decision of
*when* lives in `src/modules/maintenance/maintenance.cpp` (no LVGL); the
pop-up that tells it, in `src/ui/MaintenanceDialog.cpp`.

The protocol has **three levels**, each with its own cadence. They are not
configurable — they are the device's cleaning protocol, not a preference. The
only thing chosen in Settings is whether the device warns at all.

| Level | Due when | Firmware condition |
|---|---|---|
| **Daily** | with a patient inside | `BabyWizard_HasLiveSession()` and 1 day since the last daily record |
| **Weekly** | every 7 days, or sooner if visibly dirty | 7 days since the last weekly record |
| **Terminal** | at patient discharge, or every 7 days for a prolonged stay | a discharge is pending, or 7 days since the last terminal record |

The levels **contain each other**: a terminal clean is deeper than a weekly
one, and a weekly deeper than a daily. So recording one level also brings the
shallower ones up to date — if you have just done the terminal clean, having
the device still ask for the daily would be noise.

"Sooner if visibly dirty" is operator judgement, not something firmware can
detect: it is guidance printed on the notice, and the DONE button is always
available so a clean done early can be recorded.

Discharge is detected from `BabyWizard_GetActiveSeq()`, the profile this HMI
put in charge: it drops to 0 when the exit dialog discharges the baby
(`BabyWizard_ClearActiveProfile()`), and changes from one seq to another when
the incubator swaps babies without going through a discharge. Both leave a
terminal clean pending, persisted in NVS so the device can be switched off in
between. Starting with the very first patient (0 → seq) does not: nobody came
out there.

The pop-up is the same card as `HelpDialog` (780x460 over `ui_ScreenMain`),
with the **`SUPPORT_TUTORIAL_URL` QR on the left** — the very code the help
menu's "Video tutorial" view shows, because the cleaning tutorials live on
that same page. On the right, one row per level with its cadence, the date of
its last record, whether it is `DUE NOW` (amber) or `up to date`, and its own
**DONE** button. Recording a level does not close the notice: it repaints with
the new dates, because the daily and the weekly fall due together every 7 days
and the operator may have done both.

The single button at the bottom right covers the two situations: **LATER**
(silences every level for 24 h) when something is due, and **CLOSE** when
nothing is — opened by hand from Settings, or everything already recorded.
There is no X: a reminder that can be dismissed without answering records
nothing.

**Settings > MAINTENANCE** holds the reminders on/off switch, the three levels
read-only with their dates, and an **OPEN REMINDER** button that raises the
pop-up itself — that is how a clean done on the operator's own initiative gets
recorded, without a second place to register the same thing. Switching the
reminders off silences all three levels: whoever turns it off does not want
some reminders and not others. The dates keep being recorded either way.

When it appears: only armed by **unlocking the screen** (and once at UI init,
because whoever powers the incubator up is standing in front of it), never in
the middle of a manoeuvre. `MaintenanceDialog_Poll()` then opens it only on
`ui_ScreenMain`, with no other modal dialog open (help, training, alarm
centre, baby wizard, exit dialog, time dialog, factory test or the Babies
screen) and no reason to yield.

Common rules, the same as the help menu: it yields to any active alarm
(`UI_IsAnyAlarmActive()`) or a lost board link (`Display_IsBoardLinkLost()`),
and it is exempt from the 20 s auto-lock with its own `MNT_IDLE_TIMEOUT_MS`
cap (3 min without any touch). That cap is measured with
`lv_disp_get_inactive_time()`, after `lv_disp_trig_activity()` at the moment it
opens. It is reliable because the auto-lock exemption in
`inactivity_timer_cb()` does **not** call `lv_disp_trig_activity()` for these
overlays: if it did, every 200 ms, the counter would be reset and the cap would
never fire. Closing by the cap does not count as an answer — the notice comes
back on the next unlock, since only pressing a button silences it.

State persists in NVS (`hmi_cfg`): `mnt_daily`, `mnt_weekly`, `mnt_term` (the
epoch of each level's last record), `mnt_en` (warn on/off), `mnt_snooze`
(epoch when LATER expires), `mnt_seq` (the last baby seen in charge) and
`mnt_tpend` (a discharge with no terminal clean recorded yet). Edge cases
handled in `Maintenance_Tick()`: with no clock yet nothing is due; the first
boot *with* a valid clock seeds the three dates with today (otherwise a
factory-fresh unit would read as "overdue since 1970"); and a clock moved
backwards re-anchors any date left in the future to today, where the interval
would otherwise never elapse again.

### 8.1 Daily weighing reminder

The weight curve is the key data point of the dashboard and used to depend on
somebody remembering to weigh. With a baby in charge and **no weighing
recorded in the last 24 h**, unlocking the screen raises a notice on the same
overlay, with the same rules for giving way (§8 above): own arm flag, only on
`ui_ScreenMain`, no other modal open (now including the Babies screen), yields
to any alarm or a lost board link, 3 min idle cap, closing without answering
does not count.

**Code layout** (three layers, only the first two testable on the host):

| File | Role | Tests |
|---|---|---|
| `modules/maintenance/weigh_reminder.{h,cpp}` | pure rule: "should it warn?" from a snapshot | `test/test_weigh_reminder` |
| `modules/maintenance/weigh_state.{h,cpp}` | state machine (discharge, baby change, deferred stamp, first-seen, future re-anchor), clock/baby/training injected through `weigh_env_t` | `test/test_weigh_state` |
| `modules/maintenance/weigh_tracker.{h,cpp}` | thin glue: NVS load/save, fills `weigh_env_t` from `HMI_GetEpochNow()`, `BabyWizard`, `Training` | manual |

**Rule** (`weigh_reminder_should_warn()`): warn when the switch is on, there is
a live session (`BabyWizard_HasLiveSession()`), the clock is synced, a
*reference* exists and `now - reference >= 24 h`, and no LATER is in force. The
reference is, in this order: the epoch of the last weighing stamped **for that
same `seq`**; else the moment the HMI first saw that `seq` in charge
("seen", persisted). No clock, no reference, or a reference in the future: no
warning. Nothing is invented from `millis()`, which does not survive a restart.

**Stamp.** The protocol has no epoch for the last weighing
(`CTRL,WEIGHT_HISTORY` gives only days relative to admission), so the HMI
stamps it itself. Only a real weight stamps: `SKIP` (0 g) does not. When it is
stamped depends on who sends it, on purpose:

- **Wizard (admission and re-activation weights) and Babies screen** do not
  wait for any reply, so they stamp **on send**, inside
  `Communication_SendProfileWeight()` (default `stampOnSend=true`). If the frame
  is lost the reminder fires once too often, never too late.
- **WEIGH NOW** (the notice's own keypad) **waits for the board**. The only
  reply to `HMI,PROFILE_WEIGHT` is `CTRL,PROFILE_RANGE` (`PROTOCOL.md`), so the
  dialog sends with `stampOnSend=false` and shows "Recording the weight..." for
  up to 3 s. It stamps, cancels LATER and shows "Weight recorded" only when
  a `PROFILE_RANGE` for the same `seq` arrives. Without it: error toast, **no
  stamp**, back to the notice so the operator can retry. A lost link would
  otherwise silence the reminder for 24 h with a weight that never arrived. The
  one case that stamps without confirmation does not exist in the dialog; the
  caveat is the opposite one: the board may have received a weight we did not
  see confirmed, so a retry can send the same value twice (the board dedups
  unchanged weights).

A weight for a baby who is **not** in charge (registering a baby from the
Babies screen) is ignored: there is a **single stamp slot**, kept for the baby in
charge. Consequence: if baby A is replaced by baby B and later A comes back,
A's stamp was dropped when B took over, so A's deadline counts from "seen"
(the moment the HMI saw A in charge again), not from A's last real weighing.
In training nothing is written (ADR-0002); `weigh_state_tick()` also returns
first thing in training. With no clock at the instant of a weighing the stamp
is deferred until there is one and nothing is warned meanwhile.

**Lifecycle** (`weigh_state_tick()`): discharge (`BabyWizard_GetActiveSeq()` →
0) clears the stamp, "seen" and LATER; a different baby in charge drops
whatever is not that baby's (and LATER, unless that baby was already known, e.g.
after an HMI restart); a stored date more than 60 s ahead of the clock
(`WEIGH_FUTURE_TOLERANCE_SECS`) is re-anchored to today (smaller differences
are left alone to avoid pointless NVS writes). The identifier of the live
session lives in RAM, so after an HMI restart there is no warning until the
wizard identifies the baby again; the NVS stamp is recognised when the same
`seq` returns.

**UI** (`ui/MaintenanceDialog.cpp`, `Mode::WeighPrompt` / `WeighEntry` /
`WeighWaiting`): the card shows "<Name>: not weighed for N h", a hint, **WEIGH
NOW** and **LATER** (24 h, only if there is a clock). WEIGH NOW swaps the
content for a digit keypad (400-5000 g, same range as the wizard, empty on
purpose); REGISTER sends the weight and waits as described above.

Arming and chaining: both notices are armed by the same unlock and are
independent flags. The cleaning notice goes first and, once **answered**, the
weighing one follows without loops. A cleaning notice closed *without* answering
(idle cap, screen change) does not chain the weighing one; closed by an alarm or
lost link it **re-arms itself** so it returns when the cause ends (the weighing
notice does the same). The weighing flag is consumed the first time it is
evaluated at an openable moment, due or not, so the notice is "on unlocking",
not mid-use when the 24 h happen to elapse; but it is **not consumed while no
baby is in charge** (`BabyWizard_GetActiveSeq() == 0`, e.g. at power-up), so the
first unlock after the baby is identified does warn.

Orphan replies: a `PROFILE_RANGE` that arrives after the dialog gave up
(timeout, closed by an alarm) is dropped within a 3 s window. If the wizard,
the Babies screen or a lesson opens in that window, the window is cancelled
**and `g_pendingProfileRange` is cleared** at once: the straggler is for the same
baby and the newcomer would adopt it as the answer to its own weight (same rule
as `BabyHistory::discardPendingReplies()`).

**Settings > MAINTENANCE** has a second switch, **WEIGHING REMINDER**, under the
cleaning one (`wgh_en`, **ON by default**, also on a fresh NVS: it is a
clinical prompt, unlike `mnt_en`). Off silences the notice but stamps keep being
recorded; turning it on again clears an old LATER. In training the switch is
rejected with the standard toast and snaps back (it writes NVS).

NVS (`hmi_cfg`, epochs in UTC, written outside `LVGL_Lock` by
`WeighTracker_Flush()` after `LVGL_Unlock()` in `UI_Task`): `wgh_seq` and
`wgh_last` (baby and epoch of the last real weighing), `wgh_sseq` and
`wgh_sfirst` (baby and epoch of when the HMI first saw it in charge),
`wgh_snooze` (epoch when LATER expires) and `wgh_en` (switch). If
`WeighTracker_Init()` could not open the NVS (it retries read-write, which also
creates the namespace on a fresh unit) nothing is ever written that session
(`s_loaded`), so default zeros do not overwrite good values. Strings:
`STR_WEIGH_*` in `i18n_strings.def`, in the four languages.

**Bench procedure** (no automated test covers the UI, NVS or board link; the rule
and the state machine are covered on the host, `pio test -e native`). Needs a
unit with a synced clock and a baby admitted from the wizard; read the `WGH` log
lines on the HMI serial port.

1. *Stamp on admission:* admit a baby with a real weight; `WGH` logs
   `bebe al mando` and `pesada: ... sellada`; after a restart, `init:` shows
   `weigh seq/at` set. With NO WEIGHT (SKIP) only `seen` is set.
2. *Due notice:* with a stamp older than 24 h (set the clock forward or wait),
   lock and unlock: the notice shows the name and hours. LATER: it does not
   return on the next unlocks for 24 h.
3. *WEIGH NOW with the link up:* type a value, REGISTER: "Recording..." for
   under a second, then "Weight recorded"; `wgh_last` updates and the notice does
   not return. Out-of-range (300, 6000): toast, nothing sent.
4. *WEIGH NOW with the link down* (unplug the board UART): after 3 s an error
   toast, back to the notice, `wgh_last` unchanged. Plug it back: no stale
   `PROFILE_RANGE` is adopted by the wizard (open it right after).
5. *Giving way:* with the notice open raise an alarm: it closes and, for the
   cleaning notice, comes back when the alarm ends. Leave it idle 3 min: it
   closes and the weighing notice does not follow. Open the Babies screen with
   the notice due: it waits.
6. *Power-up:* reboot with a baby already controlled; no notice until the wizard
   identifies the baby, and then the first unlock does warn.
7. *Switch:* Settings > Maintenance > WEIGHING REMINDER off: no notice; on again
   (and ON on a factory-fresh NVS). In a lesson: toast and the switch snaps back.
8. *Discharge:* discharge the baby; `wgh_*` all 0.
9. *Layout and languages:* the Maintenance panel with its two switches, three
   level lines, button and hint fits the card in Spanish, English, French and
   Portuguese without clipping.
## 9. Backlight and night mode

The backlight is the **STC8H1K28** at I2C `0x30` (the same chip that drives
the display's buzzer and speaker): one byte per transaction, no register.
`0` is full brightness, `245` is off, and `246..249` are the buzzer/speaker
opcodes, which are **never** written as brightness. The wire is
`src/drivers/backlight.{h,cpp}` (`Backlight_SetPercent()`, `Backlight_Off()`);
*what* level is wanted is decided by `src/modules/brightness/brightness_policy.cpp`,
which has no Arduino or LVGL and is covered by `pio test -e native`
(`test/test_brightness_policy`).

**Settings > Modes** has two rows below the humidity switch: a **BRIGHTNESS**
slider (20-100 %, default 100 %) and a **NIGHT MODE** switch (default off).
The slider applies live while dragging and writes NVS when released.

Target level, first matching row wins:

| Alarm active (or board link lost) | Night mode | Lock screen loaded | Idle (no touch) | Target |
|---|---|---|---|---|
| yes | any | any | any | **100 %** |
| no | on | yes | >= 30 s | `min(user level, 20 %)` |
| no | any other combination | | | user level |

Idle time is LVGL's (`lv_disp_get_inactive_time()`), so any touch, on the lock
screen or the unlock pop-up, brings the screen back to the user level on the
next `UI_Task` pass. The auto-lock fires at 20 s, so night mode dims about 10 s
after an automatic lock and 30 s after a manual one. A screen is never dimmed
with an alarm active, silenced or not. The power-off countdown (`pwroff`) is
outside the policy: it still writes byte `245` and halts.

The percentage maps to the byte with a quadratic duty curve, clamped to
`[10, 240]` (`Brightness_PercentToByte()`): 100 % = 10 (the byte the firmware
has always used), 80 % = 95, 60 % = 160, 40 % = 207, 20 % = 236. These points
are a hypothesis until calibrated on the bench.

I2C is done by `UI_Task` **after** `LVGL_Unlock()` (ARQ-LOCK-001): the level is
computed under the lock and written outside it, only when it changes. A write
the chip does not ACK is not recorded as applied and is retried every 200 ms,
except when the target is 100 % (alarm or link lost), which is retried on every
pass with no throttle.

> **Not verified on hardware: shipped disabled.** That the STC8H1K28 accepts
> intermediate bytes is an assumption from its v1.3 datasheet comment; until
> now only `10` and `245` had ever been written. `DISPLAY_BL_LEVELS_SUPPORTED`
> in `include/config/display_config.h` is therefore **`0` by default**: every
> level writes the full-on byte, so the slider and night mode work in the UI and
> NVS but leave the hardware at full brightness. A chip that ignored or
> misread intermediate bytes would otherwise leave a persisted 20 % as a nearly
> black screen on every boot. **Set it to `1` only after the bench checks
> below pass.**

**I2C bus `0x30` has no lock.** Brightness, buzzer and speaker opcodes share
that address and each is a separate one-byte transaction with no mutex. It is
safe today only because every writer runs in `UI_Task`
(`AudioManager::begin()`/`playTone()` are not started). Re-enabling
`AudioManager` would add a second writer and a brightness/opcode race (an
opcode interleaved into a brightness transaction): add a bus lock first.

Bench checklist (required before setting the flag to `1`):

- Sweep bytes `10 / 100 / 150 / 236` and confirm monotonic brightness, no
  blanking, no buzzer, no hang.
- Night mode with an alarm raised while dimmed: 100 % at once, dims again when
  it clears.
- Simulated NACK: the level is not recorded as applied and is retried.
- Training course: changes are not persisted and are restored on exit.
- `pwroff`: the screen turns fully off.

NVS (`hmi_cfg`): `bl_pct` (uchar, 20-100, default 100; clamped on load) and
`bl_night` (uchar 0/1, default 0). Absent keys on units already deployed read
as the defaults, which reproduce the previous behaviour byte for byte. Writes
go through the deferred block of `UI_Task`, outside `LVGL_Lock`, and are
postponed while a training course is active; the training snapshot restores the
brightness and night mode on exit.
## 10. Patient identity and accumulated therapy

Two things the operator used to need a dialog for are now always on screen
while a therapy is running: **whose baby this is**, and **how much of each
therapy that baby has already had**. The first closes the "wrong baby" risk
(two identical incubators, one therapy running, nothing on the main screen
saying who it belongs to). The pure logic is in
`src/modules/patient/patient_session.{h,cpp}` (no Arduino, no LVGL, covered by
`test/test_patient_session`, 54 cases in `[env:native]`); the live state and
NVS in `src/state/patient_state.{h,cpp}`; the widgets in `ElementsCreation.cpp`
and the two refresh functions `patient_identity_update()` /
`patient_therapy_update()` in `UITask.cpp`, which run every pass next to
`clock_update()`.

**When it shows.** Only while `BabyWizard_HasLiveSession()` is true (a known
baby *and* some therapy running). With the incubator idle the remembered `seq`
is history, not "who is inside", and another baby's name next to an empty
cabin is exactly the error this feature exists to prevent.

### Identity (heading)

The Main heading has no free slot (help 20-64, clock 83-263, connectivity
282-322, lock 341-459, Babies 468-578, alarm 622-670, settings 744-794 on the
800 px width), so the identity lives **inside the "Babies" button**: with a
live session its label is hidden and two lines appear, the name (14 pt; it
scrolls circularly if it does not fit, rather than being cut with "...") and
the detail (12 pt, truncated with dots if it does not fit; days of life are capped at `99+`). Without a session it reads "Babies" again. Nothing moves
and the button still opens the baby viewer. `ui_ScreenLock` has no buttons to
the right of the connectivity indicator, so its replica goes there in full
(`ui_LockPatientName` 20 pt, `ui_LockPatientDetail` 14 pt, x = 350).

The detail is one of three catalogue strings (`STR_PATIENT_AGE_FMT`,
`STR_PATIENT_GEST_ONLY_FMT`, `STR_PATIENT_DAYS_ONLY_FMT`):
`EGC 34+5 - 5d` (corrected gestational age, weeks+days, and days of life; the
acronym is `CGA`/`AGC`/`IGC` in en/fr/pt), `EG 34 sem` when the age is unknown,
or `5d de vida` when only the weeks are unknown. A corrected age needs both.

**Days of life are computed on the HMI.** The motherBoard sends no birth date
or admission epoch, only `CTRL,PROFILE_RANGE.ageDays` at the moment of the
answer. `BabyWizard_Poll` now hands it, with `ageKnown`, to
`PatientState_OnAge()`, which stores `(ageDays, HMI_GetEpochNow())`; the days
shown are `ageDays + floor((now - capture) / 86400)`. **The age is unknown**
(no days of life, no corrected age) when there was no clock at capture, when
there is none now, or when more than 365 days have elapsed: without a clock
nobody can say how many days passed, and a counter frozen at the value from
hours ago is worse than none. The error against the board's day boundary is at most one day, because the
count starts at the capture and not at admission. A baby picked only for a
lamp or humidifier (`targetIsIdentityOnly`) never goes through the weight/age
screens, so it has no age until another wizard run captures one.

### Accumulated therapy (panel headers)

Each of the three panel headers (temperature, humidity, phototherapy) carries
a one-line 12 pt label with no caption, `3h 20m` (`patient_fmt_minutes()`, the
same format the baby viewer already used; its private copy was moved there),
**under the title** and aligned like it: left in humidity and phototherapy
(titles at x = 20), right in temperature (title at x = -20). A first version
put a two-line `Total / 3h 20m` between the title and the toggle button, and
`100h 5m` under the French `PHOTOTHERAPIE` overlapped the title: there is no
reliable horizontal gap. Vertically there is: the headers are 50-54 px and a
14 pt title takes about 16 centred, so about 14 px are free below it. The label
is hidden unless the identity is confirmed and the minutes are known (see
below). `ui_HumidityLabel` sits at x = 20, y = -98, the same row as its button
and background, so the hum label hangs 16 px below that row. Fit with `100h 5m`
in the four languages is a bench item.

Minutes per baby reach the HMI only in `CTRL,PROFILE_LIST`, on demand, and
`g_pendingProfileList` has **a single consumer** (wizard or baby viewer), so
Main must not poll for it. Instead:

1. **Local count at 1 Hz** from the real `CTRL,STATE`, with the board's rule
   (temperature counts `act` 1|3, humidity `act` 2|3, phototherapy `photo`
   non-zero; see `PROTOCOL.md`). A pass credits at most 5 s, and nothing is
   credited while the board link is lost, in training mode, or with no baby.
2. **Seed** at `CTRL,PROFILE_ACK`: the minutes of that baby in the list the
   wizard just fetched, or 0 for a new one. If the same baby was already being
   counted, the local value is kept (it is fresher).
3. **Resync** on every `CTRL,PROFILE_LIST`, whoever consumes it: the parser
   also copies it into a read-only mirror with a generation counter
   (`Communication_TakeProfileListMirror()`, a `portMUX` that only copies
   data) without touching the flag. A counter that is running keeps the larger
   of the two values (the board flushes to NVS every 10 min, so its figure
   lags); one that is stopped takes the board's.
4. **Persist** every 60 s if it changed, and once when everything stops.

### NVS keys (`hmi_cfg`)

| Key | Content | Size |
|---|---|---|
| `pat_id` | version **2**, `seq`, name[24], gestational weeks, `ageDays`, capture epoch, board `sn` | 44 B |
| `pat_ther` | version, `seq`, seconds of phototherapy / thermo / humidity | 20 B |

Each is **one blob**, not one key per field: NVS is not transactional, and
separate keys would let a power cut leave one baby's name next to another's
minutes (same reasoning as `rtc_tz`). Serialization is byte by byte,
little-endian, in `patient_session.cpp` (no `memcpy` of a struct, whose padding
is compiler-defined); a record with an unknown version, wrong size, `seq` 0,
an unterminated name or gestational weeks outside 0 / 20-44 is discarded.
**Version 1 (40 B, no `sn`) is discarded and its keys erased**: it cannot be
tied to any board. `pat_ther` is accepted only if its `seq` is the restored
identity's, and without it the minutes stay **hidden** (and uncounted) until a
`PROFILE_LIST` seeds them (`PatientAccum.known`). Both keys are removed by
`BabyWizard_ClearActiveProfile()` (discharge) and by a `PROFILE_ACK` of 0 in
answer to a `PROFILE_SELECT` (the board no longer knows that baby). A `0` in
answer to a rejected `PROFILE_NEW` (board full) says nothing about the baby
still inside and touches nothing.

### Confirmation against the board

`PatientState_Restore()` runs at UI task start, **before** `Maintenance_Init()`,
and puts the identity back with `BabyWizard_SetSession()`: the board keeps the
active baby and resumes its therapies after an HMI restart. But a restored
identity is only what *this HMI* once saw, so it is born **unconfirmed** and is
not painted as true. A `seq` identifies a baby only inside one board (two boards
both start at 1), the board's NVS may have been wiped, and the discharge may
have been done by another route; showing the wrong baby is the very error this
feature exists to prevent.

The board confirms with `activeSeq`, field 24 of `CTRL,STATE`
(`babyStore_getActiveSeq()`, 0 = nobody; see `PROTOCOL.md`). Decision table
(`patient_confirm_decide()`, host-tested), once the first `CTRL,STATE` arrived:

| Situation | Result |
|---|---|
| board `sn` is not the stored one | discard |
| `activeSeq == seq` | confirmed |
| `activeSeq != 0` and `!= seq` | discard |
| `activeSeq == 0` and the board reports a therapy | discard |
| `activeSeq == 0` and no therapy | wait (nothing is painted anyway) |
| the board does not send the field | check the list |

An identity that comes from a `PROFILE_ACK` of the wizard is born confirmed.
**While unconfirmed nothing is shown** (no identity, no minutes), and the wizard
does not take the `selectExisting` shortcut: it offers the picker. With an old
board that does not send `activeSeq`, the HMI asks for `PROFILE_LIST` (at most
three times, 10 s apart, never while the wizard or the baby viewer is open) and,
if the baby is there with the same `seq`, name and weeks, paints the name **grey
with a trailing "?"** and no age or minutes; otherwise it discards it. The
list's reply lowers `g_pendingProfileList` only when it was this request's and
nobody else is waiting for one. `activeSeq` and the therapy flag are published
by `CommTask` as one snapshot (`Communication_GetBoardView()`), because reading
them one by one from `ctrl_state_msg` could pair this message's therapy with the
previous message's `activeSeq` and discard a valid baby.

Minutes are credited only if the board sends `activeSeq` and it equals the
counter's `seq` (the board credits the active `seq` and, with 0, nobody), or,
when it does not send it, only while the identity is confirmed. The same `seq`
with another name or weeks is a **different baby** (the board reassigned the
`seq`): age and counters are reset.

**`BabyWizard_HasLiveSession()` now requires a confirmed identity** (semantics
change, documented in `BabyWizard.h`). Every path that acts on the `seq` towards
the board goes through it, so requiring confirmation there closes them all at
once: `BabyExitDialog` (a `PROFILE_DISCHARGE` would archive another baby's
profile; it also has its own direct `PatientState_IsConfirmed()` guard in
`BabyExitDialog_Tick`), `BabyHistory`'s `afterRegistered` (re-sends
`PROFILE_SELECT`), the picker shortcut, and the phototherapy/humidity wizard
gating in `UITask`. Any other branch that calls it (a weighing reminder, say)
inherits that: until the board confirms, it sees "no live session". Two callers
need "a patient is inside, whoever it is" and use the explicit
`BabyWizard_HasAnySession()` instead: the daily-cleaning reminder and the
painting of an unconfirmed identity grey. The rule itself is
`patient_live_session()`, host-tested.

Dropping an identity calls `BabyWizard_ClearActiveProfile()`, which takes the
active `seq` to 0. That is **not a discharge** when the identity was never
confirmed: `Maintenance_NoteSeqDropped()` tells `Maintenance_Tick()`, and
`mnt_seq_decide()` (pure, host-tested) does not mark a terminal clean pending
(which would have been wrong and persisted). It excuses one transition, and only
`seq -> 0` of the very `seq` that was dropped. A drop is **deferred while the
baby wizard is open**: its in-progress baby lives in `s_seq`/`s_name`, which
`ClearActiveProfile()` would zero mid-registration; the drop is applied once it
closes, or cancelled if the wizard itself confirms or replaces the identity.

Details of the confirmation, each host-tested where it is pure:

*   After confirming by `activeSeq`, name and weeks are contrasted with the next
    `PROFILE_LIST`: a `seq` can be reused after `babyStore_wipeAll()`. A mismatch
    drops the identity.
*   A serial number of 0 (in the record or on the board) cannot be verified and
    is not "another board": `activeSeq` alone does not confirm, and the list
    contrast is mandatory (`patient_confirm_requires_list()`).
*   The HMI's own `PROFILE_LIST` request expires after 10 s without an answer and
    is retried (3 attempts at most), `patient_list_request_due()`; before, a lost
    reply blocked every retry forever.
*   `s_legacyBoard` is re-read on every pass from the board snapshot, and with
    the link lost nothing is decided: the last `CTRL,STATE` is stale.
*   Residual risks (wipe, `sn == 0`, no checksum on a last-field `%u`): see
    `known_issues.md` #24.

All NVS access is in `PatientState_Service()` / `Restore()`, **outside**
`LVGL_Lock` (ARQ-LOCK-001); the `OnXxx` hooks the wizard calls under the lock
only set flags. Nothing from a lesson reaches this state: practice babies
(`Training_IsPracticeSeq()`) and `Training_IsActive()` are filtered at every
entry point.

Not covered by host tests (manual, on the bench): the widgets and their fit,
reading the board snapshot, the mirror in the parser, the single list request,
the grey "?" rendering and the restore after a
real restart. See the checklist in
the `hmi-patient-identity` and `shared-patient-identity-confirm` changes (archived under `openspec/changes/archive/`).
## 11. Telemetry trend (24 h, persistent)

`TelemetryHistory` (`Display_HMI/src/ui/TelemetryHistory.cpp`) is a full-screen
overlay on `lv_layer_top()` with three stacked charts — air, skin, humidity —
for the last **4 h** (default), **12 h** or **24 h**. The X axis is always the
whole window with "now" on the right edge; five hour labels sit under the
humidity chart (local `HH:MM` once the board has sent time and time zone,
otherwise `-h:mm` back from now).

**Entry point.** The **TREND** button in the header of the alarm centre
(`AlarmCenter`, to the right of TEST). The alarm centre is one tap away from
Main (alarm icon or the "all OK" check) and from the lock screen (alarm icon),
and the trend opens *on top of it*: closing it, or the trend yielding to an
alarm, lands back in the alarm centre. The button is greyed out while an alarm
is active or the board link is lost, because the trend yields to **any** active
alarm and to a lost link (the banner and AUDIO PAUSED icon must stay visible —
same rule as the help menu). The Main chart button (`ui_ChartButton`, hidden for
years, and overlapping the Babies button) and the old `ui_ScreenCharts` are
unreachable and unused; the lock-screen TREND button stays removed (production
batch of 2026-09-17).

**Markers.** On each chart: the **setpoint** as a purple line (a second series,
drawn under the measurement) and one **vertical mark per alarm edge**: a solid
line in the priority colour when the condition appears (high red, medium
orange, low dark yellow), a dashed green line when it clears. A legend in the
header explains them. Edges are stamped in `processReceivedAlarm()` (CommTask)
only on a state change, never on a re-sent `CTRL,ALM`; CommTask only enqueues
them (16-entry queue under a critical section) and `UI_Task` drains the queue
stamping each with the *instant of the edge*. An alarm already active when the
HMI boots is stamped when the HMI learns of it. A setpoint is recorded only for
what is under control according to the board's `CTRL,STATE` echo (air setpoint:
temperature control in air mode; skin: temperature control in skin mode;
humidity: humidity control); otherwise it is "no data" and no line is drawn.

### Data model

All of it lives in `Display_HMI/src/modules/trend/` (pure C++17, no LVGL or
Arduino, host-tested in `[env:native]`):

| Module | Role |
|---|---|
| `trend_store` | ring of 8640 slots (10 s each) × 6 series (`int16`, tenths: air, skin, humidity, and the three setpoints); a gap between two samples is filled with "no data" in all six; `INT16_MIN` = no data |
| `trend_events` | ring of the last 64 alarm edges (id, priority, raised/cleared, slot) |
| `trend_clock` | slot = `esp_timer`/10 s + offset; with time, slot = `epoch/10` |
| `trend_decimate` | window plan and min/max decimation |
| `trend_file` | `/trend.bin` format, CRC-32, restore |
| `trend_model` | store + events + clock |
| `trend_policy` | dump policy, setpoint validity, marker classification |
| `trend_persist` | glue with real I/O (`hmi_fs` + PSRAM): restore and chunked dump |

The whole model is **one PSRAM block** of ~104.8 KB allocated once in
`TelemetryHistory_Init()` (`heap_caps_malloc(MALLOC_CAP_SPIRAM)`); only the
16-entry edge queue (256 B) lives in internal `.bss`. If there is no PSRAM the
trend records nothing and the unit monitors as usual. The model belongs to
`UI_Task`; no other task touches it.

**Time base.** Before the board sends time the slot is `esp_timer`-based. At the
first valid time, `trend_clock_update()` returns the offset and the ring and
the events are *relabelled* by that many slots (their relative spacing is
kept — nothing is lost or duplicated). Later corrections of more than one slot
(NTP, manual set) shift everything the same way, forwards or backwards; a
difference of one slot or less is `millis()`/`esp_timer` rounding and is
ignored. The time zone plays no part: slots are UTC and local time is applied
only when formatting labels.

**Windows and decimation.** LVGL cannot draw 8640 useful points on ~670 px. The
charts always have 1442 points = 721 buckets × (min, max) of 2 / 6 / 12 slots
for 4 / 12 / 24 h, emitted in the order they happened so the trace keeps its
shape and isolated peaks survive. Bucket boundaries are aligned to *absolute*
multiples of the bucket size (otherwise every new slot would shift them and the
min/max would flicker); the cost is a partial bucket on the right, at most
2 minutes at 24 h. A bucket with no data cuts the line. Grid: every 15 min (4 h),
1 h (12 h), 2 h (24 h).

### Storage (`hmi_fs`) and `/trend.bin`

`Display_HMI/src/modules/storage/hmi_fs.{h,cpp}` mounts **LittleFS** on the
existing `spiffs` partition (`partitions/hmi_16mb_ota.csv`, 0xA10000, 0x5E0000,
≈5.9 MB) by label — the partition table is not touched. API:
`HmiFs_Begin()`, `HmiFs_State()`, `HmiFs_Service()`, `HmiFs_IsMounted()`,
`HmiFs_Info()`, `HmiFs_FileSize()`, `HmiFs_ReadAll()`, `HmiFs_Remove()`,
`HmiFsReader` (incremental read), `HmiFsAtomicWriter` (`begin` / `write` /
`commit` / `abort`: writes `path.tmp` and renames it over `path`) and
`HmiFs_WriteAtomic()`. Rules: never under `LVGL_Lock` (ARQ-LOCK-001); mount from
`UI_Task` after the RGB panel has reserved its bounce buffers (it is mounted
lazily the first time the trend needs it, not in `setup()`); a flash write
suspends the cache and can flicker the panel briefly (see the note in
`platformio.ini`), so large files are written in 4 KB chunks.

**Semantics change — `HmiFs_Begin()` no longer formats on the spot** (code
review C1). In the field the 6 MB `spiffs` partition is blank or carries an old
SPIFFS image, so the first mount fails; formatting there could erase the whole
partition in one go and freeze `UI_Task` for tens of seconds with no banner and
no alarm sound. Now `HmiFs_Begin()` only *tries* to mount (`begin(false)`) and
returns the result; if there is no valid image the state becomes
`HmiFsState::NeedsFormat` and `Begin()` returns `false` (it does not retry the
mount, which would spam the VFS log). The format is **deferred** and spread by
`HmiFs_Service(ioAllowed, millis())`, called every `UI_Task` pass: only with no
active alarm and a live board link, it erases the partition in **64 KB ranges**
(`esp_partition_erase_range`, starting at offset 0 where the superblocks live —
a power cut mid-way leaves a "blank" partition and the next boot restarts the
format), one range per pass and ≥ 100 ms apart (94 ranges, ~10 s of spacing plus
the erase time), then `LittleFS.format()` on the already-erased partition and a
mount, logging the total time (and the erase/`format()` split). Until it ends
`Begin()` returns `false` and **the trend works in RAM only**; callers must treat
`false` as "no persistence yet" and call `Begin()` again later. `Failed` is
final (no format loop). `LittleFS.format()` on an erased partition only writes
the superblocks; that is to be confirmed on the bench (checklist 2.7) — if it
turns out slow, the plan is to drop it and use `begin(true)` on the erased
partition. Production alternative: flash an **empty LittleFS image** at 0xA10000
with the flasher so the field never formats (pending; the flasher is not
touched by this change). The public API of `hmi_fs` is otherwise unchanged:
`HmiFs_State()`, `HmiFs_Service()` and `HmiFsReader` are additions.

`/trend.bin` (little-endian, written byte by byte, no packed structs):

```
off  size  field
  0     4  magic "ITRN"
  4     2  version = 1
  6     2  headerSize = 32
  8     4  slotSeconds = 10
 12     4  count          (slots stored, 1..)
 16     4  lastEpoch      (UTC s at the start of the last slot = lastSlot*10)
 20     2  series = 6
 22     2  eventCount
 24     4  payloadCrc32   (CRC-32/IEEE of everything after the header)
 28     4  headerCrc32    (CRC-32/IEEE of bytes 0..27)
 32  6*count*2  series, planar int16 LE, oldest to newest
  .  eventCount*8  { u32 epoch, u8 id, u8 priority, u8 raised(1)/cleared(0), u8 0 }
```

Maximum 104 224 B. A wrong magic, version, length or CRC makes the file be
ignored **whole** — it is never restored halfway.

**Dump policy** (`UI_Task`, outside `LVGL_Lock`, `TelemetryHistory_PersistService`
once per loop pass): every **5 min** if samples have arrived since the last
dump, and on **entering the lock screen** with at least 2 min since the previous
dump (the 20 s auto-lock fires after every interaction; without the floor each
touch would be a dump). Only with synchronised time — before that the slots are
not epochs.

**Everything is sliced** (code review C2): one `UI_Task` pass does at most *one*
slice, never "allocate 104 KB + serialise 51 840 values + two CRCs + first
erase" at once. A dump is: (1) a pass that reserves the PSRAM staging buffer
(~104 KB, only while dumping) and takes the event snapshot; (2) *N* passes that
serialise 4096 values each (13 for 24 h) and then compute the payload CRC over
16 KB per pass (7), keeping incremental state (`trend_ser_*`, pure and
host-tested); (3) a pass that opens `/trend.bin.tmp`; (4) **the writing starts on
a later pass**: one 4 KB chunk per pass, at most one every 50 ms; (5) rename over
`/trend.bin`. Because the model keeps recording between passes, the serialiser
carries the ring's generation counter: if a sample (or a clock shift) lands
between slices it restarts (up to 5 times) instead of writing a torn snapshot.

**No I/O with an alarm or a lost link.** Nothing starts or continues while
`UI_IsAnyAlarmActive()` or `Display_IsBoardLinkLost()` is true
(`trend_io_allowed()`): a dump in progress is **aborted cleanly** (the `.tmp` is
discarded and the dump is retried when due), a restore is paused where it is, and
the deferred partition format waits too. A power cut mid-dump leaves the
previous file intact and an orphan `.tmp` that the next dump truncates. A real
failure aborts, frees the staging buffer and retries after a full period (never
in a loop); the in-RAM trend is unaffected. **Consequence:** during a long alarm
the trend is not saved, so a power cut in the middle of an alarm loses back to
the last dump (up to the alarm's length + 5 min). Cost: ~27 block erases per
dump; with the usual 5 min period that is ≈5 erases/block/day over ~1450
blocks, but with frequent locks the 2 min floor means one dump every 2 min:
720 dumps/day × 27 = 19 440 erases/day ≈ **13 erases/block/day** — still ~20
years of a 100 000-cycle budget. **Maximum loss on a power cut (no alarm): 5 min
of samples and the edges in them.**

**Known risk — UART without an IRAM-safe ISR during erases (A1/A2).** A flash
erase suspends the instruction cache; the UART driver's ISR is not placed in IRAM
here, so bytes of the `CTRL,*` stream can be lost while a sector erase is in
progress (≈45 ms each; the 4 KB chunks and the 64 KB format ranges each cost
at least one). Mitigations in place: no I/O with an alarm or a lost link, chunks
spaced ≥ 50 ms, and the protocol's lines are re-sent periodically and parsed
defensively (a truncated line is discarded). It is not eliminated: the bench
checklist (2.7) measures the lines discarded during a dump. If the count is not
zero in practice, the options are to lengthen the gap between chunks, to dump
only while locked, or to enable `CONFIG_UART_ISR_IN_IRAM` (blocked by the
build-environment limitation noted in `platformio.ini`).

**Restore** (once per boot, as soon as the clock is synchronised; also sliced,
and paused with an alarm or lost link): the file is read 4 KB per pass into a
PSRAM buffer; then the header is validated, the payload CRC is computed over
16 KB per pass and the block is *prepended* to what the session has already
recorded in 4096-slot slices — aligned by the epoch of its last slot; the gap
between that slot and the first live sample stays "no data"; live samples win
where the two overlap; what does not fit in 24 h is dropped from the old side;
file events in range and older than the first live event are prepended to the
live ones. The slices write only into free ring positions and the ring changes
visibly in the last one, so a live sample arriving mid-restore just makes the
job re-plan (`trend_prepend_*`). A file whose last slot is after "now + 1 slot"
(`FUTURE`, a wrong clock) or that no longer falls inside the ring (`STALE`) is
rejected: it is **retried once**, the first time the clock is corrected
afterwards (`TrendModel::clockShifts` changes), and **the dump does not overwrite
it until that retry** — or until a 30 min grace expires, so that a file that is
really old does not block persistence for ever. A file that is merely `INVALID`
(corrupt, other version) is final.

Manual bench checklist: `openspec/changes/archive/2026-10-25-hmi-trend/tasks.md`
(1.8 and 2.7, **still open**: no hardware was available). 2.7 includes
measuring the `CTRL` lines discarded during a dump (A1/A2) and the deferred
format on a blank partition (C1).
## 12. Service by hours of use

Each actuator with a bounded life (heater, fan, humidifier, phototherapy LEDs)
has a service threshold in hours of use. The motherBoard owns the data and the
threshold; the HMI only shows what `CTRL,SERVICE` says (see `PROTOCOL.md`) and
decides *when* to warn. It is **information, not an alarm**: no `AlarmId`, no
buzzer, nothing in the alarm centre.

Where it lives: the pure state (last report, due, "later") is
`src/modules/service/service_state.cpp` (host tests in
`test/test_service_state`); the view is a second mode of
`src/ui/MaintenanceDialog.cpp`, so it shares the overlay, the card and every
yielding rule of section 8. `Comm_Task` parses the line into
`g_serviceReport` and raises `g_pendingService`; `MaintenanceDialog_Poll()`
(UI_Task) hands it to `service_state`, so no LVGL call happens in the comm
task (ARQ-LOCK-001 is about I/O under the LVGL mutex: nothing here does NVS,
network or I2C — the only I/O is the UART request, same as the other
dialogs).

How to reach it: **Settings > MAINTENANCE > OPEN REMINDER**, then the
**SERVICE** button next to LATER/CLOSE (amber when something is due). The view
lists each actuator with `hours of threshold h since last service` and whether
it is `DUE NOW` or `up to date`; a **CLEANING** button goes back to the cleaning
view. There is no PIN on Settings > Maintenance today, so recording a service
takes two steps: **DONE** arms the row (the button becomes CONFIRM next to a
CANCEL) and **CONFIRM** sends `HMI,SERVICE_DONE,<id>`. It is refused in training
mode (it writes the board's NVS for real). The board answers with an updated
`CTRL,SERVICE`; until then the old value stays on screen.

When it opens by itself: after the cleaning reminder has had its turn
(`Maintenance_ShouldWarn()` first), if the **SERVICE REMINDER switch is on**
and at least one actuator is due. That switch is its own (NVS `hmi_cfg`,
`svc_en`) and **defaults to ON, also on a virgin NVS**: it does NOT depend on
the cleaning REMINDERS switch, which is off on a factory-fresh unit. Switched
off, only the automatic opening is silenced; the SERVICE button keeps working.
Changing it in training mode is refused with the standard toast and writes
nothing. It opens
directly in the service view, with the same yielding rules as the cleaning
(alarm, lost board link, other modal dialogs, training) and the same 3 min idle
cap. **LATER** silences it for 24 h, but only for the actuators that were due at
that moment: another actuator falling due afterwards brings it back. The
snooze is in RAM only (a display reboot forgets it).

If no report has arrived yet the view says so and the HMI asks for one every
30 s (`HMI,SERVICE_REQ`); the board also sends it on `HMI,UI_READY` and hourly.

Texts: `STR_SVC_*` in `i18n_strings.def` (ES/EN/FR/PT, no accents).

**Bench checklist** (not covered by host tests; nothing here has been run on
hardware yet):
1. Boot both boards: the service view shows 4 actuators with sane hours.
2. Force a threshold low (rebuild with `SERVICE_THRESHOLD_FAN_H 1u`) or edit the
   NVS base (`svc_b_fan` to 0 with the life counter above threshold): unlock the
   screen with the SERVICE REMINDER switch on (default, whatever the cleaning
   REMINDERS switch says) and see the soft notice; with that switch off, nothing
   opens by itself and the SERVICE button still works. Reboot: the switch keeps
   its value.
3. LATER: it does not come back on the next unlock within 24 h; another actuator
   falling due does bring it back.
4. DONE then CANCEL records nothing; DONE then CONFIRM zeroes the row, the
   board log prints `servicio N registrado`, and the value survives a reboot.
5. During an alarm or with the MB cable pulled the notice does not open / closes.
6. In a training lesson DONE shows "not available in training mode".
7. ThingsBoard shows `svc_heater_h`, `svc_fan_h`, `svc_hum_h`, `svc_photo_h`
   (GPRS only: DIAG is off over WiFi) and the lifetime counters are unchanged.

### Settings > MAINTENANCE panel coordinates

Coordinates (LVGL `LV_ALIGN_CENTER` offsets inside `ui_MaintConfigCont`,
770x361) in `ElementsCreation.cpp`, recorded because other branches add rows to
this same panel (`feat/hmi-weigh-reminder` adds a `wgh_en` switch):

| Element | x | y |
|---|---|---|
| title | 185 | -140 |
| REMINDERS label / switch (`ui_MaintEnable*`) | 60 / 296 | -95 |
| SERVICE REMINDER label / switch (`ui_MaintSvc*`, switch 90x30) | 60 / 296 | -60 |
| level labels (3 rows) | 185 | -15, 23, 61 (`-15 + 38*i`; was `-40 + 42*i`) |
| OPEN REMINDER button | 185 | 105 |
| hint | 185 | 152 |

The service switch sits one row (35 px) under REMINDERS; the only existing line
that changed is the `lv_obj_set_y` of the level labels. A new row for another
switch needs the same kind of room, so on merge the level labels and the hint
have to be re-spaced once for all the switches together.
