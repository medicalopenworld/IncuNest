# Tecnología y Configuración — IncuNest

> **Versión documento:** 2026-02-25

---

## 1. Entorno de Desarrollo

| Herramienta | Versión/Detalle |
|---|---|
| IDE | PlatformIO (VSCode extension) |
| Framework | Arduino (espressif32) |
| Plataforma Display_HMI | `espressif32 @ 6.3.2` |
| Plataforma motherBoard | `espressif32 @ 6.6.0` |
| Lenguaje | C++ (C++17) |
| Build dir | `C:\pio\.bld` (configurado fuera del repo) |

---

## 2. Stack de Tecnologías por Subsistema

### 2.1 Interfaz Gráfica (Display_HMI)

| Tecnología | Detalle |
|---|---|
| **LVGL 8.3.11** | Framework UI vectorial. Widgets: botones, switches, arcos, etiquetas, gráficas, imágenes |
| **LovyanGFX ^1.1.12** | Driver de bajo nivel para ESP32-S3 RGB paralelo. Alta velocidad DMA |
| **PSRAM 8 MB OPI** | Usada como framebuffer LVGL (`BOARD_HAS_PSRAM` definido) |
| Color depth | 16-bit RGB565 |
| Imágenes | Convertidas a arrays C (`.c`) en `src/ui_img_*.c` |

**Configuración LovyanGFX relevante:**
```
PCLK:       42   HSYNC:  39   VSYNC:  40   DE: 41
Freq:       12 MHz
PCLK_ACTIVE_NEG: 1 (flanco negativo)
DE_IDLE_HIGH:    1
Rotación:   2 (landscape)
```

### 2.2 Audio (Display_HMI)

| Tecnología | Detalle |
|---|---|
| **ESP32-audioI2S ^2.0.7** | Librería de audio I2S para ESP32. Decodifica MP3 desde SPIFFS |
| **SPIFFS** | Sistema de archivos en flash. Archivo: `/sapphire.mp3` |
| **AudioManager** (singleton) | Gestiona volumen (0-21), arranque, parada y configuración I2S |
| I2S pines | BCLK=5, LRCK=6, DOUT=4 (zona segura ESP32-S3) |
| Tarea audio | Core 0, prioridad 2, ciclo 3-loops+3ms para no interferir con DMA LCD |

**Particiones personalizadas** (`IncuNest_display_v1_audio.csv`):
- Incluye partición SPIFFS para almacenar el archivo de audio MP3

### 2.3 Control Térmico (motherBoard)

| Tecnología | Detalle |
|---|---|
| **Arduino-PID-Library** | 3 instancias PID: aire, piel, humedad |
| **Modo P_ON_E** | Proporcional sobre error (no sobre medida) |
| **Anti-windup** | Ki=0 cuando error > `anti_windup_offset` |
| **heaterSafeMAXPWM** | Límite dinámico ajustado por INA3221 en tiempo real |
| **Ciclo de humidificador** | Ventana temporal de 5000 ms con PWM por tiempo |

### 2.4 Conectividad IoT

| Tecnología | Protocolo | Plataforma |
|---|---|---|
| **ThingsBoard SDK v0.13.0** | MQTT | ThingsBoard Cloud |
| **ArduinoMqttClient** | MQTT | Cliente MQTT genérico |
| **TBPubSubClient 2.9.4** | MQTT PubSub | Backend MQTT ThingsBoard |
| **TinyGSM** | HTTP/TLS sobre GSM | Módulo 2G (SIM800/A7670) |
| **ArduinoHttpClient** | HTTP | Actualizaciones OTA HTTP |
| **ArduinoJson 6.21.5** | JSON | Serialización telemetría |

### 2.5 Sensores (motherBoard)

| Librería | Sensor | Función |
|---|---|---|
| `SparkFun_SHTC3` | SHTC3 | T + HR ambiente (principal) |
| `Adafruit_SHT4X` | SHT4X | T + HR ambiente (alternativo) |
| `Sensirion/arduino-i2c-sts3x` | STS3X (STS35) | T piel (redundante, múltiples instancias) |
| `Protocentral/protocentral-afe4490-arduino` | AFE4490 | SpO2 + HR cardíaco |
| `beast-devices/Arduino-INA3221` | INA3221 | Corriente + voltaje (2 chips) |
| `andrew153d/BQ25792_Driver` | BQ25792 | Gestión batería Li-ion |
| `RobTillaart/TCA9555` | TCA9555 | Expansión GPIO I2C |

---

## 3. Gestión de Memoria

### Display_HMI
- **PSRAM habilitada** (`board_build.arduino.memory_type = qio_opi`)
- `COLOR_DIVISOR = 15` → tamaño del draw buffer LVGL = pantalla / 15
- Imágenes LVGL almacenadas como arrays `const uint8_t` en PROGMEM

### motherBoard
- Stack por tarea: 4096 bytes (estándar) o 8192 bytes (GPRS, OTA)
- Mutex recursivo (`log_mutex`) para sincronizar Serial en contexto multi-tarea
- Semáforo binario (`GPRS_monitor_mutex`) para watchdog de tarea GPRS

---

## 4. OTA (Over-The-Air Update)

### Display_HMI OTA
- WiFi AP: `IncuNest_Display`
- mDNS habilitado para descubrimiento
- Servidor HTTP con WebServer de Arduino
- Ruta: `http://<ip>/update`
- Prioridad tarea OTA: 4 (más alta del Display, para interrumpir operación normal)

### motherBoard OTA
- También vía WiFi
- `EEPROM_PANIC_OTA_CHANGE` (addr 250): flag para forzar arranque en modo OTA tras crash

---

## 5. Seguridad y Robustez

| Mecanismo | Implementación |
|---|---|
| **Watchdog hardware** | `watchdogReload()` en `loop()` |
| **Monitor de tarea GPRS** | `GPRSMonitorTask` elimina la tarea GPRS si no responde |
| **Timeout RX serie** | Buffer limpiado si no llega `\n` en 50 ms |
| **Bloqueo de pantalla** | Auto-lock 20 s de inactividad (configurable EEPROM) |
| **Desbloqueo seguro** | Long-press 1.5 s con arco de progreso visual |
| **Alarmas con hysteresis** | Evita oscilaciones de alarmas en el umbral |
| **Heater corte inmediato** | PWM → 0 en cualquier alarma crítica |
| **Anti-windup PID** | Previene saturación integral en aceleración |
| **Cola de alarmas pendientes** | Hasta 10 alarmas encoladas si Display no está conectado al arrancar |

---

## 6. Idiomas Soportados

El sistema es completamente multiidioma. Idiomas disponibles:

| Código | Idioma | EEPROM_LANGUAGE |
|---|---|---|
| 0 | Español | 0 |
| 1 | English | 1 |
| 2 | Français | 2 |

El idioma se gestiona **localmente en el Display** (no se sincroniza desde el motherboard para evitar bucles). El motherboard recibe el idioma como parámetro de comando HMI y lo usa para localizar mensajes de alarma.

---

## 7. Configuración de Debug

### Display_HMI
```ini
build_flags = -D CORE_DEBUG_LEVEL=5   ; Log máximo ESP32
monitor_speed = 115200
```

### motherBoard
```ini
build_type = debug
build_flags = -g3 -O0 -D CORE_DEBUG_LEVEL=5
debug_tool = esp-prog
debug_init_break = tbreak setup
monitor_filters = esp32_exception_decoder  ; Decodifica stack traces automáticamente
```
