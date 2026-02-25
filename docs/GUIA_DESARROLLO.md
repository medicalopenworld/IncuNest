# Guía de Desarrollo — IncuNest

> **Versión documento:** 2026-02-25

---

## 1. Estructura del Repositorio

```
IncuNest/
├── Firmware/
│   ├── Display_HMI/          ← Firmware pantalla ESP32-S3
│   │   ├── src/
│   │   │   ├── main.cpp
│   │   │   ├── UITask.cpp            (lógica UI LVGL, ~4000 líneas)
│   │   │   ├── ElementsCreation.cpp  (widgets LVGL, ~3000 líneas)
│   │   │   ├── CommTask.cpp          (comunicación serie)
│   │   │   ├── AudioManager.cpp      (audio I2S MP3)
│   │   │   ├── EEPROM.cpp            (persistencia)
│   │   │   ├── Wifi_OTA.cpp          (WiFi + OTA)
│   │   │   └── ui_img_*.c            (imágenes convertidas)
│   │   ├── include/
│   │   │   ├── main.h                (constantes globales, tipos)
│   │   │   ├── display_comms.h       (protocolo TLV binario)
│   │   │   ├── EEPROM_defines.h      (mapa EEPROM)
│   │   │   ├── AudioManager.h
│   │   │   ├── CommTask.h
│   │   │   └── lv_conf.h             (configuración LVGL)
│   │   ├── platformio.ini
│   │   └── IncuNest_display_v1_audio.csv   (tabla de particiones)
│   │
│   ├── motherBoard/           ← Firmware placa madre ESP32-S3/FireBeetle32
│   │   ├── src/
│   │   │   ├── main.cpp              (setup + 9 tareas FreeRTOS)
│   │   │   ├── PID.cpp               (3 controladores PID)
│   │   │   ├── security.cpp          (9 tipos de alarma)
│   │   │   ├── sensors.cpp
│   │   │   ├── initHardware.cpp
│   │   │   ├── CommTask.cpp          (comunicación con Display, v15+)
│   │   │   ├── GPRS.cpp              (conectividad GSM 2G)
│   │   │   ├── Wifi_OTA.cpp
│   │   │   ├── updateData.cpp
│   │   │   ├── calibrateSensors.cpp
│   │   │   └── ...
│   │   ├── include/
│   │   └── platformio.ini            (2 entornos: v14 + v15)
│   │
│   └── old/                   ← Código legacy (no usar)
│
├── Hardware/
│   ├── Electronics/           ← Esquemas PCB (KiCad)
│   └── Mechanical/            ← Modelos 3D (STL/STEP)
│
├── docs/                      ← Documentación técnica (este directorio)
│   ├── ARQUITECTURA_FIRMWARE.md
│   ├── ANALISIS_COMUNICACION.md
│   ├── SISTEMA_ALARMAS.md
│   ├── INVENTARIO.md
│   ├── TECNOLOGIA.md
│   └── GUIA_DESARROLLO.md     ← Este archivo
│
└── README.md
```

---

## 2. Compilar y Flashear

### Display_HMI

```bash
# Compilar
pio run -e main

# Flashear firmware
pio run -e main -t upload

# Subir sistema de archivos SPIFFS (audio MP3)
pio run -e main -t uploadfs

# Monitor serie
pio device monitor
```

> ⚠️ **Importante:** El archivo `/sapphire.mp3` debe subirse con `uploadfs` antes de que el audio funcione.

### motherBoard — v15 (ESP32-S3)

```bash
# Compilar y subir
pio run -e in3ator_V15 -t upload ; pio device monitor

# Solo compilar
pio run -e in3ator_V15
```

### motherBoard — v14 (FireBeetle32)

```bash
pio run -e in3ator_UP_TO_V14 -t upload
```

---

## 3. Parámetros de Control Configurables

### Temperaturas (display_HMI `main.h`)

| Constante | Valor | Descripción |
|---|---|---|
| `AIR_TEMP_MIN` | 30.0 ºC | Mínimo temperatura aire seleccionable |
| `AIR_TEMP_MAX` | 37.0 ºC | Máximo temperatura aire seleccionable |
| `SKIN_TEMP_MIN` | 35.0 ºC | Mínimo temperatura piel seleccionable |
| `SKIN_TEMP_MAX` | 37.5 ºC | Máximo temperatura piel seleccionable |
| `TEMP_INCREMENT` | 0.2 ºC | Paso de incremento en la UI |
| `TEMP_ALARM_THRESHOLD` | 37.0 ºC | Umbral de alarma de temperatura |

### Humedad

