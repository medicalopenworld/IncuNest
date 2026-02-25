# Inventario de Hardware — IncuNest

> **Versión documento:** 2026-02-25

---

## 1. Versiones de Hardware

| Versión | MCU Motherboard | MCU Display | Estado |
|---|---|---|---|
| ≤ v14 | ESP32 (FireBeetle32) | — (pantalla ILI9341 integrada) | Legacy |
| v15+ | ESP32-S3 @ 240 MHz | ESP32-S3 (CrowPanel 7.0 Advance) | Actual |

---

## 2. Microcontroladores

### 2.1 Display_HMI — Elecrow CrowPanel 7.0 Advance

| Parámetro | Valor |
|---|---|
| MCU | ESP32-S3 |
| Frecuencia | 240 MHz |
| Flash | 16 MB (QIO, 80 MHz) |
| PSRAM | 8 MB OPI |
| Pantalla | TFT RGB 800×480 |
| Táctil | GT911 capacitivo |
| HMI name WiFi | `IncuNest_Display` |
| Firmware | `Display_HMI v1.0.5` |

### 2.2 Motherboard — FireBeetle32 / ESP32-S3

| Parámetro | v14 | v15+ |
|---|---|---|
| MCU | ESP32 FireBeetle32 | ESP32-S3 |
| Frecuencia CPU | 240 MHz | 240 MHz |
| Flash | 16 MB | 8 MB |
| Depuración | esp-prog | esp-prog |
| Particiones | `ESP32_OTA_partition_16MB.csv` | `ESP32S3_OTA_partition_8MB.csv` |

---

## 3. Sensórica

### 3.1 Temperatura y Humedad del Aire

| Sensor | Librería | Protocolo | Redundancia |
|---|---|---|---|
| **SHTC3** | `SparkFun_SHTC3_Arduino_Library` | I2C | Principal |
| **SHT4X** | `Adafruit_SHT4X` | I2C | Alternativo |

### 3.2 Temperatura Cutánea

| Sensor | Librería | Protocolo | Cantidad |
|---|---|---|---|
| **STS3X** (STS35) | `Sensirion arduino-i2c-sts3x` | I2C | `STS3X_NUM` instancias (redundante) |

### 3.3 Sensor NTC

- Temperatura NTC medida por ADC del ESP32 (`measureNTCTemperature()`)
- Usada para verificación y calibración

---

## 4. Instrumentación Médica

### 4.1 Pulsioxímetro — AFE4490

| Parámetro | Detalle |
|---|---|
| Chip | Texas Instruments AFE4490 |
| Librería | `Protocentral/protocentral-afe4490-arduino` |
| Medidas | SpO2 (saturación de oxígeno) + Frecuencia cardíaca |
| Interfaz | SPI |
| Archivo | `SPO2.cpp` |

---

## 5. Sistema de Energía

### 5.1 Cargador de Batería — BQ25792

| Parámetro | Detalle |
|---|---|
| Chip | Texas Instruments BQ25792 |
| Topología | Buck-Boost bidireccional |
| Librería | `andrew153d/BQ25792_Driver` |
| Celdas soportadas | 1 a 4 celdas Li-ion |
| Rango voltaje entrada | 3.6 V – 24 V |
| Interfaz | I2C |

### 5.2 Monitor de Potencia — INA3221

| Parámetro | Detalle |
|---|---|
| Chip | Texas Instruments INA3221 |
| Librería | `beast-devices/Arduino-INA3221` |
| Canales | 3 canales de corriente + voltaje |
| Instancias | 2 sensores (principal + secundario, direcciones INA3221_ADDR41_VCC e INA3221_ADDR40_GND) |
| Medidas | Corriente heater, fan, humidificador, USB, batería, foterapia |

---

## 6. Expansores I/O

| Chip | Librería | Protocolo | Uso |
|---|---|---|---|
| **TCA9555** | `RobTillaart/TCA9555` | I2C | Expansión GPIO general motherboard |
| **PCA9557** | `maxpromer/PCA9557-arduino` | I2C | Reset táctil GT911 en Display_HMI |

---

## 7. Pantalla e Interfaz HMI

| Componente | Detalle |
|---|---|
| Panel LCD | 800×480 px, bus RGB paralelo |
| Driver LCD | LovyanGFX (LGFX_ESP32_RGB + ILI9341 compatible) |
| Controlador backlight | STC8H1K28 @ I2C 0x30 |
| Comandos backlight | 246=Buzzer ON, 247=Buzzer OFF, 248=Speaker ON, 249=Speaker OFF |
| Panel táctil | GT911 capacitivo, I2C (SDA=15, SCL=16) |
| Resolución tactil | 800×480 px (rotación 3) |
| Framework UI | LVGL 8.3.11 |

### 7.1 Pantalla Legacy (≤v14)

- Driver: `Bodmer/TFT_eSPI` con `ILI9341_2_DRIVER`
- Pines: MISO=19, MOSI=23, SCLK=18, CS=15, DC=0
- Encoder rotatorio: `mathertel/RotaryEncoder.git`

---

## 8. Audio

| Componente | Detalle |
|---|---|
| Chip amplificador | Interno al módulo CrowPanel (vía STC8 @ 0x30, cmd 248) |
| Librería | `esphome/ESP32-audioI2S ^2.0.7` |
| Pines I2S | BCLK=5, LRCK=6, DOUT=4 |
| Formato | MP3 desde SPIFFS (`/sapphire.mp3`) |
| Rango volumen | 0–21 (guardado en EEPROM[251]) |
| Modo | forceMono=true |

---

## 9. Conectividad

| Módulo | Tecnología | Librería | Uso |
|---|---|---|---|
| ESP32 WiFi | 802.11 b/g/n | SDK nativo | OTA, ThingsBoard WiFi |
| GSM/GPRS | 2G (SIM800/A7670) | `TinyGSM` | Telemetría en zonas sin WiFi |
| MQTT | — | `ArduinoMqttClient` / `TBPubSubClient` | ThingsBoard IoT |

---

## 10. Actuadores

| Actuador | Control | Descripción |
|---|---|---|
| Calentador (Heater) | PWM (`HEATER_PWM_CHANNEL`) | Resistencia calefactora, controlada por PID |
| Humidificador | PWM time-slicing | MAM_in3ator_Humidifier, controlado por PID de humedad |
| Ventilador (Fan) | Activado por software | Activo durante control térmico y foterapia |
| LEDs Foterapia | PWM (`PHOTOTHERAPY_PWM_CHANNEL`) | Tira LED UV/azul para tratamiento de ictericia |
| Buzzer | GPIO / PWM | Sólo motherboard emite alarmas sonoras |

---

## 11. Almacenamiento

| Medio | Tamaño | Uso |
|---|---|---|
| Flash NOR (Motherboard) | 16 MB (≤v14) / 8 MB (v15+) | Código + EEPROM emulada |
| Flash NOR (Display_HMI) | 16 MB QIO | Código + SPIFFS (audio MP3) + EEPROM emulada |
| SPIFFS (Display_HMI) | Partición configurada en `IncuNest_display_v1_audio.csv` | `/sapphire.mp3` y otros archivos de audio |
| PSRAM (Display_HMI) | 8 MB OPI | Framebuffer LVGL, decodificación MP3 |
| EEPROM emulada (Display_HMI) | 256 bytes | Configuración persistente (ver ARQUITECTURA_FIRMWARE.md) |
