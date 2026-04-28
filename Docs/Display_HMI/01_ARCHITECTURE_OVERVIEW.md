# Visión General de la Arquitectura — Display HMI

## 1. Diagrama ASCII del Sistema Completo

```
┌─────────────────────────────────────────────────────────────────────┐
│                        SISTEMA INCUNEST                             │
│                                                                     │
│  ┌──────────────────────────┐        ┌──────────────────────────┐  │
│  │      MOTHERBOARD         │        │      DISPLAY HMI         │  │
│  │   (Control Module)       │        │   (Interface Module)     │  │
│  │                          │        │                          │  │
│  │  ┌───────────────────┐   │        │  ┌───────────────────┐   │  │
│  │  │  Sensores         │   │ UART   │  │  LVGL 8.3.11      │   │  │
│  │  │  - SHT4X (T+H)    │◄──┼────────┼──│  UI Engine        │   │  │
│  │  │  - NTC Piel       │   │115200  │  │                   │   │  │
│  │  │  - STS3X (Aire)   │   │  8N1   │  │  ┌─────────────┐ │   │  │
│  │  └───────────────────┘   │  \n    │  │  │ Pantallas   │ │   │  │
│  │                          │        │  │  │ - Intro     │ │   │  │
│  │  ┌───────────────────┐   │        │  │  │ - Lock      │ │   │  │
│  │  │  PID Controllers  │   │        │  │  │ - Main      │ │   │  │
│  │  │  - Temp. Aire     │   │        │  │  │ - Alarms    │ │   │  │
│  │  │  - Temp. Piel     │   │        │  │  │ - Charts    │ │   │  │
│  │  │  - Humedad        │   │        │  │  │ - PulseOxi  │ │   │  │
│  │  └───────────────────┘   │        │  │  │ - Settings  │ │   │  │
│  │                          │        │  │  └─────────────┘ │   │  │
│  │  ┌───────────────────┐   │        │  └───────────────────┘   │  │
│  │  │  Sistema Alarmas  │   │        │                          │  │
│  │  │  (9 alarmas)      │   │        │  ┌───────────────────┐   │  │
│  │  └───────────────────┘   │        │  │  Touch GT911      │   │  │
│  │                          │        │  │  I2C SDA:15 SCL:16│   │  │
│  │  ┌───────────────────┐   │        │  └───────────────────┘   │  │
│  │  │  GPRS + ThingsBoard│  │        │                          │  │
│  │  │  OTA WiFi         │   │        │  ┌───────────────────┐   │  │
│  │  └───────────────────┘   │        │  │  Backlight STC8   │   │  │
│  │                          │        │  │  I2C @0x30        │   │  │
│  │  ESP32-S3 (main board)   │        │  └───────────────────┘   │  │
│  └──────────────────────────┘        │                          │  │
│                                      │  ESP32-S3 (CrowPanel 7") │  │
│                                      └──────────────────────────┘  │
│                                                                     │
│                      ┌────────────────┐                            │
│                      │   USUARIO      │                            │
│                      │  (Enfermera /  │◄── Pantalla táctil 7"     │
│                      │   Médico UCI)  │                            │
│                      └────────────────┘                            │
└─────────────────────────────────────────────────────────────────────┘
```

## 2. Arquitectura Interna del Display HMI

