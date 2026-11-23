# 5. Using the incubator

This chapter explains how to care for an infant with IncuNest, step by step. Before you start, the incubator must be prepared and preheated (chapter 4). Chapter 3 describes the screens mentioned here.

## 5.1 Registering the baby

When you switch on temperature control, the incubator first asks for the baby's data. With it, the incubator proposes a suitable air temperature and keeps a record of the baby's care.

When you switch on phototherapy or humidity control and no baby is registered yet, the incubator asks only who the baby is (the first steps below), without proposing a temperature.

You can press **SKIP** at any step. The incubator then starts the function without a proposed temperature, in manual mode, and without linking it to a baby.

### Steps

1. **New or existing baby.** Choose a baby from the list, or press **NEW BABY**. The list shows the babies currently registered on this incubator (up to 3).

    ![New or existing baby](img/wizard_choose_baby_en.png)

2. **Baby name.** Type the name with the on-screen keyboard. Use letters only.

    ![Baby name](img/wizard_name_en.png)

3. **Gestational age (weeks).** Type the gestational age at birth, from 20 to 40 weeks.

    ![Gestational age](img/wizard_gest_age_en.png)

4. **Current weight (grams).** Weigh the baby and type the weight, from 400 to 5000 g. The field is always empty, so that you type today's weight and not an old one. If you cannot weigh the baby now, press **NO WEIGHT**; the incubator will not be able to propose a temperature.

    ![Current weight](img/wizard_weight_en.png)

5. **Age in days.** This step appears only when the incubator's clock is not set. Type the baby's age in days, from 0 to 365.

6. **Summary.** Check the proposal and press **APPLY**, or **CANCEL**.

    ![Summary with proposed air temperature](img/wizard_summary_range_en.png)

Use **BACK** to correct an earlier step.

If the screen shows "No response from board" or "Operation refused, try again", wait a few seconds and try again. If it keeps happening, see chapter 8.

::: warning
**WARNING** When the clock is set, the incubator counts the baby's age from the day the baby was **registered on this incubator**, not from the day of birth. For a baby registered some days after birth, the proposed air temperature may be too high. Check the proposal against the baby's real age. [PENDING: Pablo — confirm this behaviour and whether the wizard should ask for the date of birth.]
:::

## 5.2 Placing the infant

1. Tilt the mattress tray to the angle you need and make sure it is locked in position. [PENDING: how the tilt mechanism of the V19 works.]
2. Open the cover and place the infant gently on the mattress.
3. If you are going to use skin temperature control, attach the skin temperature probe to the infant's abdomen with an adhesive patch (see 5.4).
4. Close the cover and all access ports.

::: warning
**WARNING** Do not leave the infant unattended. Watch the infant regularly and check the infant's condition.
:::

## 5.3 Air temperature control

In air mode, the incubator keeps the **air** inside the chamber at the temperature you set.

1. On the main screen, touch the **AIR** panel to select air mode.
2. Press **TURN ON** in the TEMPERATURE block.
3. Complete the baby data (5.1). If the incubator could calculate a range, the summary shows the **proposed air temperature** and its range. Press **APPLY**: the set temperature becomes the middle of the range.
4. If you need a different value, change it with the up and down arrows next to **SET**: steps of 0.2 °C, from 30.0 °C to 39.0 °C.

![Air temperature control on](img/main_air_mode_on_en.png)

### How the proposed temperature is calculated