| Constante | Valor | Descripción |
|---|---|---|
| `HUM_MIN` | 20 % | Mínimo humedad seleccionable |
| `HUM_MAX` | 90 % | Máximo humedad seleccionable |
| `HUM_STEP` | 5 % | Paso de incremento en la UI |
| `HUM_ALARM_THRESHOLD` | 60 % | Umbral de alarma de humedad |

---

## 4. Añadir una Nueva Pantalla LVGL

1. En `ElementsCreation.cpp`: crear función `create_ScreenNueva()` con widgets LVGL
2. En `ElementsCreation.h`: declarar la función
3. En `UITask.cpp`: añadir llamada a `lv_scr_load()` con la nueva pantalla en la transición correspondiente
4. Definir callbacks de eventos LVGL en `UITask.cpp` (patrón `EventNueva_cb`)

---

## 5. Añadir un Nuevo Parámetro EEPROM

1. Añadir `#define EEPROM_NUEVO_PARAM <dirección>` en `EEPROM_defines.h`
2. Añadir lectura en `initEEPROM()` y escritura en `loadDefaultValues()` en `EEPROM.cpp`
3. Guardar desde UI en eventos de LVGL con `EEPROM.write(EEPROM_NUEVO_PARAM, valor)`

> ⚠️ El `EEPROM_SIZE` es 256 bytes. La dirección 255 es el tope. Actualmente usada hasta la 251.

---

## 6. Añadir una Nueva Alarma

### En motherBoard

1. Añadir el ID en el enum `ALARMS_ID` en `main.h` (antes de `NUM_ALARMS_ID`)
2. Añadir el texto localizado en `alarmIDtoString()` en `security.cpp`
3. Añadir el texto detallado en `sendAlarmUSB()` en `security.cpp`
4. Añadir la lógica de detección en `securityCheck()` o en la tarea correspondiente

### En Display_HMI

El sistema es dinámico: el Display recibe las alarmas por protocolo serie y las muestra automáticamente si el ID está en el rango `MAX_ALARMS` (10).

---

## 7. Protocolo de Comunicación entre placas

Ver [`ANALISIS_COMUNICACION.md`](ANALISIS_COMUNICACION.md) para la especificación completa.

**Resumen de flujo para enviar un comando desde UI:**
```cpp
// En UITask.cpp, al cambiar un switch:
hmi_msg.actuation = ACTUATION_TEMPERATURE;
hmi_msg.desiredAirTemperature = airTempValue;
hmi_msg.shouldSendData = true;  // CommTask lo detecta y envía
```

---

## 8. Control de Volumen de Audio

| Acción | Código |
|---|---|
| Subir volumen (UI) | `AudioManager::getInstance().setVolume(vol + 1)` |
| Bajar volumen (UI) | `AudioManager::getInstance().setVolume(vol - 1)` |
| Guardar en EEPROM | `EEPROM.write(EEPROM_AUDIO_VOLUME, vol)` + commit diferido |
| Rango válido | 0 – 21 |
| Valor por defecto | 15 (si EEPROM[251] == 0 o > 21) |

---

## 9. Notas de Compatibilidad entre versiones HW

| Característica | ≤ v14 | v15+ |
|---|---|---|
| Pantalla HMI | Integrada ILI9341 | Separada (CrowPanel 7.0) |
| Comunicación con display | N/A | USB-CDC / UART |
| Tarea UI en motherBoard | ✅ Activa | ❌ Desactivada (`#if HW_NUM < 15`) |
| Comunicación activada | ❌ | ✅ (`CONFIG_IDF_TARGET_ESP32S3`) |
| `cdc_acm_host.c` compilado | ❌ (excluido) | ✅ |

---

## 10. Conocidos Issues / Limitaciones

| Issue | Estado | Solución aplicada |
|---|---|---|
| Parpadeo de pantalla durante audio | ✅ Resuelto | AudioTask con ciclos cortos (3 loops + 3ms) en Core 0 |
| Volumen alto al iniciar reproducción | ✅ Resuelto | `playTone()` usa `_volume` en lugar de valor hardcodeado 21 |
| Bloqueo por escritura EEPROM durante audio | ✅ Resuelto | Escritura diferida de EEPROM con commit asíncrono |
| Desincronización estado al reconectar HMI | ✅ Resuelto | `HMI,REQ,STATE` con reintentos cada 500 ms + cola de alarmas pendientes |
| Guru Meditation Error (LoadProhibited) | ✅ Resuelto | Verificación de punteros LVGL nulos en callbacks |
| EMI en bus USB entre placas | ⚠️ Mitigado | Timeout buffer RX 50 ms + mutex log |
