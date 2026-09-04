# IncuNest System Hardware

This document details the main physical components, sensors, and actuators managed by the IncuNest system firmware. The system is built on standard components and medical-grade sensors, coordinated by two main boards.

## 1. Processing Modules

### Motherboard (Main Control Board)
The control brain runs on a System-on-Chip (SoC) ranging from standard ESP32 to the newer **ESP32-S3** family, which offers a robust combination of Wi-Fi, Bluetooth connectivity, and multiple integrated peripherals on dedicated pins.

**Hardware Revisions:**
*   **v14 and older**: Uses ESP32 (FireBeetle32 board).
*   **v15 and newer**: Uses ESP32-S3 @ 240 MHz.

**Features in the system:**
*   Uses a real-time operating system (FreeRTOS) with task division between its two main cores (Core 0 for radio/network, Core 1 mainly for RTOS rigid loops and operations).
*   Has secondary GPRS/GSM connectivity embedded via the SIM800 module.
*   Connects to the Display and bridges communication using the intermediate **CH340C** chip configured with UART/USB.
*   Uses an ADC with integrated and pre-processed filters for conditioning thermistors (NTC) connected to isolated millivolt voltage inputs.
*   Monitors main supply voltages and currents (12V and 5V) using INA3221 I2C shunts and converters.

### Display HMI (Elecrow CrowPanel Advance 7.0)
The user interface operates on specialized Elecrow hardware, also based on the **ESP32-S3**. Its main mission and focus are audiovisual peripherals.

**Hardware Specifications & Pinouts:**
*   **MCU**: ESP32-S3 (240 MHz, 16 MB Flash QIO, 8 MB PSRAM OPI).
*   **RGB LCD Timings**: 800x480 px resolution. PCLK at 12 MHz (Negative edge). HSYNC/VSYNC pulses (Front: 8, Pulse: 4, Back: 8).
*   **RGB Pins**: DE=41, VSYNC=40, HSYNC=39, PCLK=42.
*   **Touch Interface**: GT911 Capacitive. Pins I2C (SDA=15, SCL=16).
*   **Audio I2S Pins**: BCLK=5, LRCK=6, DOUT=4.
*   **Backlight Output**: PWM on GPIO 2 (driven via STC8H1K28 controller at I2C 0x30).

**Features in the system:**
*   7-inch full-color capacitive touch panel (800x480 resolution).
*   Heavy payload via a parallel RGB bus/high-speed DMA interface to achieve refresh rates around 25-30 frames per second in the LVGL v8+ environment.
*   Has its own dedicated I2S amplifier hardware.
*   Houses a dedicated IO expander processor (usually PCA9557) managing VSYNC, HSYNC signals, and backlight enables, at fixed I2C addresses.

## 2. Sensors (Redundant and Telemetry)

The project relies on reliable, stable, and accurate collection of the crib environment and patient biometric variables, implemented across different buses and standards.

### SHT4x (Ambient or External Temperature and Humidity)
*   **Interconnection**: Primary I2C bus (standard 0x44 address).
*   **Purpose**: Ambient sensing to compensate for external room/chamber variables. Used in non-critical logic.

### Sensirion STS3x and SHTC3 (Cabin Temperature and Humidity / Air Control)
*   **Interconnection**: Main machine I2C bus (commonly managed at addresses 0x4A and 0x4B).
*   **Purpose**: Read at constant intervals (approx. 5 Hz to 1 Hz, and filtered at a simulated ~ 1000 Hz) using fault-tolerant algorithms. A **MAIN** and a **REDUNDANT** sensor are managed.
*   If the routine discovers a sudden disparity, protection logic and "Air Sensor Issue" alarms are routed.

### Blood Oxygen and Pulse Infrared Sensor (SPO2)
*   Integrated into the ecosystem, it uses high-speed buses that publish biometric data.

### NTC Sensor for Patient Skin (Skin Sensor)
*   **Interconnection**: External connector and channel routed to native ADC conversion.
*   **Purpose**: Resistive reading of negative temperature coefficient for intimate measurement of the neonatal dermis. Relying on the ESP32S3 SoC, it uses extensive calculation (Beta factor equations) smoothed with noise via Butterworth filters in the `measureNTCTemperature()` routine.

## 3. Actuators

The physical mechanisms that alter the energy parameters of the incubator upon orders from the `PID` commands dispatched in the firmware.

1.  **Heating Block (Heater)**
    *   **Control**: Linear Pulse Width Modulation (PWM) output with hardware-based upper limit routines to prevent melting (HEATER_MAX_PWM). Base modulation according to the PID algorithm.
    *   **Monitoring**: Its internal consumption (current drawn) is audited, again using I2C to dedicated INA3221 channels to confirm thermal wire faults ("Heater Issue Alarm").
