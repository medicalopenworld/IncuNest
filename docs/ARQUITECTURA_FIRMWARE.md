# Arquitectura de Firmware — IncuNest

> **Versión documento:** 2026-02-25  
> **FW Display_HMI:** v1.0.5  
> **Plataforma:** PlatformIO + Arduino Framework

---

## 1. Visión General del Sistema

IncuNest es una incubadora neonatal open-source de alta precisión diseñada para entornos de recursos limitados. El sistema se compone de **dos placas ESP32** independientes que se comunican por USB-CDC/UART serie:

```
┌──────────────────────────────────────────────┐
│         Elecrow CrowPanel 7.0 Advance        │
│               ESP32-S3 @ 240 MHz             │
│         Firmware: Display_HMI  v1.0.5        │
│  LVGL 8 · LovyanGFX · AudioI2S · ThingsBoard│
└────────────────┬─────────────────────────────┘
                 │ USB-CDC / UART 115200 bps
                 │ Protocolo ASCII + TLV-CRC16
┌────────────────▼─────────────────────────────┐
│         Placa madre (Motherboard)            │
│   FireBeetle32 (≤v14) / ESP32-S3 (v15+)     │
│            Firmware: motherBoard             │
│  PID · Sensores · GPRS · BQ25792 · INA3221  │
└──────────────────────────────────────────────┘
```

---

## 2. Firmware — Display_HMI

### 2.1 Hardware objetivo

| Componente | Detalle |
|---|---|
| MCU | ESP32-S3 (240 MHz, 16 MB Flash QIO, 8 MB PSRAM OPI) |
| Pantalla | TFT RGB 800×480, driver vía LovyanGFX/ILI9341 |
| Táctil | GT911 capacitivo, I2C (SDA=15, SCL=16) |
| Backlight | Controlador STC8H1K28 en dirección I2C 0x30 |
| Audio I2S | Altavoz externo, pines BCLK=5, LRCK=6, DOUT=4 |
| Expansor IO | PCA9557 (reset táctil) |

### 2.2 Estructura de tareas FreeRTOS

```
setup()
  ├─ UITask          (Core 1, prio 2)  — LVGL + pantallas LVGL
  ├─ CommTask        (Core 1, prio 3)  — Serie con motherboard
  ├─ OTA_Task        (Core 1, prio 4)  — WiFi + OTA
  └─ AudioTask       (Core 0, prio 2)  — Bucle I2S MP3 (3 loops + 3 ms delay)
```

> **Balance DMA:** El `AudioTask` usa ciclos cortos (3 loops + 3 ms) para evitar que el DMA de I2S interfiera con el bus RGB de la pantalla, eliminando el parpadeo de pantalla durante reproducción.

### 2.3 Módulos fuente principales

| Archivo | Responsabilidad |
|---|---|
| `main.cpp` | Setup, arranque de tareas |
| `UITask.cpp` | Lógica de pantallas, callbacks LVGL, foterapia, alarmas visuales |
| `ElementsCreation.cpp` | Creación de todos los objetos LVGL (widgets, paneles) |
| `CommTask.cpp` | Comunicación serie con el motherboard (envío/recepción) |
| `AudioManager.cpp` | Singleton de audio I2S, gestión de volumen con EEPROM |
| `EEPROM.cpp` | Init, carga y guardado de parámetros flash |
| `Wifi_OTA.cpp` | Conexión WiFi, servidor mDNS, actualización OTA |
| `buzzer.cpp` | Buzzer local (actualmente deshabilitado; sólo motherboard emite alarmas) |

### 2.4 Mapa EEPROM (256 bytes)

| Dirección | Variable | Rango/Tipo |
|---|---|---|
| 0 | `EEPROM_CHECK_STATUS` | Flag de integridad |
| 10 | `EEPROM_FIRST_TURN_ON` | Bool |
| 20 | `EEPROM_AUTO_LOCK` | Bool |
| 30 | `EEPROM_LANGUAGE` | 0=ES, 1=EN, 2=FR |
| 40 | `EEPROM_SERIAL_NUMBER` | int32 |
| 60 | `EEPROM_CONTROL_ACTIVE` | Bool |
| 65 | `EEPROM_PHOTOTHERAPY_ACTIVE` | Bool |
| 66 | `EEPROM_PHOTO_TIMER_MINUTES` | uint8 minutos |
| 70 | `EEPROM_CONTROL_MODE` | 0=Aire, 1=Piel |
| 80 | `EEPROM_DESIRED_AIR_TEMP` | double (×10 en raw) |
| 85 | `EEPROM_DESIRED_SKIN_TEMP` | double |
| 90 | `EEPROM_DESIRED_HUMIDITY` | double |
| 100 | `EEPROM_RAW_SKIN_TEMP_LOW_CORRECTION` | float |
| 115–175 | `EEPROM_WIFI_SSID/PASSWORD` | char[] |
| 200 | `EEPROM_THINGSBOARD_PROVISIONED` | Bool |
| 205 | `EEPROM_THINGSBOARD_TOKEN` | char[21] |
| 226–250 | Tiempos activos (standby, heater, fan, photo, humidifier) | uint32 flotante |
| 250 | `EEPROM_PANIC_OTA_CHANGE` | Bool |
| **251** | **`EEPROM_AUDIO_VOLUME`** | **uint8, rango 0–21** |

