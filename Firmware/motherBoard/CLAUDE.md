# CLAUDE.md — IncuNest Firmware (motherBoard)

## Proyecto

**IncuNest** es una incubadora neonatal open-source desarrollada por Medical Open World.
Repositorio: https://github.com/medicalopenworld/IncuNest/tree/master/Firmware

- **Hardware:** Placa propietaria ESP32-S3 (revisión V15) + AFE4490 (SpO2/PPG)
- **Framework:** PlatformIO + Arduino + ESP-IDF + FreeRTOS
- **Entornos de build:**
  - `in3ator_UP_TO_V14` — FireBeetle32 / ESP32
  - `in3ator_V15` — ESP32-S3-DevKitC-1

## Arquitectura FreeRTOS (tasks)

| Task | Descripción |
|------|-------------|
| `sensors_Task` | NTC piel, STS3x/SHTC3 aire, INA3221 consumo |
| `PID_Task` | Control calentador y humidificador |
| `UI_Task` | Pantalla TFT (TFT_eSPI) + encoder rotativo |
| `Security_Task` | **PRIORIDAD CRITICA** — alarmas térmicas, fallos ventilador, sensores |
| `Comm_Tasks` | WiFi, GPRS (SIM800), ThingsBoard SDK (IoT) |

## Ficheros clave

| Fichero | Rol |
|---------|-----|
| `include/main.h` | Diccionario global: pines, constantes, structs, flags de debug |
| `include/board.h` | HAL de pines por versión de hardware |
| `src/main.cpp` | Setup + lanzamiento de tasks FreeRTOS |
| `src/security.cpp` | Alarmas y protección del paciente |
| `src/PID.cpp` | Control temperatura/humedad |
| `src/sensors.cpp` | HAL sensores I2C/analógicos |
| `src/updateData.cpp` | Estados globales y logs (logI, logE, logAlarm) |
| `src/GPRS.cpp` | Conectividad celular y telemetría |

## Stack tecnológico

- **Gráficos:** TFT_eSPI, Adafruit_GFX
- **IoT/Nube:** ThingsBoard SDK + ArduinoJson
- **Celular/GSM:** TinyGSM (SIM800)
- **Sensores:** Beastdevices_INA3221, SHT4x, STS3x
- **Almacenamiento:** NVS (non-volatile) + EEPROM

## Reglas de oro (obligatorias)

1. **Dispositivo medico:** fiabilidad es prioridad #1 — nunca sacrificar seguridad
2. **No bloqueante:** prohibido `delay()`, usar siempre `vTaskDelay()`
3. **Thread-safe:** usar `log_mutex` en todos los recursos compartidos entre tasks
4. **Robustez I2C:** manejar siempre errores de comunicación I2C
5. **Pines:** seguir estrictamente los definidos en `include/main.h` y `include/board.h`
6. **Sin over-engineering:** cambios mínimos y focalizados; dispositivo en producción
