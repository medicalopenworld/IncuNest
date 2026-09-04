# HMI Interface (Human-Machine Interface)

The visual and interactive layer of IncuNest is embedded into the **LVGL** (Light and Versatile Graphics Library) ecosystem, one of the most prolific embedded graphics libraries. The HMI is designed to be direct, highly responsive for touch control with medical gloves, and consume minimal cognitive bandwidth.

## 1. Visual Design and Layout

The main screen is partitioned into informational quadrants and side plus ceiling/floor toolbars (HUD - Heads Up Display) providing situational awareness in tenths of a second.

### Permanent HUD (Top Zone)
It continuously witnesses key and constant states on whichever screen the doctor navigates to:
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

## 6. Factory Test (splash button)

The boot splash (`ui_ScreenIntro`) carries a single extra control, the
**HW test** button (bottom centre). It is the only entry point: no other
screen offers it, so the test can only be started right after power-up, before
the unit is in clinical use. Pressing it cancels the automatic jump to the main
screen and suspends the 20 s inactivity lock while the test screen is open.
The button ignores taps during the first 1.5 s of the splash and has no
extended click area: the GT911 can report a phantom corner touch while it
initialises, and on the bench that opened the test by itself.

The test screen is a full-screen overlay (`src/ui/FactoryTest.cpp`, same
pattern as `AlarmCenter`) showing a **3-column grid of buttons**, one per
test, sorted FAIL → WARNING → running → PASS; skipped tests are not shown.
Colours: red FAIL, amber WARNING (the test could not be completed for lack of
environment: no AP, no coverage, no server, no network time), blue PASS.
Tapping a button opens a detail panel with a one-line description of what the
test checks, its status, the measured value and Retry when it applies. The
flow is:

0. **Entry gate**: a warning that the unit must be EMPTY (no patient) because
   actuators will be driven open-loop, with Yes / No. "No" returns to the normal
   splash flow. This is a confirmation, not an authentication.
1. **Local tests**, run inside `UI_Task` by polling: flash/PSRAM/heap, I2C
   presence of the GT911 touch (0x14) and the STC8H1K28 backlight/buzzer
   controller (0x30), buzzer and speaker with Yes/No, WiFi MAC and connection
   or scan, NVS write/read, and the UART link with the motherBoard. The panel
   colour and touch-target tests were removed after the first bench run: they
   lengthened the battery without adding value on the current assembly line.
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
masks, board counters, firmware version) for traceability. Operator questions
time out after 60 s as FAIL, 120 s without any `CTRL,FTEST*` line closes the
board section as "link lost", and the summary closes itself after 10 min of
inactivity, so an unattended unit never stays exempt from the inactivity lock.
The display keeps sending its 1 Hz keepalive while the screen is open: the
motherBoard aborts the battery if the display goes silent for 5 s.
