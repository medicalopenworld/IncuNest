# IncuNest System Architecture

The **IncuNest** system employs a **distributed architecture** composed of two main microcontrollers, designed to separate critical control responsibilities from intensive user interface tasks. This separation guarantees maximum safety, real-time stability, and a smooth, uninterrupted user experience.

## 1. System Overview

The system is divided into two physical and logical modules interconnected via a robust asynchronous serial link:

1.  **Motherboard (Control Module)**: Acts as the real-time brain of the incubator. It is primarily responsible for patient safety, closed-loop temperature and humidity control, biometric sensor reading, and hardware activation (heating, ventilation, phototherapy).
2.  **Display HMI (Human-Machine Interface)**: It is the visual and tactile interface for the end user (medical staff). Its only responsibility is to render the graphical interface, play audio alarms, record user interactions, and transmit commands to the Motherboard.

Both systems are designed to operate semi-independently; if the HMI fails or restarts, the Motherboard continues to execute its control loops and safety schemes without interruption.

## 2. Relationship between Motherboard and HMI

The relationship follows an asynchronous model based on asynchronous message passing, prioritizing the Motherboard as the single "Source of Truth" regarding the hardware state and the active patient configuration.

*   **The Motherboard commands**: The HMI assumes that the Motherboard is always right about the actual state of the equipment. The Motherboard periodically sends telemetry (sensor state) and state confirmations (`STATE`).
*   **The HMI suggests**: When a user interacts with the screen (e.g., changes a target temperature or language), the HMI sends a command to the Motherboard. The HMI **does not assume** that the change is effective until the Motherboard returns a confirmation (upon receiving a new `STATE` message reflecting the changes).
*   **Fault Management**: The system includes deep mechanisms to recover from temporary USB/Serial disconnections, employing critical *Handshakes* (`HMI,UI_READY`) and synchronizing alarm bitmasks every second to ensure no pending alarm goes unnoticed due to packet loss.

## 3. Logical Block Diagram

The logical flow of the main components is described textually below:

### Motherboard (Main MCU)
- **Sensor Layer**: Continuously reads cabin temperature and humidity data, as well as patient sensors (Skin NTC) and biometrics (SPO2). Applies Butterworth filters (6th order) to ensure cleanliness in raw measurements and handles physical redundancy if main sensor failures are detected.
- **Security and Alerts Task**: Monitors extreme conditions (Thermal Cutouts, disconnections, overcurrent) independently and assertively, feeding the central state machine to trigger SOS signals or turn off actuators in emergencies.
- **PID and Actuation Modules**: Calculates and adjusts the power supplied to the thermal heating resistor and evaporators via PWM, using independent PID controllers for Skin, Air, and Humidity.
- **Communication Transceiver (COMM_HOST)**: Sends all telemetry, the consolidated machine state (configuration values + flags), and triggered alarm identifiers via CDC-ACM to the Display every second or upon forced changes.

### Communication Link
- Physical Link: Serial bridge via CH340 / USB UART at 115200 baud.
- Asymmetrical Messaging: High volume of MB -> HMI sending, with bursts caused by HMI -> MB events.

### Display HMI (Interface MCU)
- **Graphics Engine (LVGL)**: Dedicated task (`UITask`) that runs a loop rendering received changes. Updates widgets, charts, and animations. Keeps provisional settings cached until the Motherboard accepts them.
- **I2S Audio Control (`AudioManager`)**: Instance of non-blocking DMA queues to emit parallel audible medical patterns without tying up graphics resources.
- **Communication Transceiver (`CommTask`)**: Parses Motherboard commands without blocking Core 1, processing Handshakes (`STATE`, `TEL`, `ALM`), calculating resynchronizations (by *alarm bits*), and forwarding touch actions to the Motherboard as `HMI,...` statements.

## 4. FreeRTOS Task Structure

### Display_HMI
```text
setup()
  ├─ UITask          (Core 1, prio 2)  — LVGL + UI screens
  ├─ CommTask        (Core 1, prio 3)  — Serial with motherboard
  ├─ OTA_Task        (Core 1, prio 4)  — WiFi + OTA updates
  └─ AudioTask       (Core 0, prio 2)  — I2S MP3 playback loop (3 loops + 3 ms delay)
```
*Note: `AudioTask` uses short cycles to prevent the I2S DMA from interfering with the screen's RGB bus, eliminating screen flickering during playback.*