### 2.5 AudioManager

- **Singleton** con `getInstance()`
- Carga volumen desde EEPROM en `begin()` (dirección 251)
- Guarda volumen en EEPROM desde la UI con escritura diferida (evita bloqueos del bus flash que causan parpadeo)
- `playTone()` usa volumen `_volume` (no hardcodeado)
- Pines I2S fijos: BCLK=5, LRCK=6, DOUT=4 ("zona segura" del ESP32-S3)
- Rango de volumen: 0–21 (librería ESP32-audioI2S)

### 2.6 UI y pantallas

La interfaz gráfica usa **LVGL 8.3** con LovyanGFX como driver. Las pantallas principales son:

- **Splash/Intro** — Logo IncuNest + barra de carga (6 s)
- **Main Dashboard** — Temperatura aire/piel, humedad, gráficas tiempo real
- **Settings** — Control de temperatura/humedad, modo piel/aire, foterapia, volumen
- **Alarmas** — Hasta 4 alarmas simultáneas visibles; panel de alarma con card roja

Características UI:
- Bloqueo de pantalla automático (20 s inactividad, configurable)
- Desbloqueo por pulsación larga 1.5 s con arco de progreso animado
- Idioma persistente: Español / English / Français
- Foterapia con temporizador countdown configurable

---

## 3. Firmware — motherBoard

### 3.1 Hardware objetivo

| Versión HW | MCU | Board PIO |
|---|---|---|
| ≤ v14 | ESP32 (FireBeetle32) | `firebeetle32` |
| v15+ | ESP32-S3 @ 240 MHz | `esp32-s3-devkitc-1` |

### 3.2 Estructura de tareas FreeRTOS

```
setup()
  ├─ sensors_Task        (Core 1) — Lectura de todos los sensores
  ├─ security_Task       (Core 1) — Monitor de alarmas y seguridad
  ├─ buzzer_Task         (Core 1) — Generación de tonos de alarma
  ├─ GPRS_Task           (Core 1) — Conectividad GSM/GPRS + ThingsBoard
  │    └─ GPRSMonitorTask (Core 0) — Watchdog de GPRS (mata tarea si cuelga)
  ├─ OTA_WIFI_Task       (Core 1) — WiFi OTA + ThingsBoard WiFi
  ├─ Backlight_Task      (Core 1) — Control retroiluminación
  ├─ TimeTrack_Task      (Core 1) — Tracking de tiempos activos
  ├─ Communication_Task  (Core 1) — Envío datos al Display (v15+)
  ├─ Communication_Receiver (Core 1) — Recepción comandos del Display (v15+)
  └─ UI_Task             (Core 1) — Interfaz gráfica ILI9341 (sólo ≤ v14)
```

### 3.3 Módulos fuente principales

| Archivo | Responsabilidad |
|---|---|
| `main.cpp` | Setup, arranque de todas las tareas, `loop()` con watchdog |
| `PID.cpp` | 3 controladores PID: aire, piel y humedad |
| `security.cpp` | Sistema de alarmas completo (9 tipos, 3 idiomas) |
| `sensors.cpp` | Lectura SHTC3, SHT4X, STS3X, NTC |
| `initHardware.cpp` | Inicialización GPIO, I2C, PWM, BQ25792, INA3221 |
| `updateData.cpp` | Actualización periódica de telemetría y envío al display |
| `GPRS.cpp` | Módulo GSM/TinyGSM, conexión ThingsBoard MQTT |
| `CommTask.cpp` | Comunicación USB-CDC con Display_HMI (v15+) |
| `calibrateSensors.cpp` | Calibración de sensores de temperatura y humedad |
| `userInterface.cpp` | Menú gráfico en pantalla ILI9341 (≤ v14) |
| `EEPROM.cpp` | Gestión de parámetros persistentes |
| `security.cpp` | Verificación segura de alarmas con hysteresis |
| `Wifi_OTA.cpp` | WiFi + actualización OTA + ThingsBoard WiFi |