```
┌────────────────────────────────────────────────────────────────────┐
│                     DISPLAY HMI — CAPAS INTERNAS                   │
│                                                                     │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │                    CAPA DE APLICACIÓN                       │   │
│  │                                                             │   │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────────┐  │   │
│  │  │   UITask     │  │  CommTask    │  │    OTA Task      │  │   │
│  │  │  Core 1      │  │  Core 1      │  │    Core 1        │  │   │
│  │  │  Prio: 2     │  │  Prio: 3     │  │    Prio: 4       │  │   │
│  │  │  Stack: 16KB │  │  Stack: 8KB  │  │    Stack: 8KB    │  │   │
│  │  │              │  │              │  │                  │  │   │
│  │  │ - LVGL render│  │ - Parse UART │  │ - WiFi AP        │  │   │
│  │  │ - Touch input│  │ - Send HMI   │  │ - Web server OTA │  │   │
│  │  │ - UI state   │  │ - Alarm sync │  │ - ThingsBoard    │  │   │
│  │  │ - Charts     │  │ - State sync │  │                  │  │   │
│  │  └──────┬───────┘  └──────┬───────┘  └──────────────────┘  │   │
│  │         │                 │                                  │   │
│  │         └────────┬────────┘                                  │   │
│  │                  │ g_lvgl_mutex (Recursive)                  │   │
│  └──────────────────┼──────────────────────────────────────────┘   │
│                     │                                               │
│  ┌──────────────────▼──────────────────────────────────────────┐   │
│  │                    CAPA LVGL 8.3.11                         │   │
│  │                                                             │   │
│  │  Tick: millis()       Refresh: 30ms      Color: RGB565      │   │
│  │  Draw buf: 24KB SRAM  Bounce buf: 64KB   FB: PSRAM          │   │
│  └──────────────────┬──────────────────────────────────────────┘   │
│                     │                                               │
│  ┌──────────────────▼──────────────────────────────────────────┐   │
│  │                    CAPA DE DRIVERS                          │   │
│  │                                                             │   │
│  │  ┌─────────────────┐  ┌──────────────┐  ┌───────────────┐  │   │
│  │  │  esp_lcd_panel_  │  │ TAMC_GT911   │  │  Preferences  │  │   │
│  │  │  rgb (IDF)       │  │ (I2C touch)  │  │  (NVS)        │  │   │
│  │  └─────────────────┘  └──────────────┘  └───────────────┘  │   │
│  │                                                             │   │
│  │  ┌─────────────────┐  ┌──────────────┐  ┌───────────────┐  │   │
│  │  │  PCA9557 (I2C   │  │ Serial CDC   │  │  I2C @0x30    │  │   │
│  │  │  IO expander)   │  │ (USB-UART)   │  │  (backlight)  │  │   │
│  │  └─────────────────┘  └──────────────┘  └───────────────┘  │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                                                                     │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │                    HARDWARE                                 │   │
│  │                                                             │   │
│  │  ESP32-S3 @ 240 MHz   16MB Flash QIO   8MB PSRAM OPI       │   │
│  │  Panel RGB565 800×480  Touch GT911     Backlight STC8@0x30  │   │
│  └─────────────────────────────────────────────────────────────┘   │
└────────────────────────────────────────────────────────────────────┘
```

## 3. Resumen Ejecutivo

El Display HMI de IncuNest está basado en el módulo **CrowPanel Advance 7"** (Elecrow) que monta un **ESP32-S3** con 16 MB de Flash y 8 MB de PSRAM OPI, conectado a un panel LCD de 800×480 píxeles mediante bus RGB paralelo de 16 bits.

El firmware corre bajo **Arduino 3.x** (PlatformIO) y gestiona tres tareas FreeRTOS:

- **UITask** (Core 1, Prio 2): Inicializa el display RGB, el touch GT911 y LVGL. Ejecuta el render de las 6+ pantallas de la UI y procesa eventos de toque. Corre el `lv_timer_handler()` en su bucle principal.
- **CommTask** (Core 1, Prio 3): Lee el puerto serie (USB CDC, 115200 baud) con mensajes de la Motherboard (`CTRL,STATE`, `CTRL,TEL`, `CTRL,ALM`). Envía comandos del usuario a la Motherboard (`HMI,...`). Sincroniza el estado de alarmas vía bitmask.
- **OTA Task** (Core 1, Prio 4): Gestiona WiFi en modo Access Point, servidor web para OTA, y conectividad ThingsBoard.

La comunicación entre UITask y CommTask usa un **mutex LVGL recursivo** (`g_lvgl_mutex`) que garantiza que solo una tarea acceda a los objetos LVGL simultáneamente.

## 4. Hardware — Especificaciones Técnicas

### 4.1 Microcontrolador

| Parámetro | Valor |
|---|---|
| MCU | ESP32-S3 (Xtensa LX7 dual core) |
| CPU | 240 MHz |
| Flash | 16 MB QIO (80 MHz) |
| PSRAM | 8 MB OPI (octal SPI) |
| Framework actual | Arduino 3.x (arduino-espressif32 53.03.10) |

