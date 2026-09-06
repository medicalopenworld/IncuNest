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

1. **Guided tour** (`HelpTour`, `HelpTour.cpp`): a full-screen overlay drawn
   on `lv_layer_top()` — so it survives screen changes — that highlights each
   real control with a 4 px amber frame with a glow, keeps the inside of the
   frame at normal brightness while four shades dim everything else
   (spotlight), plus an explanatory bubble (PREVIOUS/NEXT/EXIT) placed on
   the opposite half of the screen. It walks 19 steps across `ui_ScreenMain` and
   `ui_ScreenSettings` (help button, clock, connectivity, lock, Babies,
   alarms, temperature, humidity, phototherapy, then the Settings rows Info,
   WiFi, Languages, Modes), switching screens with `lv_scr_load()` when a step
   needs it. Steps whose target control is currently hidden (e.g. humidity
   disabled) are skipped. The overlay is clickable and swallows every touch,
   so nothing gets actioned during the tour — it always returns to
   `ui_ScreenMain` on exit or at the last step.
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

Common rules: the help menu and the guided tour are exempt from the 20 s
auto-lock timeout while open, but only up to `HELP_IDLE_TIMEOUT_MS` (3 min
without any touch): past that they close themselves and the normal auto-lock
resumes from zero (the alarm banner is only drawn on `ui_ScreenLock`, so a
forgotten help view must never block the way to it). Any active alarm
(`UI_IsAnyAlarmActive()`, regardless of priority) or a lost board link
(`Display_IsBoardLinkLost()`) closes both and returns to `ui_ScreenMain` —
the same yield rule as `TelemetryHistory`. The tour overlay is created
before the alarm banner and the AUDIO PAUSED icon in `lv_layer_top()` and is
never raised above them. Every text is available in ES/EN/FR (`g_lang`). The
debug report contains no patient data (no baby name or profile, no SSID,
password or token).

## 7. Hardware Test (splash button and Settings row)

There are two entry points, both leading to the same test screen:

- **Factory**: the boot splash (`ui_ScreenIntro`) carries a **HW test**
  button (bottom centre). Pressing it cancels the automatic jump to the main
  screen and suspends the 20 s inactivity lock while the test screen is open.
  The button ignores taps during the first 1.5 s of the splash and has no
  extended click area: the GT911 can report a phantom corner touch while it
  initialises, and on the bench that opened the test by itself.
- **Field service**: a **Hardware test** row at the bottom of
  `ui_ScreenSettings` (below Info), reachable only through the long-press lock
  icon like the rest of Settings. The row is enabled only while thermal
  control and phototherapy are off; otherwise it is greyed out with "Turn
  control off to test". "No" on the empty-unit warning returns to Settings.
  It was placed in Settings rather than in the help menu on purpose: the help
  overlays are for clinical staff and yield to any alarm, whereas the test
  inhibits alarms and drives actuators open-loop. Without a service PIN the
  field barrier is long-press + control off + explicit confirmation (residual
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

0. **Entry gate**: a warning that the unit must be EMPTY (no patient) because
   actuators will be driven open-loop, with Yes / No. "No" returns to the normal
   splash flow. This is a confirmation, not an authentication.
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
   back as `HMI,FTEST,CONFIRM`. If the board rejects the test (control active,
   test already running) the reason is displayed. If nothing arrives within
   10 s the section is marked "board without support".
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