### Motherboard
```text
setup()
  ├─ sensors_Task        (Core 1) — Reads all sensors
  ├─ security_Task       (Core 1) — Alarm and safety monitor
  ├─ buzzer_Task         (Core 1) — Alarm tone generation
  ├─ GPRS_Task           (Core 1) — GSM/GPRS connectivity + ThingsBoard
  │    └─ GPRSMonitorTask (Core 0) — GPRS watchdog (kills task if hanging)
  ├─ OTA_WIFI_Task       (Core 1) — WiFi OTA + ThingsBoard WiFi
  ├─ Backlight_Task      (Core 1) — Backlight control
  ├─ TimeTrack_Task      (Core 1) — Active times tracking
  ├─ Communication_Task  (Core 1) — Sending data to Display (v15+)
  ├─ Comm_Receiver       (Core 1) — Receiving Display commands (v15+)
  └─ UI_Task             (Core 1) — ILI9341 GUI (only for hardware ≤ v14)
```

## 5. Non-Volatile Memory (EEPROM Map)

The system uses 256 bytes of emulated EEPROM in flash memory to store persistent configurations.

| Address | Variable | Range/Type |
|---|---|---|
| 0 | `EEPROM_CHECK_STATUS` | Integrity Flag |
| 10 | `EEPROM_FIRST_TURN_ON` | Bool |
| 20 | `EEPROM_AUTO_LOCK` | Bool |
| 30 | `EEPROM_LANGUAGE` | 0=ES, 1=EN, 2=FR |
| 40 | `EEPROM_SERIAL_NUMBER` | int32 |
| 60 | `EEPROM_CONTROL_ACTIVE` | Bool |
| 65 | `EEPROM_PHOTOTHERAPY_ACTIVE` | Bool |
| 66 | `EEPROM_PHOTO_TIMER_MINUTES` | uint8 (minutes) |
| 70 | `EEPROM_CONTROL_MODE` | 0=Air, 1=Skin |
| 80 | `EEPROM_DESIRED_AIR_TEMP` | double (×10 raw) |
| 85 | `EEPROM_DESIRED_SKIN_TEMP` | double |
| 90 | `EEPROM_DESIRED_HUMIDITY` | double |
| 100 | `EEPROM_RAW_SKIN_TEMP_LOW_CORRECTION` | float |
| 115–175 | `EEPROM_WIFI_SSID` / `PASSWORD` | char[] |
| 200 | `EEPROM_THINGSBOARD_PROVISIONED`| Bool |
| 205 | `EEPROM_THINGSBOARD_TOKEN` | char[21] |
| 226–250 | Active working times (standby, heater, fan, photo, humidifier) | uint32 (floating) |
| 250 | `EEPROM_PANIC_OTA_CHANGE` | Bool |
| **251** | **`EEPROM_AUDIO_VOLUME`** | **uint8, range 0–21** |

## 6. Software Stack & Dependencies

### Development Environment
*   **IDE**: PlatformIO (Arduino Framework `espressif32`)
*   **Language**: C++17

### Display_HMI
*   **LVGL 8.3.11**: Vector UI framework (uses 8MB PSRAM OPI).
*   **LovyanGFX ^1.1.12**: Low-level high-speed parallel RGB DMA LCD driver.
*   **ESP32-audioI2S ^2.0.7**: I2S audio library for decoding MP3s from SPIFFS partition (`/sapphire.mp3`).
*   **PCA9557-arduino**: IO touch expander.

### Motherboard
*   **Arduino-PID-Library**: 3 PID instances (air, skin, humidity) with Anti-windup.
*   **TinyGSM** / **ThingsBoard SDK**: Remote IoT telemetry.
*   **Arduino-INA3221**: Current/Voltage monitor.
*   **Sensors**: `SparkFun SHTC3`, `Adafruit SHT4X`, `Sensirion STS3X`.
*   **BQ25792_Driver**: Li-ion battery charging management.
