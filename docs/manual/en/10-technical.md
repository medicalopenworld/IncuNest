# 10. For technical personnel

This chapter is for authorised technical personnel only. It covers the touchscreen functions used for installation, testing and service. Clinical staff do not need these functions to use the incubator.

::: warning
**WARNING** Some functions in this chapter switch the heater, the fan and the phototherapy lamp on without temperature control. Use them only with the incubator empty.
:::

[PENDING: a PIN to protect these functions has been proposed (decision 4). At present they can be opened by anyone from the settings screen. If a PIN is added, describe how to enter it here.]

## 10.1 Information and versions

**Settings > INFO** shows:

| Field | Content |
|---|---|
| **DISPLAY VERSION** | Firmware version of the touchscreen, with its build mark. |
| **MOTHERBOARD VERSION** | Firmware version of the control board. |
| **S/N** | Serial number of the incubator. |
| **CONNECTIVITY** | **WIFI + SERVER**, **WIFI (NO SERVER)**, **2G + SERVER**, **2G (NO SERVER)**, or **-** when there is no connection. |

This manual applies to the firmware versions given on the cover. If the versions on the screen are different, check that you have the right manual.

![Settings — information](img/settings_info_en.png)

## 10.2 Wi-Fi and server connection

The incubator works normally without a network connection. A connection is used to send data to the server, to set the clock automatically and to update the firmware.

To connect to a Wi-Fi network:

1. Open **Settings > WIFI**.
2. Touch **SSID** and type the network name (1 to 32 characters).
3. Touch **PASSWORD** and type the password (up to 63 characters).
4. Press **CONNECT**.

The panel shows two status lines: the connection of the control board and the connection of the touchscreen. Press **Disconnect** to leave the network.

![Settings — Wi-Fi](img/settings_wifi_en.png)

The connectivity icon and bars in the header show the quality of the connection (0 to 4 bars).

[PENDING: how the 2G connection is configured (SIM card, activation) and when the incubator uses 2G instead of Wi-Fi.]

## 10.3 Hardware test

The hardware test checks the touchscreen and the control board, and asks the operator to confirm the parts that need a person to see or hear them.

::: warning
**WARNING** During the hardware test the heater, the fan and the phototherapy lamp work without temperature control. The chamber must be **empty**.
:::

**Before you start:** switch off all functions. The test cannot start while a function is on ("Turn control off to test") or during a training lesson.

1. Open **Settings > HARDWARE TEST**. Read the warning and confirm that the incubator is empty.
2. The touchscreen tests itself first: system information, internal bus (I2C), Wi-Fi, memory (NVS) and the link to the control board.
3. The control board then runs its tests. The screen shows them in a grid of 3 × 3, over several pages. Each test shows one of these results:

    | Result | Meaning |
    |---|---|
    | **PASS** | The test passed. |
    | **FAIL** | The test failed. |
    | **WARN** | The test passed with a warning. |
    | **SKIPPED** | The test was not run. |
    | **WAIT** | The test is running. |
    | **CONFIRM** | The test needs you to answer a question. |

4. Answer the questions when they appear: whether the door sensor responds, whether the lamp lights up, and whether you hear the buzzer. Each row of tests waits up to 150 seconds.
5. At the end, the screen shows a summary. Touch a test to see its details, or press **RETRY** to repeat the test.

![Hardware test](img/hw_test_grid_en.png)

During the test, the alarms for mains interruption and high noise are suppressed.

If any test fails, do not use the incubator with a patient until the fault has been repaired.

## 10.4 Recording a service

When a part reaches its service hours, the service reminder appears (see 8.1). After servicing a part:

1. Open **Settings > MAINTENANCE > OPEN REMINDER**, then **SERVICE**.
2. Press **DONE** next to the part you have serviced.
3. Press **CONFIRM**. The hour counter of that part starts again from zero. [PENDING: confirm that DONE resets the counter.]

