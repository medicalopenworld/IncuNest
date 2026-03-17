# IncuNest State Management

IncuNest relies on a robust centralized state machine that dictates the thermal, electronic, and visual behavior of the equipment. This global state is guarded by the Motherboard, which always holds the "final word" on the truth of the system (*Single Source of Truth*).

## 1. Key System Variables (Main Structure)

All vital context is encapsulated in a global `struct` named `in3ator_parameters` (commonly instanced as `in3`). This structure resides in the ESP32-S3 RAM of the Motherboard and is fragmentally copied into the RAM of the Display HMI.

### Key Operation Variables:
*   `actuation`: An integer/boolean flag denoting whether physical actuator relays, contactors, or heater PWM outputs are permitted to energize. Works as a software "Master Switch".
*   `controlMode`: Boolean defining if the central PID algorithm must pursue the air target (`CONTROL_AIR` = `true` / `1`) or the baby's skin target (`CONTROL_SKIN` = `false` / `0`).
*   `desiredAirTemperature` / `desiredSkinTemperature`: The setpoints in degrees Celsius input by the nurse.
*   `phototherapy`: Binary status and residual timer of the ultraviolet lights.
*   `skinModeEnabled`: Dependent flag determining whether the user has logical permission in the HMI to select Skin mode (e.g., conditional on the dermis probe being correctly inserted and reading logical values without short-circuits).

## 2. Thermal State Machine (PID Control Loops)

The incubator operates mathematically under closed-loop proportional, integral, and derivative (PID) control regimes.

1.  **Standby State (Idle)**: `in3.actuation == false`. The machine oversees telemetry (sensors, consumptions) but the PIDs do not inject `Kp`, `Ki`, nor `Kd` to the PWM actuators. Every calculation is discarded and forced to mathematical zero. Grave physiological alarms are omitted (crib cleaning or omitted initial heating phase).
2.  **AIR CONTROL State (Air Mode)**:
    *   **PID Input**: Temperature from the secondary digital cabin probe (STS3x).
    *   **SetPoint**: `in3.desiredAirTemperature`.
    *   **Output**: Routed to Heating Element PWM (Resistor).
3.  **SKIN CONTROL State (Skin Mode / Servocontrol)**:
    *   **PID Input**: Purified temperature returned by the analog peripheral `measureNTCTemperature()`.
    *   **SetPoint**: `in3.desiredSkinTemperature`.
    *   **Output**: Rerouted preferentially to Heating Element PWM, overpowering and sidelining air sub-loops (as long as hard hardware limits on the air are not surpassed to avoid thermal asphyxiation).
4.  **HUMIDITY CONTROL State (Parallel Loop)**:
    *   Totally independent of current Thermal mode (Air/Skin). Exerts its own routine injecting a Duty Cycle to the kettle/piezoelectric evaporator pin.

## 3. Temporary Retention (Volatile vs Non-Volatile Memory)

To protect the patient during sporadic reboots (Electrical brown-outs or MCU failures), certain facets of the state are backed up in the internal Flash Memory (emulating EEPROM).

*   **Persistently Retained Values**: Serial Number (`sn`), Absolute offset Calibrations, Cumulative module Usage Time (for preventative maintenance), Language Selection.
*   **Precautionary Volatility Values**: When the device is deliberately or abruptly turned off, the *SetPoints* and active mode (Skin/Air) can be rescued from the last save, but the machine will always boot in `ACTUATION = OFF` for safety reasons per standard IEC 60601-2-19, forcing the user to visually ratify the reconnection and manually trigger the heating (`actuation = ON`) from the HMI.

## 4. UI State and Cache Management (HMI Display)

Because the human is asynchronous and LVGL FPS demands swift redraws, the HMI employs *State Caching*:

1.  The doctor taps the temperature "+" button. In LVGL's volatile local cache, the `Label` goes up to "36.2°C" figuratively and its auxiliary variable rises to `36.2`.
2.  The screen blinks that number temporarily (indicating *Pending Approval*) and fires `HMI,....,36.2,...`.
3.  Meanwhile, the UI does not force a structural redesign.
4.  If milliseconds later, the Motherboard assimilates under its safety limits "OK, I accept 36.2", it dispatches `CTRL,STATE,....,36.2,...`.
5.  The HMI reabsorbs the STATE, confirms its cache matches, and "clamps" the number, solidifying the font without blinking. If the Motherboard had rejected it (e.g. crossing a Hard Limit), it would send `36.0°C`, forcing LVGL to track back and display `36.0°C`, rejecting the deceived human's intent.
