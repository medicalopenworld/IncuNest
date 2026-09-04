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

The baseboard also needs to know which language the user is reading in order
to re-send the batch of alarm strings. Both converge through the initial UART
packet sent from the display.

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
never raised above them. Every text comes from the catalogue in the four
shipped languages (§5). The debug report contains no patient data (no baby
name or profile, no SSID, password or token).

## 7. Maintenance reminder

The incubator reminds the staff to clean and disinfect it. The decision of
*when* lives in `src/modules/maintenance/maintenance.cpp` (no LVGL); the
pop-up that tells it, in `src/ui/MaintenanceDialog.cpp`.

Two independent reasons fire the reminder:

1. **Interval elapsed** — calendar days since the last recorded maintenance,
   measured with the motherBoard's epoch (`HMI_GetEpochNow()`), not with
   power-on hours: an incubator that spent a month switched off still needs
   cleaning before the next patient. The interval is chosen per unit in
   **Settings > MAINTENANCE** (disabled / 7 / 15 / 30 / 90 days, default 30).
2. **New baby** — `BabyWizard_GetActiveSeq()` reports a profile different
   from the last one already reminded about, i.e. a new patient was admitted
   or the incubator changed from one baby to another. This is the
   between-patients cleaning and does not wait for the interval.

Setting the interval to *disabled* switches off both reasons: whoever turns
the reminder off does not want some reminders and not others.

The pop-up is the same card as `HelpDialog` (780x460 over `ui_ScreenMain`),
with the **`SUPPORT_TUTORIAL_URL` QR on the left** — the very code the help
menu's "Video tutorial" view shows, because the cleaning tutorials live on
that same page — the reason for the notice on the right, the date of the last
recorded maintenance, and two exits and no X:

- **MAINTENANCE DONE** records today's date, clears the snooze and resets the
  interval. Reachable also without waiting for the notice, from the button in
  Settings > MAINTENANCE.
- **LATER** silences the reminder for 24 h.

When it appears: only armed by **unlocking the screen** (and once at UI init,
because whoever powers the incubator up is standing in front of it), never in
the middle of a manoeuvre. `MaintenanceDialog_Poll()` then opens it only on
`ui_ScreenMain`, with no other modal dialog open (help, tour, alarm centre,
baby wizard, exit dialog or time dialog) and no reason to yield.

Common rules, the same as the help menu: it yields to any active alarm
(`UI_IsAnyAlarmActive()`) or a lost board link (`Display_IsBoardLinkLost()`),
and it is exempt from the 20 s auto-lock with its own `MNT_IDLE_TIMEOUT_MS`
cap (3 min). That cap is measured with `lv_tick_elaps()` from the moment it
opened and **not** with `lv_disp_get_inactive_time()`: the auto-lock exemption
itself resets that counter every 200 ms, so a cap reading it would never fire.
Closing by the cap does not count as an answer — the notice comes back on the
next unlock, since only pressing a button silences it.

State persists in NVS (`hmi_cfg`): `mnt_last` (epoch of the last recorded
maintenance), `mnt_days` (interval), `mnt_snooze` (epoch when LATER expires)
and `mnt_seq` (the last baby already reminded about, so a reboot with the same
patient inside does not nag again). Edge cases handled in `Maintenance_Tick()`:
with no clock yet nothing is due; the first boot *with* a valid clock seeds
`mnt_last` with today (otherwise a factory-fresh unit would read as "overdue
since 1970"); and a clock moved backwards re-anchors `mnt_last` to today
instead of leaving the record in the future, where the interval would never
elapse again.