### 3.4 Control PID (PID.cpp)

Se usan **3 instancias** de `Arduino-PID-Library`:

| PID | Entrada | Salida | KP / KI / KD |
|---|---|---|---|
| `airControlPID` | `temperature[ROOM_DIGITAL_TEMP_SENSOR]` | `HeaterPIDOutput` → PWM heater | definidos en `main.h` |
| `skinControlPID` | `temperature[SKIN_SENSOR]` | `HeaterPIDOutput` → PWM heater | definidos en `main.h` |
| `humidityControlPID` | `humidity[ROOM_DIGITAL_HUM_SENSOR]` | Tiempo ON/OFF humidificador | definidos en `main.h` |

**Anti-windup:** Ki se anula mientras el error supera `anti_windup_offset`, minimizando overshoot.

**Protección de corriente:** `heaterPowerConsumptionCheck()` ajusta dinámicamente `heaterSafeMAXPWM` según la corriente medida por INA3221, protegiendo la fuente de alimentación.

---

## 4. Dependencias de Librerías

### Display_HMI (`platformio.ini`)

| Librería | Versión | Uso |
|---|---|---|
| `lovyan03/LovyanGFX` | ^1.1.12 | Driver LCD RGB |
| `lvgl/lvgl` | ^8.3.11 | Framework UI |
| `maxpromer/PCA9557-arduino` | ^1.0.0 | Expansor IO táctil |
| `moononournation/GFX Library for Arduino` | 1.2.8 | Soporte gráfico auxiliar |
| `esphome/ESP32-audioI2S` | ^2.0.7 | Audio MP3 I2S |
| `thingsboard/thingsboard-arduino-sdk` | v0.13.0 | Telemetría IoT |
| `bblanchon/ArduinoJson` | 6.21.5 | Serialización JSON |
| `ArduinoMqttClient` | latest | MQTT |

### motherBoard (`platformio.ini`)

| Librería | Versión | Uso |
|---|---|---|
| `br3ttb/Arduino-PID-Library` | latest | Control PID |
| `SparkFun SHTC3` | latest | Sensor T/H digital |
| `Adafruit SHT4X` | latest | Sensor T/H alternativo |
| `Sensirion arduino-i2c-sts3x` | latest | Sensor temperatura piel |
| `RobTillaart/TCA9555` | latest | Expansor IO 16-bit |
| `Protocentral AFE4490` | latest | Front-end pulsioximetría |
| `andrew153d/BQ25792_Driver` | latest | Cargador batería |
| `beast-devices/Arduino-INA3221` | latest | Monitor corriente/voltaje |
| `TinyGSM` | latest | Conectividad GSM/GPRS |
| `Bodmer/TFT_eSPI` | latest | Pantalla ILI9341 (≤v14) |
| `thingsboard-arduino-sdk` | v0.13.0 | IoT ThingsBoard |
| `ArduinoJson` | 6.21.5 | Serialización |

---

## 5. Pinout y Periféricos Clave

### Display_HMI — ESP32-S3

| Señal | GPIO |
|---|---|
| RGB DE (HENABLE) | 41 |
| RGB VSYNC | 40 |
| RGB HSYNC | 39 |
| RGB PCLK | 42 |
| Touch SDA | 15 |
| Touch SCL | 16 |
| Backlight PWM | 2 |
| I2S DOUT | 4 |
| I2S BCLK | 5 |
| I2S LRCK | 6 |

### Display — Resolución y timings

| Parámetro | Valor |
|---|---|
| Resolución | 800 × 480 px |
| PCLK | 12 MHz |
| HSYNC front/pulse/back | 8/4/8 |
| VSYNC front/pulse/back | 8/4/8 |
| PCLK activo | Flanco negativo |
| Rotación LCD | 2 (landscape) |
| Rotación touch | 3 |

---

## 6. Flujo de Arranque (Display_HMI)

```
1. setup()
   ├─ Inicializa LovyanGFX + LVGL
   ├─ Inicializa touch GT911 via I2C
   ├─ EEPROM.begin(256) → carga parámetros
   ├─ AudioManager::begin()
   │    ├─ Escaneo I2C (debug)
   │    ├─ Activa speaker (cmd 248 → STC8 @ 0x30)
   │    ├─ Monta SPIFFS y verifica /sapphire.mp3
   │    ├─ Configura pines I2S (BCLK=5, LRCK=6, DOUT=4)
   │    ├─ Lee volumen EEPROM[251] → aplica a audio.setVolume()
   │    └─ Lanza AudioTask en Core 0
   ├─ WiFi_OTA init
   └─ Lanza UITask + CommTask
```
