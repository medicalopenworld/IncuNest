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
   real control with a 4 px amber frame plus an explanatory bubble
   (PREVIOUS/NEXT/EXIT). It walks 19 steps across `ui_ScreenMain` and
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
3. **Contact support**: an on-screen keyboard (`lv_btnmatrix`) for a message
   of up to 160 ASCII characters. The subject is composed automatically as
   `IncuNest SN <serial> - Solicitud de soporte`, and the body carries the
   message plus a debug report built by `support_report_build()`
   (`Display_HMI/src/modules/support/support_report.cpp`), in this order:
   identity/versions (`sn`, `hmi`, `mb`, `hw`), boot count and last reset
   reason, uptime, WiFi/IP/ThingsBoard connection status, serial link state
   and language, control mode/actuation/setpoints, current air/skin/humidity
   readings and probe state, phototherapy mode, active/silenced alarm
   bitmasks, the titles of the active alarms, and free internal heap / PSRAM.

   Two send paths, always offered together on the result view:
   - **From the device**: if `WIFIIsConnectedToServer()`, the request is
     queued (`SupportRequest_Submit()`) and the WiFi/OTA task
     (`supportRequestService()` in `Wifi_OTA.cpp`) publishes it as
     ThingsBoard telemetry — `support_request`, `support_message`,
     `support_report`, `support_to` — see
     [`thingsboard_dashboards.md`](thingsboard_dashboards.md). Without a
     server, nothing is queued.
   - **From the operator's phone**: a `mailto:` QR with recipient, subject
     and body already filled in, always available (works offline, no
     server needed). If the encoded content doesn't fit the QR, it is
     regenerated first without the free-text message, then without the
     report, and the screen states what was dropped.

Common rules: the help menu and the guided tour are exempt from the 20 s
auto-lock timeout while open, but only up to `HELP_IDLE_TIMEOUT_MS` (3 min
without any touch): past that they close themselves and the normal auto-lock
resumes from zero (the alarm banner is only drawn on `ui_ScreenLock`, so a
forgotten help view must never block the way to it). Any active alarm
(`UI_IsAnyAlarmActive()`, regardless of priority) or a lost board link
(`Display_IsBoardLinkLost()`) closes both and returns to `ui_ScreenMain`,
discarding any support request not yet published — the same yield rule as
`TelemetryHistory`. The tour overlay is created before the alarm banner and
the AUDIO PAUSED icon in `lv_layer_top()` and is never raised above them.
Every text is available in ES/EN/FR (`g_lang`). The free-text field of the
contact form asks for no patient data: it leaves the device over plain MQTT
and is visible in the `mailto:` QR.
