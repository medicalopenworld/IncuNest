# 6. Alarms

The incubator raises an alarm when something needs your attention: a temperature out of range, a sensor that stops working, a fault in the heater or the fan, and other conditions. This chapter explains how alarms are shown and sounded, how to handle them, and what to do for each one.

::: warning
**WARNING** When an alarm sounds, check the infant first, then the cause of the alarm. Alarms do not replace watching the infant.
:::

## 6.1 Priorities and alarm signals

Each alarm has one of three priorities. The priority is marked in front of the alarm message:

| Priority | Mark | Sound |
|---|---|---|
| **HIGH** | `!!!` | 10 beeps (two groups of 5), repeated about every 10 seconds |
| **MEDIUM** | `!!` | 3 beeps, repeated about every 25 seconds |
| **LOW** | `!` | 1 beep, repeated about every 30 seconds |

[PENDING: confirm the repetition times on the bench.]

Where alarms appear:

- **Main screen**: the alarm button in the header shows a red badge with the number of active alarms. With no active alarms it shows a green check.
- **Lock screen**: a band at the top shows the message of the most important active alarm, in its priority colour. The **STATUS** field shows how many alarms are active.
- **Alarm centre**: all active alarms and the most recent ones (see 6.2).

![Lock screen with a high-priority alarm](img/lock_alarm_banner_high_en.png)

![Lock screen with a medium-priority alarm](img/lock_alarm_banner_med_en.png)

![Lock screen with a low-priority alarm](img/lock_alarm_banner_low_en.png)

### First 30 minutes after switching on

During the first 30 minutes after you switch temperature control on, the incubator is still warming up. During this time:

- the low-temperature alarms (**AIR TEMP LOW**, **SKIN TEMP LOW**) and **HUMIDITY DEVIATION** are not raised;
- the high-temperature alarms (**AIR TEMP HIGH**, **SKIN TEMP HIGH**) are shown, but their sound is delayed.

All the other alarms work from the start.

## 6.2 Alarm centre

Open the alarm centre by touching the alarm button (or the green check) on the main screen, or **STATUS** on the lock screen.

![Alarm centre with active alarms](img/alarm_center_active_en.png)

| Part | What it shows |
|---|---|
| **Active** | One card for each active alarm, with its message, its priority (HIGH, MEDIUM or LOW) and whether its sound is paused. The card border has the colour of the priority. Shows "No active alarms" when there are none. |
| **Log** | The last 10 alarms, with the time they started and ended, or "unresolved" if they are still active. Resolved alarms have a green background. |
| **TEST** | Tests the alarm sounds (see 6.4). |
| **TREND** | Opens the trend charts (see 5.9). |

Touch an alarm card to see what to do for that alarm. Press **CLOSE** to return.

![Alarm description](img/alarm_detail_popup_en.png)

![Alarm log](img/alarm_center_log_en.png)

## 6.3 Silencing an alarm

Each card in the alarm centre has one button:

| Button | What it does |
|---|---|
| **SILENCE** | Pauses the sound of that alarm for 10 minutes. The alarm stays on the screen with the message "AUDIO PAUSED". A countdown in the header shows the time left. |
| **RESUME** | Restores the sound of a silenced alarm before the 10 minutes are over. |
| **ACKNOWLEDGE** | Only for **HEATER FAULT** (see 6.5). |

After 10 minutes, if the cause is still present, the alarm sounds again.

![Silenced alarm](img/alarm_center_silenced_en.png)

::: warning
**WARNING** Silencing an alarm does not solve its cause. Deal with the cause while the alarm is silenced.
:::

## 6.4 Testing the alarm sounds

Test the alarm sounds regularly to make sure you will hear them. [PENDING: how often — clinical/technical decision.]

1. Open the alarm centre.
2. Press **TEST**. The screen shows "ALARM TEST" and the incubator plays the low, medium and high priority sounds, one after another.

![Alarm test](img/alarm_test_banner_en.png)

The test cannot run while an alarm is active: the screen shows "An alarm is active: cannot test now".

If you do not hear one of the sounds, do not use the incubator and contact technical support.

## 6.5 Alarm list

The message on the screen has two parts: the alarm name and a short instruction. The table gives the name, what it means and what to do.

**If an alarm does not clear after you have done what the table says, or comes back again and again, switch the incubator off and on. If the problem continues, do not use the incubator and contact technical support (see 8.5).**

"Heater cut" means that the incubator stops heating while the alarm is active.

### High priority

