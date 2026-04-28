# Inventario de Dependencias PlatformIO y Equivalentes ESP-IDF

> Fuentes: `platformio.ini`, `idf_component.yml`, `lib/`, `include/`

## 1. Tabla de Dependencias

| Librería PlatformIO | Versión | Función en HMI | Equivalente ESP-IDF / IDF Component | Notas de migración |
|---|---|---|---|---|
| `maxpromer/PCA9557-arduino` | ^1.0.0 | Driver I2C para expansor I/O PCA9557 (touch reset) | Implementar directo con `i2c_master` IDF o `espressif/i2c_bus` | El PCA9557 no fue encontrado en un scan I2C (comentario en UITask.cpp). Verificar si realmente existe en el hardware v1.3/v1.4. |
| `lvgl/lvgl` | ^8.3.11 | Motor gráfico completo (UI, widgets, touch) | `lvgl/lvgl` via `idf_component_manager` — versión: `"^8.3.11"` | Migración directa. Cambiar tick de `millis()` a `esp_timer_get_time() / 1000`. |
| `thingsboard/thingsboard-arduino-sdk` | v0.13.0 | Telemetría IoT al servidor ThingsBoard | **NO necesario en HMI** — ThingsBoard lo gestiona la Motherboard | Eliminar completamente del HMI. |
| `bblanchon/ArduinoStreamUtils` | latest | Dependencia transitiva de ThingsBoard | **NO necesario en HMI** | Eliminar. |
| `vshymanskyy/StreamDebugger` | latest | Dependencia transitiva de ThingsBoard | **NO necesario en HMI** | Eliminar. |
| `arduino-libraries/ArduinoMqttClient` | latest | MQTT para ThingsBoard (ignorado en build) | **NO necesario en HMI** | Eliminado (`lib_ignore = ArduinoHttpClient`). Ya no compilado. |
| `bblanchon/ArduinoJson` | 6.21.5 | Serialización JSON para ThingsBoard | **NO necesario en HMI** | Eliminar. |
| `thingsboard/TBPubSubClient` | 2.9.4 | PubSub MQTT para ThingsBoard | **NO necesario en HMI** | Eliminar. |
| `TAMC_GT911_Fixed` (local) | fork custom | Driver touch Goodix GT911 | `espressif/esp_lcd_touch_gt911@^1.0.0` (IDF Component) | Migración a componente oficial. Configurar con `esp_lcd_touch_config_t`. |
| `esp_lcd_panel_rgb` (built-in) | IDF 5.x | Driver RGB paralelo del display | `esp_lcd` (built-in IDF) — sin cambios | Ya es IDF nativo. Migración sin cambios de configuración. |
| `FreeRTOS` (built-in) | IDF | RTOS — tareas, mutex, colas | `freertos` (built-in IDF) | Sin cambios. `xSemaphoreCreateRecursiveMutex` disponible en IDF. |
| `Preferences` / `EEPROM` (built-in Arduino) | Arduino | NVS: almacenamiento persistente | `nvs_flash` (built-in IDF) | Cambiar a API `nvs_flash_init()` / `nvs_open()` / `nvs_set_*()`. |
| `WiFi` (built-in Arduino) | Arduino | WiFi AP para OTA | `esp_wifi` (built-in IDF) | Cambiar a `esp_wifi_init()`, `esp_netif`. |
| `WebServer` (built-in Arduino) | Arduino | Servidor HTTP para OTA | `esp_http_server` (built-in IDF) | Migrar a `httpd_start()` / handlers. |
| `Update` (built-in Arduino) | Arduino | OTA vía HTTP | `esp_ota_ops.h` (built-in IDF) | Cambiar a `esp_ota_begin()` / `esp_ota_write()` / `esp_ota_end()`. |
| `Wire` (built-in Arduino) | Arduino | Bus I2C para touch y backlight | `i2c_master` (built-in IDF) | Cambiar a `i2c_master_bus_create()` y transacciones I2C. |
| `esp_log` (built-in) | IDF | Logging | `esp_log` (built-in IDF) — sin cambios | Sin cambios. |

## 2. Dependencias Reales para el Nuevo Proyecto ESP-IDF

El nuevo proyecto ESP-IDF solo necesita las siguientes dependencias (las de ThingsBoard/MQTT son exclusivas de la Motherboard):

```yaml
# idf_component.yml para el nuevo Display_HMI ESP-IDF
dependencies:
  lvgl/lvgl:
    version: "^8.4.0"
    rules:
      - if: "idf_version >= 5.0"
  espressif/esp_lcd_touch_gt911:
    version: "^1.0.0"
  idf:
    version: ">=5.1.0"
```

### 2.1 Componentes IDF Built-in necesarios

Los siguientes son parte de ESP-IDF y no necesitan declararse en `idf_component.yml`:

- `esp_lcd` — driver RGB panel
- `nvs_flash` — almacenamiento NVS
- `esp_wifi` — WiFi
- `esp_http_server` — servidor OTA
- `esp_ota_ops` — actualización OTA
- `freertos` — RTOS
- `esp_log` — logging
- `driver/i2c_master` — bus I2C para GT911 y backlight

## 3. Análisis de Dependencias ThingsBoard en el HMI

La inspección del código revela que **todas las dependencias de ThingsBoard están referenciadas únicamente en `Wifi_OTA.cpp`** y están protegidas por `#ifdef` condicionales. El HMI **no envía telemetría directamente** a ThingsBoard — esa función la gestiona la Motherboard.

Dependencias de ThingsBoard en el HMI actual:
- `thingsboard-arduino-sdk` — referenciado en `Wifi_OTA.cpp` para OTA remota via servidor ThingsBoard
- Actualmente con `build_src_filter = -<AudioManager.cpp>` — sugiere que parte del código está siendo excluido

> **Recomendación**: En la nueva implementación ESP-IDF, eliminar todas las dependencias ThingsBoard del HMI. La OTA del HMI debe gestionarse exclusivamente via WiFi local (servidor HTTP) o mediante la Motherboard como relay.

## 4. Impacto de la eliminación de ThingsBoard del HMI

| Efecto | Impacto |
|---|---|
| Reducción de tamaño de firmware | ~200-400 KB |
| Reducción de uso de SRAM en stack | ~8-16 KB |
| Eliminación de conflictos MQTT/WiFi con LVGL | Estabilidad mejorada |
| Necesidad de rediseñar `Wifi_OTA.cpp` | Refactorizar a `esp_http_server` puro |

## 5. Cambios en la API de Persistencia (EEPROM → NVS)

El HMI actual usa la librería `EEPROM` de Arduino (263 bytes emulados en flash). La nueva implementación debe usar la API NVS de ESP-IDF:

| Arduino EEPROM | ESP-IDF NVS |
|---|---|
| `EEPROM.begin(size)` | `nvs_flash_init()` + `nvs_open()` |
| `EEPROM.read(addr)` | `nvs_get_u8(handle, key, &value)` |
| `EEPROM.write(addr, val)` | `nvs_set_u8(handle, key, value)` |
| `EEPROM.commit()` | `nvs_commit(handle)` |
| `EEPROM.get(addr, var)` | `nvs_get_blob(handle, key, &var, &len)` |

> Ver guía de migración: `Firmware/docs/eeprom_to_preferences_migration.md`
