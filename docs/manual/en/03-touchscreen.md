# 3. The touchscreen

You control the incubator from the touchscreen on the front. This chapter describes what each screen shows. How to use each function step by step is explained in chapter 5, and alarms in chapter 6.

The screen has four main views: the **start-up screen**, the **main screen**, the **lock screen** and the **settings screen**. Other functions open as windows on top of them: the menu, the baby data wizard, the alarm centre, help and the reminders.

Every time you touch a control, the incubator makes a short click.

## 3.1 Start-up screen

![Start-up screen](img/intro_splash_en.png)

When you switch the incubator on, the start-up screen shows the IncuNest logos for 5 to 15 seconds while the screen connects to the control board. The main screen then appears automatically. You do not need to touch anything.

## 3.2 Main screen

![Main screen with all controls off](img/main_all_off_en.png)

The main screen is where you switch functions on and off and change their settings. It has a header bar at the top and up to three function blocks: **TEMPERATURE**, **HUMIDITY** and **PHOTOTHERAPY**.

### Header bar

From left to right:

| Element | What it shows or does |
|---|---|
| Clock | Local date and time. Shows "No time" if the clock has not been set (see 4.4). |
| Connectivity | An icon and 0 to 4 bars showing the network connection. The incubator works normally without a connection. |
| Lock button | Locks the screen (see 3.3). The ring around it fills during 20 seconds without touching the screen; when it is full, the screen locks by itself. |
| Baby identity | When a baby has been registered: the baby's name and, on the second line, the corrected gestational age and days of life (for example "CGA 32+3 - 5d"). |
| Menu button (three lines) | Opens the menu (see 3.4). |
| Alarm button (bell) | A bell with a **green check** when there are no active alarms. With active alarms, a red badge shows how many. Either way, touching it opens the alarm centre (see chapter 6). |

If the screen loses contact with the control board for more than 5 seconds, a flashing **LINK LOST** label appears in the header and the measured values disappear. This is also an alarm: see chapter 6.

![Header with baby identity](img/main_patient_heading_en.png)

### TEMPERATURE block

![Temperature control on in air mode](img/main_air_mode_on_en.png)

| Element | What it shows or does |
|---|---|
| **TURN ON / TURN OFF** button | Switches temperature control on or off. Blue (TURN ON) when control is off, red (TURN OFF) when it is on. Switching it on opens the baby data wizard first (see 5.1). |
| **AIR** panel | The measured air temperature and, after the arrow, the set temperature, in °C (for example "22.7 °C ⇒ 30.0 °C"). Touch the panel to select air mode. |
| **SKIN** panel (baby icon) | The infant's skin temperature. It appears only when skin mode is enabled in the settings and the skin temperature probe is connected. It is shown in grey while air mode is selected. Touch the panel to select skin mode. |
| **SET** with up and down arrows | Changes the set air temperature in steps of 0.2 °C, from 30.0 °C to 39.0 °C. The arrows work only in air mode. In skin mode the set temperature is fixed at 36.5 °C. |
| Therapy time | Under the title: how long this baby has been under temperature control (for example "3h 20m"). |

![Temperature control on in skin mode](img/main_skin_mode_on_en.png)

You cannot switch temperature control on while a fan failure or heater fault alarm is active.

While a **FAN FAILURE** or **HEATER FAULT** alarm is active, the TEMPERATURE block disappears from the main screen and temperature control cannot be used. See chapter 6.

![Main screen with a heater or fan fault: no TEMPERATURE block](img/main_heater_error_en.png)

### HUMIDITY block

![Humidity control on](img/main_humidity_on_en.png)

The HUMIDITY block appears only when **HUMIDITY CONTROL** is enabled in the settings (see 3.5).

| Element | What it shows or does |
|---|---|
| **TURN ON / TURN OFF** button | Switches humidity control on or off. If no baby is registered yet, the incubator asks you to identify the baby first. |
| Measured humidity | The measured relative humidity and, after the arrow, the set value, in % (for example "39 % ⇒ 50 %"). |
| **SET** with up and down arrows | Changes the set humidity in steps of 5 %, from 20 % to 90 %. |

### PHOTOTHERAPY block

| Element | What it shows or does |
|---|---|
| **TURN ON / TURN OFF** button | Switches the phototherapy lamp on or off. Before the lamp switches on, the incubator asks you to identify the baby (if no baby is registered yet) and to confirm that the infant's eyes are protected. |
| Timer: **–**, value, **+** | Appears when phototherapy is on. Sets the treatment time, from 120 to 600 minutes in steps of 20 minutes. |
| **START** | Starts the timer. The button changes to **RUNNING** and a red **X** appears. When the time is over, the lamp switches off by itself. |
| Red **X** | Cancels the timer. The lamp stays on with no time limit until you switch phototherapy off. |

If you switch phototherapy on and do not press START, the lamp stays on with no time limit.

![Phototherapy on, without timer](img/main_photo_on_continuous_en.png)