2.  **Humidifier System (Evaporative or Rapid)**
    *   Has a dedicated API (`in3ator_humidifier.cpp`) that coordinates heating duty cycles or release of relative value (%RH) to respond to the outputs of the `humidityControlPIDOutput` module.
3.  **Main Fan (Air Fan)**
    *   In charge of continuous flow and passive internal convection of the machine. Incorporates actual RPM measurement (via derived *Encoder/Pulse* wire), allowing the machine to detect a drop or jam in the rotor and compulsorily stop the heater dissipation, invoking the "FAN_ISSUE_ALARM" alert as a critical safety factor.
4.  **Therapeutic Light Matrix (Phototherapy)**
    *   Asynchronous triggering ordered from the Motherboard with a "Timer Mode" or "Continuous" freely willed by the doctor from the local UI, which emits medical UV peaks using peripheral hardware without interfering with direct temperature.

## 4. Physical and Diagnostic Connectivity

Most real-time buses are operated discretely:

*   **General I2C Bus**: Routes typical addresses for TCA9555 expanders, Backlight at 0x30, INA3221 Voltage Sensors (0x40 and 0x41), and capacitive interface.
*   **CH34x Chip**: This special controller manages a key USB-to-Serial transition acting as an umbilical cord from the Motherboard to the HMI Display on the 115200 8N1 interface. To deal with intrinsic communication hangs, DTR/RTS pins are used precisely.
*   **Flash and OTA:** Physical firmware update is enabled as well as via GPRS or WiFi, partitioning the hardware to host full banks of fail-safe copies.

## 5. Factory Test Coverage (motherBoard)

Besides the boot self-test in `initHardware()` (standby current, buzzer,
sensors, actuators), the motherBoard runs a 30-step factory battery on request
from the display (`HMI,FTEST,START`, see `Firmware/PROTOCOL.md` § 3). It lives
in `src/modules/factory_test/` and reuses `actuatorsTest()` and
`testStandByCurrent()` inside an explicit safe state (alarms inhibited, PIDs
in MANUAL, `PIDHandler()`/`turnFans()` gated by `g_factoryTestActive`, all PWM
at 0), restored unconditionally afterwards. It refuses to start while thermal
control or phototherapy is active.

| Area | Tests | Evidence |
|---|---|---|
| Power | INA3221 ×2 presence, standby current, BQ25730 readings, mains/battery | I2C ACK, `HW_error` bits, `charge_status()` |
| On-board sensors | ADS1110 + skin NTC, external SHT4x | I2C ACK, plausible ranges, `skinProbeLastReading()` |
| Cabin sensor | fresh, valid cabin temperature by either path: SensorBoard over USB (`usb`) or STS35/SHTC3 over I2C2 (`i2c`) | `sensorSourceGet()`, `sensorboard_comm` snapshot, `updateRoomSensor()`. A false ACK from the I2C2 probe on the USB lines shows up as `i2c sin datos` |
| SensorBoard (USB) | `status` availability of sht0/1/2, ALS, Hall, camera; 3×SHT40 coherence (≤ 1.0 °C spread, ≤ 3.0 °C vs external); door open/close; light drop; JPEG capture | `sensorboard_comm` snapshot and `status`/`capture` requests; skipped (hidden) when the cabin sensor came over I2C2 |
| Actuators | heater, phototherapy, fan currents; fan RPM; humidifier USB switch (`USB_EN`/`USB_FAULT`, current) | `actuatorsTest()`, INA3221 channels |
| Buzzer | dBA rise measured by the SensorBoard microphone; operator confirmation if no microphone | `sound_level` events |
| SpO2 | AFE4490 timing registers read back over SPI; probe attached (optional) | `getTimingConfig()`, `runAfeDiagnostics()`, `probe_state` |
| Communications | HMI link; GSM modem answering AT, SIM `+CPIN: READY`, CSQ, network attach; WiFi to the default AP; ThingsBoard session with provisioned token; wall clock | state already collected by `GPRS_Task` and the WiFi task, read passively. Attach, WiFi, ThingsBoard and clock end as **WARNING** (amber, not a board fault) after 30 s without environment |
| Storage | NVS write/read, LittleFS mount | `Preferences`, `LittleFS.begin()` |

Deliberately **not** tested: the TCA9535 expander and the rotary encoder
(vestigial code with no hardware behind it on this board), buzzer current
(only measurable on HW ≤ 16), `HW_NUM` and the ON/OFF latch (if the board
booted and runs the test, both worked), and the display backlight cycle.

Results are persisted in NVS namespace `mb_ftest` (epoch, PASS/FAIL/RUN
masks, firmware versions of the motherBoard and the SensorBoard).