| Part | Service interval |
|---|---|
| Heater | 5000 h |
| Fan | 10000 h |
| Humidifier | 2000 h |
| Phototherapy lamp | 10000 h |

If the screen shows "Waiting for data from the board...", wait a few seconds for the hours to load.

[PENDING: what each service consists of (procedure in the service manual or here).]

## 10.5 Firmware update

[PENDING: firmware update procedure for the V19 (over the network or by cable), who may do it and how to check the result in Settings > INFO.]

## 10.6 Technician training course

**Menu > HELP > GUIDED TUTORIAL** includes a course for technicians (see 8.3 for how training mode works):

| Lesson | Content |
|---|---|
| Information and versions | Reading Settings > INFO |
| WiFi and server | Connecting to a network |
| Language and modes | Language, skin mode, dark mode, humidity control |
| Setting the time | Date and time |
| Technical alarms | Explanation of the technical alarms |
| Updating the firmware | Explanation of the update |
| Support report | Sending the support email with the technical report |
| Safe shutdown | Explanation of how to switch off safely |

## 10.7 Alarm conditions

For reference, the conditions that raise each alarm in the current firmware. Chapter 6 tells clinical staff what each alarm means and what to do.

| Alarm | Raised when | Clears when |
|---|---|---|
| AIR THERMAL FAULT | Air temperature above the maximum (default 40 °C, adjustable between 34 °C and 40 °C) | Air drops 0.5 °C below the maximum |
| SKIN THERMAL FAULT | Skin temperature above the maximum (default 37.5 °C), in any mode | Skin drops 0.2 °C below the maximum |
| AIR SENSOR FAULT | No valid air reading for more than 5 s | A valid reading returns |
| SKIN PROBE FAULT | Skin mode with control on, and no skin reading for more than 5 s | A valid reading returns |
| FAN FAILURE | Fan speed below 3000 rpm after 6 s of starting; or, at the start-up test, fan current below 0.03 A or above 0.8 A | Fan speed of 3300 rpm or more |
| AIR OUTLET BLOCKED | Fan drive above 220 for 5 s | Fan drive 200 or less |
| MAINS INTERRUPTION | Two consecutive readings without mains (about 0.4 s) | About 2 s with mains |
| AIR TEMP HIGH / LOW | Air mode: air more than 1.0 °C above / below the set temperature (LOW only after the first 30 min) | Difference under 0.95 °C |
| SKIN TEMP HIGH / LOW | Skin mode: skin more than 1.0 °C above / below 36.5 °C (LOW only after the first 30 min) | Difference under 0.95 °C |
| HEATER FAULT | Start-up test: heater current below 1.5 A or above 13 A | Does not clear until the incubator is switched off |
| SUPPLY UNDERVOLTAGE | Supply voltage above 0 V and below 8 V | Voltage back to normal |
| DISPLAY LINK LOST | No message from the screen for more than 5 s (15 s at start-up) | Messages return |
| SKIN PROBE UNUSABLE | Air mode: the probe was read before and has given no reading for more than 5 s | A valid reading returns |
| HUMIDITY DEVIATION | Humidity control on, more than 10 % from the set value, after the first 30 min | Difference under 5 % |
| HEATER SENSOR FAULT | The heater current sensor does not answer for 10 cycles | [PENDING: in the current firmware it does not clear.] |
| SENSORBOARD LINK LOST | No signal from the sensor board for more than 90 s (60 s of grace at start-up) | The next signal arrives |
| DOOR SENSOR SUSPECT | 4 or more door changes in 60 s | The changes stop |
| PHOTOTHERAPY LAMP FAULT | Lamp on and current below 0.05 A for 5 s | Current of 0.08 A or more, or lamp off |
| HIGH NOISE | Noise of 65 dB or more for 10 s | Under 63 dB for 10 s |

[PENDING: confirm these values against the firmware version of the cover before publishing, and how the air maximum is adjusted.]
