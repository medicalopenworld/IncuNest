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

The HMI loads and manages logical bidimensional arrays pointing the LCD strings dependent on variables. The baseboard also needs to know which language the user is reading in order to re-send the batch of alarm strings. Both converge through the initial UART packet sent from the display.

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
     *interactive* lesson runs, `CommTask` freezes the motherBoard link —
     the periodic keepalive keeps going out so the link isn't declared lost,
     but with the `hmi_msg` snapshot taken when the lesson started, not the
     live one the student is changing on screen; it stops sending any baby
     profile or time-set request (alarm silence and alarm test still go out:
     they act on the alarm system, not on therapy, and must never be muted),
     and instead answers the waiting assistants (`BabyWizard`, `BabyExitDialog`,
     `TimeDialog`) with **simulated responses** built locally (empty profile
     list, a fake ACK with `seq 0xFFFF`, the NTE range computed with the
     same `shared/include/nte_table.h` the board uses, a `TIME_ACK`) after a
     short delay, so the assistants behave exactly as with a real board.
     `Display_ApplyCtrlState()` keeps updating identity, firmware-version
     labels and the alarm bitmask from the board's `CTRL,STATE`, but does
     **not** apply the rest of it (setpoints, switches, modes) to the UI.
     Nothing changed during the lesson is written to NVS, and the WiFi
     CONNECT / DISCONNECT buttons are refused with a toast while training.
     Leaving the lesson (SALIR, an abort, or finishing it) restores the local
     snapshot (setpoints, panel, switches, dark mode, humidity, language,
     phototherapy), then `Training_Exit()` restores `hmi_msg` from the frozen
     snapshot and lets the next real `CTRL,STATE` take over again.
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
   - **Phase 1 content** (`openspec/changes/hmi-cursos-formacion`): Nursing
     has E0 (interface introduction, the old guided-tour steps, passive) and
     the interactive E1 (air temperature) and E5 (handling an alarm);
     Technician only has T0 (same intro). The rest of both courses (E2–E11,
     T1–T8) ships in later phases.
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
and returns to `ui_ScreenMain` — the same yield rule as `TelemetryHistory`;
an interactive lesson also aborts on a shutdown in progress. The lesson
overlay is created before the alarm banner and the AUDIO PAUSED icon in
`lv_layer_top()` and stays below them, except that during a free step it
temporarily raises above `AlarmCenter`/`TelemetryHistory` if the lesson
opens one of them, so its bottom strip stays visible; it drops back to its
original place once that modal closes. Every text is available in ES/EN/FR
(`g_lang`). The debug report contains no patient data (no baby name or
profile, no SSID, password or token).