### 4.2 Panel de Display

| Parámetro | Valor |
|---|---|
| Panel | CrowPanel Advance 7" (Elecrow v1.3/v1.4) |
| Resolución | 800 × 480 px |
| Interfaz | RGB paralelo 16 bits (RGB565) |
| Controlador | Sin controlador externo (RGB directo) |
| Pixel clock | 15 MHz (configurable 12–25 MHz via EEPROM) |
| Framebuffer | En PSRAM (`fb_in_psram=1`) |
| Bounce buffers | 2 × 20 líneas (2 × 32 KB en SRAM interna) |

### 4.3 Pines del Bus RGB

| Señal | GPIO | Señal | GPIO |
|---|---|---|---|
| B0 | 21 | G0 | 9 |
| B1 | 47 | G1 | 10 |
| B2 | 48 | G2 | 11 |
| B3 | 45 | G3 | 12 |
| B4 | 38 | G4 | 13 |
| R0 | 7 | G5 | 14 |
| R1 | 17 | HSYNC | 40 |
| R2 | 18 | VSYNC | 41 |
| R3 | 3 | DE | 42 |
| R4 | 46 | PCLK | 39 |

> **Nota**: Existe una inconsistencia entre `display_config.h` (valores correctos, versión 2.7) y `main.h` (valores legacy obsoletos). La nueva implementación DEBE usar únicamente `display_config.h`.

### 4.4 Touch Controller (GT911)

| Parámetro | Valor |
|---|---|
| IC | Goodix GT911 |
| Interfaz | I2C |
| SDA | GPIO 15 |
| SCL | GPIO 16 |
| INT | -1 (no conectado) |
| RST | -1 (gestionado por STC8H1K28) |
| I2C Address | 0x14 |
| I2C Freq | 400 kHz |
| Driver | TAMC_GT911_Fixed (librería local) |

### 4.5 Backlight Controller

| Parámetro | Valor |
|---|---|
| IC | STC8H1K28 (MCU auxiliar CrowPanel) |
| Interfaz | I2C |
| Address | 0x30 |
| Encender | Comando 0x0A (valor `DISPLAY_BL_ON_VALUE`) |
| Apagar | Comando 0xF5 (valor `DISPLAY_BL_OFF_VALUE`) |
| GPIO BL | GPIO 2 (no usado directamente) |

> El STC8H1K28 también gestiona el buzzer integrado de la placa (comandos 246/247/248/249).

### 4.6 Tabla de Particiones Flash

| Nombre | Tipo | Subtipo | Offset | Tamaño |
|---|---|---|---|---|
| nvs | data | nvs | 0x9000 | 20 KB |
| otadata | data | ota | 0xE000 | 8 KB |
| app0 (OTA_0) | app | ota_0 | 0x10000 | 3 MB |
| app1 (OTA_1) | app | ota_1 | 0x310000 | 3 MB |
| spiffs | data | spiffs | 0x610000 | 8 MB |
| coredump | data | coredump | 0xE10000 | 64 KB |

> Particiones definidas en `IncuNest_display_v1_audio.csv`.

## 5. Mapa de Memoria NVS/EEPROM del HMI

La EEPROM emulada (263 bytes) almacena configuración persistente del HMI:

| Dirección | Variable | Tipo |
|---|---|---|
| 0 | CHECK_STATUS | flag integridad |
| 10 | FIRST_TURN_ON | bool |
| 20 | AUTO_LOCK | bool |
| 30 | LANGUAGE | 0=ES, 1=EN, 2=FR |
| 40 | SERIAL_NUMBER | int32 |
| 60 | CONTROL_ACTIVE | bool |
| 80 | DESIRED_AIR_TEMP | double |
| 85 | DESIRED_SKIN_TEMP | double |
| 90 | DESIRED_HUMIDITY | double |
| 251 | AUDIO_VOLUME | uint8 (0-21) |
| 252 | DARK_MODE | uint8 (0/1) |
| 253 | DISPLAY_FREQ | uint32 (Hz, 4 bytes) |
| 257 | HUMIDITY_ENABLED | uint8 (0/1) |

> Ver archivo fuente: `include/EEPROM_defines.h`