The proposed range depends on the gestational age, the current weight and the age in days. It comes from published thermal-neutrality tables (Sauer, Dane and Visser, 1984; Deacon and O'Neill, 2004; Cloherty; WHO/AAP). [PENDING: clinical validation of the table and full references.]

**Example:** a baby born at 30 weeks, weighing 1350 g, registered 4 days ago. The incubator proposes **33.0 °C to 34.0 °C** and sets **33.5 °C**.

The incubator cannot propose a temperature when:

- no weight was entered (**NO WEIGHT**);
- the gestational age is below 24 weeks;
- the table has no value for that combination (for example, babies of 36 weeks or more after their second week).

In these cases the summary says that there is no automatic range, and air mode starts at the set temperature, which you adjust by hand.

The proposal is a starting point. Medical staff decide the temperature for each infant.

## 5.4 Skin temperature control

In skin mode, the incubator controls the heating so that the infant's **skin** temperature stays at **36.5 °C**. This value is fixed and cannot be changed.

Before you start:

- Enable **SKIN MODE** in Settings > MODES (see 3.5). It can be enabled only with the skin temperature probe connected.
- Attach the probe to the infant's abdomen with an adhesive patch. The summary of the baby data wizard shows a picture of where to place it.

![Where to place the skin probe](img/wizard_summary_skin_sensor_en.png)

1. On the main screen, touch the **SKIN** panel to select skin mode.
2. Press **TURN ON** in the TEMPERATURE block and complete the baby data (5.1).

![Skin temperature control on](img/main_skin_mode_on_en.png)

::: warning
**WARNING** In skin mode, if the probe comes off or stops working, the incubator raises the alarm **SKIN PROBE FAULT** and cuts the heater. Check the infant, reattach or replace the probe, or switch to air mode. Check the infant's temperature with a clinical thermometer.
:::

::: warning
**WARNING** Do not use the skin temperature probe as a rectal probe.
:::

## 5.5 Humidity control

1. Make sure the humidifier water tank is filled (see 4.6).
2. Press **TURN ON** in the HUMIDITY block. If no baby is registered yet, identify the baby first.
3. Set the humidity with the up and down arrows: steps of 5 %, from 20 % to 90 %.

![Humidity control on](img/main_humidity_on_en.png)

If, after the first 30 minutes, the measured humidity is more than 10 % away from the set value, the incubator raises the alarm **HUMIDITY DEVIATION** (see chapter 6). Check the water tank.

[PENDING: recommended humidity by gestational age — clinical validation.]

## 5.6 Phototherapy

::: warning
**WARNING** Always protect the infant's eyes with opaque eye shields during phototherapy.
:::

Before you start:

- Do not apply creams, lotions or oils to the infant's skin.
- The infant should wear only a nappy, so that as much skin as possible is exposed to the light.

1. Press **TURN ON** in the PHOTOTHERAPY block. If no baby is registered yet, identify the baby first.
2. The incubator shows **EYE PROTECTION REQUIRED**. Put the eye shields on the infant, then press **TURN ON**.

    ![Eye protection message](img/photo_safety_popup_en.png)

3. The lamp switches on. To limit the treatment time, set it with **–** and **+** (from 120 to 600 minutes, in steps of 20 minutes) and press **START**. The button changes to **RUNNING**.
4. When the time is over, the lamp switches off by itself.

![Phototherapy timer running](img/main_photo_timer_running_en.png)

- To cancel the timer and keep the lamp on with no time limit, press the red **X**.
- To stop the treatment at any time, press **TURN OFF**.
- The time left is shown on the lock screen, next to **PHOTOTHERAPY**.

If the lamp does not draw current while it should be on, the incubator raises the alarm **PHOTOTHERAPY LAMP FAULT**: the treatment is not being delivered.

## 5.7 Pulse oximetry

::: warning
**WARNING** IncuNest is not a patient monitor. It shows SpO2, pulse rate and perfusion index, but raises no alarm for them. If the infant needs monitoring, use a separate monitor with alarms.
:::

1. Connect the pulse oximetry probe to its connector on the incubator (see 2.3).
2. Place the probe on the infant. [PENDING: which probe is supplied, where to place it (hand or foot) and how to fix it.]
3. Lock the screen, or wait 20 seconds. The lock screen shows the pulse wave, **PR**, **SpO2** and **PI %** while the probe is correctly applied.

![Vital signs on the lock screen](img/lock_screen_spo2_en.png)

If the probe is not applied correctly, or there is too much light on it, the values and the wave disappear from the screen. Check the probe and its position.

## 5.8 Oxygen

::: warning
**WARNING** Always use an oxygen analyser when you give oxygen to the infant. Oxygen increases the risk of fire: keep flammable substances and equipment that can produce sparks away.
:::

- Give supplementary oxygen only through the plastic inlet on the front panel.
- Giving oxygen can increase the noise level inside the incubator.

## 5.9 Following the baby

### Trends

From the alarm centre, press **TREND** to see the air temperature, skin temperature and humidity of the last 4, 12 or 24 hours. Each chart shows the measured value, the set value (violet line) and when alarms started and ended.

![Trend charts](img/trend_4h_en.png)

### Weighing reminder

If the weighing reminder is switched on (Settings > MAINTENANCE), the incubator reminds you to weigh the baby every 24 hours. The reminder appears when you unlock the screen.

- Press **WEIGH NOW**, type today's weight (400 to 5000 g) and press **REGISTER**.
- Press **LATER** to be reminded again in 24 hours.

![Weighing reminder](img/weighing_reminder_en.png)

### Registered babies

Open Menu > **REGISTERED BABIES** to see:

- **Active**: the babies currently registered on this incubator (up to 3), with their gestational age, kangaroo care and therapy times.
- **History**: babies who have been discharged, with the outcome.
- **Weight evolution**: the baby's weight curve by day of life.

![Registered babies](img/babies_list_en.png)

![Weight evolution](img/babies_weight_curve_en.png)

To register a baby without switching any function on, press **NEW BABY**, enter the name, gestational age and admission weight, and press **REGISTER**. If 3 babies are already active, discharge one first.

## 5.10 When the baby leaves the incubator

When you switch all functions off while a baby is registered, the incubator asks what has happened:

![Incubator idle](img/baby_exit_dialog_en.png)

- **WITH THE MOTHER**: the baby is out for kangaroo care and will come back. The baby stays registered.
- **DISCHARGE**: the baby leaves the incubator for good. Choose the outcome: **SURVIVED**, **DID NOT SURVIVE**, **TRANSFERRED** or **UNKNOWN**. If the baby did not survive, choose the cause.

You can also discharge a baby from Menu > **REGISTERED BABIES**, with the **DISCHARGE** button next to the baby.

After a discharge, clean the incubator completely (terminal cleaning, chapter 7).

## 5.11 Switching off

1. Switch off all functions on the main screen (**TURN OFF**).
2. Remove the infant from the incubator.
3. Press and hold the power button. A ring on the screen fills up; when it is complete, the incubator switches off. [PENDING: position and operation of the V19 power button and main switch — check on the bench.]
4. If the incubator will not be used or will be stored for a long time, disconnect the power supply cable.