| Alarm | What it means | What to do |
|---|---|---|
| **AIR THERMAL FAULT** | The air temperature has gone above the safety limit (40 °C by default). Heater cut. | Check the infant immediately. Open the access ports to cool the chamber if needed. Check that the incubator is not in the sun or near a heat source. |
| **SKIN THERMAL FAULT** | The skin temperature has gone above the safety limit (37.5 °C by default). Heater cut. | Check the infant immediately and measure the core temperature with a clinical thermometer. Check that the skin probe is correctly attached. |
| **AIR SENSOR FAULT** | The incubator has no valid air temperature reading. Heater cut. | Check the infant. The incubator cannot control the temperature: move the infant to another warm environment if the alarm does not clear. Contact technical support. |
| **SKIN PROBE FAULT** | In skin mode, there is no valid reading from the skin probe. Heater cut. | Check the infant. Check that the probe is attached and connected, or replace it. If you cannot, switch to air mode. |
| **FAN FAILURE** | The fan is not turning, or not fast enough. There is no air circulation. Heater cut, and temperature control cannot be switched on. | Check the infant. Move the infant to another warm environment if needed. Contact technical support. |
| **AIR OUTLET BLOCKED** | The air outlet is obstructed: the fan is working too hard. Heater cut. | Remove anything that blocks the air outlet inside the chamber (blankets, clothes, objects). |

[PENDING: the messages of AIR THERMAL FAULT and SKIN THERMAL FAULT say "ALARM STAYS UNTIL RESTART", but in the current firmware they clear when the temperature drops. Waiting for the firmware fix.]

### Medium priority

| Alarm | What it means | What to do |
|---|---|---|
| **AIR TEMP HIGH** | In air mode, the air is more than 1 °C above the set temperature. Heater cut until it drops. | Check the infant. Check that the incubator is not in the sun or near a heat source, and that the room is not too warm. Keep the cover closed. |
| **AIR TEMP LOW** | In air mode, the air is more than 1 °C below the set temperature. | Check the infant. Close the cover and the access ports. Check that the room is not too cold or draughty. |
| **SKIN TEMP HIGH** | In skin mode, the skin is more than 1 °C above 36.5 °C. Heater cut until it drops. | Check the infant and measure the core temperature: the infant may have a fever. Check that the probe is correctly attached and not covered or exposed to heat. |
| **SKIN TEMP LOW** | In skin mode, the skin is more than 1 °C below 36.5 °C. | Check the infant and measure the core temperature. Check that the probe is correctly attached to the skin. Close the cover and the access ports. |
| **HEATER FAULT** | The start-up test found a fault in the heater. The incubator does not heat. Temperature control cannot be switched on. This alarm stays until the incubator is switched off. | Check the infant and move the infant to another warm environment. Phototherapy can still be used. Contact technical support. |
| **HEATER SENSOR FAULT** | The incubator cannot measure the heater current. Heater cut. | Check the infant. Contact technical support. |
| **MAINS INTERRUPTION** | The incubator has lost mains power. | Check the power cable and the socket. [PENDING: what happens on the V19 during a power cut, given that the battery is not included.] |
| **SUPPLY UNDERVOLTAGE** | The supply voltage is too low. The fan stops and temperature control is blocked. | Check the infant. Check the power supply and its cable. Contact technical support if it continues. |
| **DISPLAY LINK LOST** | The control board has lost contact with the screen. The values shown may not be reliable. | Check the infant. Switch the incubator off and on. |
| **SENSORBOARD LINK LOST** | The control board has lost contact with the sensor board. There is no air temperature reading. | Check the infant. Contact technical support. |
| **PHOTOTHERAPY LAMP FAULT** | The lamp should be on but does not draw current. The treatment is not being delivered. | Switch phototherapy off. Contact technical support. |

The screen can also show **BOARD LINK LOST**: the screen has lost contact with the control board for more than 5 seconds. The measured values disappear and **LINK LOST** flashes in the header. Check the infant, then switch the incubator off and on. You can silence this alarm from the alarm centre.

![Link lost](img/main_link_lost_chip_en.png)

### Low priority

| Alarm | What it means | What to do |
|---|---|---|
| **SKIN PROBE UNUSABLE** | In air mode, the skin probe was working and has stopped giving readings. Air control continues normally. | Check that the probe is attached and connected. |
| **HUMIDITY DEVIATION** | The humidity is more than 10 % away from the set value. | Check the water level in the humidifier tank. |
| **DOOR SENSOR SUSPECT** | The door sensor has changed state many times in a short time. It may be faulty. | Check that the cover closes properly. Contact technical support if it continues. |
| **HIGH NOISE** | The noise level has been 65 dB or higher for 10 seconds. | Reduce the noise near the incubator: voices, equipment, alarms of other devices. |

## 6.6 Heater or fan fault on the main screen

While **HEATER FAULT** or **FAN FAILURE** is active, the TEMPERATURE block disappears from the main screen: temperature control cannot be used. Phototherapy and the other functions can still be used. Open the alarm centre to see the active alarm.

![Main screen with a heater or fan fault](img/main_heater_error_en.png)

**HEATER FAULT** stays until the incubator is switched off. When technical support has solved the cause, switch the incubator off and on to return to normal operation.

[PENDING: the screen gives no message where the TEMPERATURE block was. A visible message has been proposed to the firmware team.]
