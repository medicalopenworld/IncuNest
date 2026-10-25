# HMI Interface (Human-Machine Interface)

The visual and interactive layer of IncuNest is embedded into the **LVGL** (Light and Versatile Graphics Library) ecosystem, one of the most prolific embedded graphics libraries. The HMI is designed to be direct, highly responsive for touch control with medical gloves, and consume minimal cognitive bandwidth.

## 1. Visual Design and Layout

The main screen is partitioned into informational quadrants and side plus ceiling/floor toolbars (HUD - Heads Up Display) providing situational awareness in tenths of a second.

### Permanent HUD (Top Zone)
It continuously witnesses key and constant states on whichever screen the doctor navigates to:
*   Help button (`?`, `ui_HelpButton`) at the leftmost slot, to the left of the clock. Opens the help menu (see §6). Shown only on `ui_ScreenMain`; the `ui_ScreenLock` heading replica keeps clock and connectivity at the same x position but has no help button.
*   Connectivity icons (Server, WiFi, or Null state).
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
centre, baby wizard, exit dialog or time dialog) and no reason to yield.

Common rules, the same as the help menu: it yields to any active alarm
(`UI_IsAnyAlarmActive()`) or a lost board link (`Display_IsBoardLinkLost()`),
and it is exempt from the 20 s auto-lock with its own `MNT_IDLE_TIMEOUT_MS`
cap (3 min). That cap is measured with `lv_tick_elaps()` from the moment it
opened and **not** with `lv_disp_get_inactive_time()`: the auto-lock exemption
itself resets that counter every 200 ms, so a cap reading it would never fire.
Closing by the cap does not count as an answer — the notice comes back on the
next unlock, since only pressing a button silences it.

State persists in NVS (`hmi_cfg`): `mnt_daily`, `mnt_weekly`, `mnt_term` (the
epoch of each level's last record), `mnt_en` (warn on/off), `mnt_snooze`
(epoch when LATER expires), `mnt_seq` (the last baby seen in charge) and
`mnt_tpend` (a discharge with no terminal clean recorded yet). Edge cases
handled in `Maintenance_Tick()`: with no clock yet nothing is due; the first
boot *with* a valid clock seeds the three dates with today (otherwise a
factory-fresh unit would read as "overdue since 1970"); and a clock moved
backwards re-anchors any date left in the future to today, where the interval
would otherwise never elapse again.

## 9. Telemetry trend (24 h, persistent)

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