![Phototherapy timer running](img/main_photo_timer_running_en.png)

[PENDING: the timer shows "h:mm" while the lamp is on and "N min" while it is off, and its starting value is below the minimum. Waiting for the firmware fix before describing the display in detail.]

## 3.3 Lock screen

![Lock screen in air mode](img/lock_screen_air_en.png)

The lock screen shows the measured values in large digits and prevents accidental changes. It appears when you touch the lock button, or by itself after 20 seconds without touching the screen.

While the help, training or reminder windows are open, the screen does not lock. These windows close by themselves after 3 minutes without use.

### What the lock screen shows

| Element | What it shows |
|---|---|
| **AIR TEMPERATURE** | Measured air temperature. |
| **BABY TEMPERATURE** | Measured skin temperature. |
| **HUMIDITY** | Measured humidity (only when humidity control is enabled). |
| **TARGET TEMP** / **TARGET HUMIDITY** | The set values, when the corresponding control is on. |
| **STATUS** | The alarm status: how many alarms are active. Touch it to open the alarm centre. |
| **PHOTOTHERAPY** | Time left on the phototherapy timer. |
| Vital signs | Pulse oximetry: see below. |
| Alarm banner | A band at the top with the most important active alarm (see chapter 6). |

The header repeats the clock, the connectivity and the baby identity, and adds two indicators:

- **Door**: shows **Open** when the cover is open.
- **Noise**: an icon with three levels of measured noise: below 65 dBA, from 65 to 74 dBA, and 75 dBA or more. [PENDING: where the noise is measured (inside the chamber or in the room).]

[PENDING: confirm that every V19 unit has the door and noise sensors.]

![Lock screen with door open and high noise](img/lock_screen_door_noise_en.png)

### Vital signs (pulse oximetry)

![Lock screen with vital signs](img/lock_screen_spo2_en.png)

When the pulse oximetry probe is correctly applied to the infant, the lock screen shows:

| Element | Colour | Meaning |
|---|---|---|
| Waveform | Cyan | The pulse wave (plethysmogram), last 5 seconds. |
| **PR** (bpm) | Green | Pulse rate, in beats per minute. |
| **SpO2** (%) | Cyan | Oxygen saturation. |
| **PI %** | Violet | Perfusion index. |

A value that is not available yet is shown as "--".

::: warning
**WARNING** The vital signs appear only on the lock screen and only while the probe is correctly applied. If the probe comes off, they disappear without any message. The incubator does not raise alarms for SpO2 or pulse rate. See 1.3.
:::

### Unlocking the screen

![Unlock prompt](img/lock_unlock_prompt_en.png)

1. Touch the screen anywhere. The message **"PRESS 2 SEC TO UNLOCK"** appears.
2. Press and hold the message. Its border fills up.
3. When the border is full, the main screen appears.

If you do not press the message, it disappears after 5 seconds.

[PENDING: the screen says 2 seconds but unlocks after 1.5 seconds. Waiting for the firmware fix.]

## 3.4 Menu

![Menu](img/menu_en.png)

Touch the menu button (three lines) in the header to open the menu:

| Button | Opens |
|---|---|
| **DATE AND TIME** | Setting the date and time (see 4.4). |
| **REGISTERED BABIES** | The list of babies, their history and weight curve (see 5.9). |
| **SETTINGS** | The settings screen (see 3.5). |
| **HELP** | Guided tutorials, video tutorials and contacting support (see 8.2). |

Touch **X** to close the menu.

## 3.5 Settings

The settings screen has a list on the left. Touch a row to see its options on the right. Touch the back arrow to return to the main screen.

| Row | Options |
|---|---|
| **INFO** | Display version, motherboard version, serial number (S/N) and connectivity. Have this information ready when you contact technical support. |
| **LANGUAGE** | English, Spanish, French or Portuguese. |
| **MODES** | **SKIN MODE**: allows skin temperature control. **DARK MODE**: dark colours for the screen. **HUMIDITY CONTROL**: shows or hides the HUMIDITY block. |
| **MAINTENANCE** | Switches the cleaning, weighing and service reminders on or off, and **OPEN REMINDER** shows them now (see 8.1). |
| **WIFI** and **HARDWARE TEST** | For technical personnel only (see chapter 10). |

![Settings — information](img/settings_info_en.png)

![Settings — language](img/settings_language_en.png)

![Settings — modes](img/settings_modes_en.png)

**SKIN MODE** can be switched on only when the skin temperature probe is connected. Without the probe, the row shows **NO SKIN PROBE DETECTED**, and trying to switch it on shows "Skin mode unavailable: Connect the skin temperature probe".

![Settings — no skin probe](img/settings_modes_no_probe_en.png)

[PENDING: a PIN for the technician functions has been proposed (decision 4). If it is added, describe it here.]

## 3.6 Short messages

Some actions show a short message in the middle of the screen for a few seconds, for example "Alarm silenced" or "Skin mode unavailable". These messages are information only: they are not alarms and need no action.
