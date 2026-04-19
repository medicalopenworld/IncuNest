# Migración EEPROM → Preferences (NVS)

## Contexto

La librería `EEPROM` de Arduino ESP32 almacena internamente un blob binario en NVS bajo el namespace `"eeprom"`, clave `"data"`. La librería `Preferences` usa namespaces independientes. Ambas coexisten en la misma partición NVS sin conflicto.

El objetivo de esta migración es reemplazar el acceso por offsets numéricos (`EEPROM_DESIRED_CONTROL_TEMPERATURE = 42`) por claves con nombre (`"ctrl_temp"`), ganando:

- Legibilidad y mantenibilidad
- Acceso tipado nativo (float, int, bool, string)
- Eliminación de la dependencia de la librería `EEPROM`
- Compatibilidad con herramientas de diagnóstico NVS del ESP-IDF

Los 10 equipos enviados a producción tienen datos en el blob EEPROM. La migración debe preservarlos sin pérdida en el primer arranque tras OTA.

---

## Estrategia: migración one-shot con flag NVS

En el primer boot con el nuevo firmware, el código lee el blob EEPROM, vuelca cada valor a Preferences y marca `migrated=true`. Los arranques siguientes leen directamente de Preferences. El blob EEPROM queda huérfano (no se borra, no interfiere).

```
Boot nuevo firmware
        │
        ▼
cfg.getBool("migrated") == false?
        │
   SÍ  ├─────────────────────────────────────────────────┐
        │                                                 │
        ▼                                                 ▼
EEPROM.begin()                                   Leer directo
Leer todos los valores                           de Preferences
Escribir en Preferences
cfg.putBool("migrated", true)
EEPROM.end()
        │
        ▼
Continuar con Preferences
```

---

## Mapa de claves

Cada offset EEPROM debe tener un nombre de clave Preferences. Namespace recomendado: `"config"`.

| Constante EEPROM | Offset | Tipo | Clave Preferences | Namespace |
|---|---|---|---|---|
| `EEPROM_FIRST_TURN_ON` | — | magic byte | *(no migrar, reemplazar por flag `migrated`)* | — |
| `EEPROM_AUTO_LOCK` | — | uint8 | `"auto_lock"` | `"config"` |
| `EEPROM_LANGUAGE` | — | uint8 | `"language"` | `"config"` |
| `EEPROM_CONTROL_MODE` | — | uint8 | `"ctrl_mode"` | `"config"` |
| `EEPROM_DESIRED_CONTROL_TEMPERATURE` | — | float | `"ctrl_temp"` | `"config"` |
| `EEPROM_DESIRED_CONTROL_HUMIDITY` | — | uint8 | `"ctrl_hum"` | `"config"` |
| `EEPROM_FINE_TUNE_TEMP_SKIN` | — | float | `"ft_skin"` | `"config"` |
| `EEPROM_FINE_TUNE_TEMP_AIR` | — | float | `"ft_air"` | `"config"` |
| `EEPROM_RAW_SKIN_TEMP_LOW_CORRECTION` | — | float | `"cal_skin_low"` | `"config"` |
| `EEPROM_RAW_SKIN_TEMP_RANGE_CORRECTION` | — | float | `"cal_skin_rng"` | `"config"` |
| `EEPROM_REFERENCE_TEMP_RANGE` | — | float | `"cal_ref_rng"` | `"config"` |
| `EEPROM_REFERENCE_TEMP_LOW` | — | float | `"cal_ref_low"` | `"config"` |
| `EEPROM_SERIAL_NUMBER` | — | int | `"serial_num"` | `"config"` |
| `EEPROM_FAN_PWM` | — | int | `"fan_pwm"` | `"config"` |
| `EEPROM_FAN_CTL_PWM` | — | int | `"fan_ctl_pwm"` | `"config"` |
| `EEPROM_HEATER_MAX_AMPS` | — | float | `"heater_amps"` | `"config"` |
| `EEPROM_SKIN_TEMP_MAX` | — | float | `"skin_tmax"` | `"config"` |
| `EEPROM_AIR_TEMP_MAX` | — | float | `"air_tmax"` | `"config"` |
| `EEPROM_GPRS_ACT_PERIOD` | — | int | `"gprs_act_p"` | `"config"` |
| `EEPROM_GPRS_PHOTO_PERIOD` | — | int | `"gprs_photo_p"` | `"config"` |
| `EEPROM_GPRS_STBY_PERIOD` | — | int | `"gprs_stby_p"` | `"config"` |
| `EEPROM_STANDBY_TIME` | — | float | `"stby_time"` | `"config"` |
| `EEPROM_CONTROL_ACTIVE_TIME` | — | float | `"ctrl_act_t"` | `"config"` |
| `EEPROM_HEATER_ACTIVE_TIME` | — | float | `"heat_act_t"` | `"config"` |
| `EEPROM_FAN_ACTIVE_TIME` | — | float | `"fan_act_t"` | `"config"` |
| `EEPROM_HUMIDIFIER_ACTIVE_TIME` | — | float | `"hum_act_t"` | `"config"` |
| `EEPROM_PHOTOTHERAPY_ACTIVE_TIME` | — | float | `"photo_act_t"` | `"config"` |
| `EEPROM_WIFI_SSID` | — | string | `"wifi_ssid"` | `"config"` |
| `EEPROM_WIFI_PASSWORD` | — | string | `"wifi_pass"` | `"config"` |
| `EEPROM_CONTROL_ACTIVE` | — | uint8 | `"ctrl_active"` | `"config"` |
| `EEPROM_PHOTOTHERAPY_ACTIVE` | — | uint8 | `"photo_active"` | `"config"` |

> **Nota**: Completar los offsets reales consultando `motherBoard/include/main.h` antes de implementar.

---

## Implementación paso a paso

### Paso 1 — Crear `StorageManager.h` / `StorageManager.cpp`

Centralizar todo el acceso a Preferences en un único módulo. Esto evita que `Preferences p; p.begin(...)` esté repartido por 9 ficheros.

```cpp
// motherBoard/include/StorageManager.h
#pragma once
#include <Preferences.h>
#include "main.h"

void     Storage_Init();          // migración + carga inicial
void     Storage_Save();          // flush completo a NVS
void     Storage_SaveCalibration();
void     Storage_SaveRuntime();   // actuation, phototherapy
```

### Paso 2 — Función de migración en `StorageManager.cpp`

```cpp
#include "StorageManager.h"
#include <EEPROM.h>   // solo durante migración; se elimina después

static void runMigrationIfNeeded(Preferences &cfg) {
  if (cfg.getBool("migrated", false)) return;

  ESP_LOGW("STORAGE", "First boot with new firmware — migrating EEPROM → NVS");
  EEPROM.begin(EEPROM_SIZE);

  cfg.putFloat("ctrl_temp",    EEPROM.readFloat(EEPROM_DESIRED_CONTROL_TEMPERATURE));
  cfg.putUChar("ctrl_mode",    EEPROM.read(EEPROM_CONTROL_MODE));
  cfg.putUChar("ctrl_hum",     EEPROM.read(EEPROM_DESIRED_CONTROL_HUMIDITY));
  cfg.putUChar("language",     EEPROM.read(EEPROM_LANGUAGE));
  cfg.putUChar("auto_lock",    EEPROM.read(EEPROM_AUTO_LOCK));
  cfg.putFloat("ft_skin",      EEPROM.readFloat(EEPROM_FINE_TUNE_TEMP_SKIN));
  cfg.putFloat("ft_air",       EEPROM.readFloat(EEPROM_FINE_TUNE_TEMP_AIR));
  cfg.putFloat("cal_skin_low", EEPROM.readFloat(EEPROM_RAW_SKIN_TEMP_LOW_CORRECTION));
  cfg.putFloat("cal_skin_rng", EEPROM.readFloat(EEPROM_RAW_SKIN_TEMP_RANGE_CORRECTION));
  cfg.putFloat("cal_ref_rng",  EEPROM.readFloat(EEPROM_REFERENCE_TEMP_RANGE));
  cfg.putFloat("cal_ref_low",  EEPROM.readFloat(EEPROM_REFERENCE_TEMP_LOW));
  cfg.putInt  ("serial_num",   EEPROM.readInt(EEPROM_SERIAL_NUMBER));
  cfg.putInt  ("fan_pwm",      EEPROM.readInt(EEPROM_FAN_PWM));
  cfg.putInt  ("fan_ctl_pwm",  EEPROM.readInt(EEPROM_FAN_CTL_PWM));
  cfg.putFloat("heater_amps",  EEPROM.readFloat(EEPROM_HEATER_MAX_AMPS));
  cfg.putFloat("skin_tmax",    EEPROM.readFloat(EEPROM_SKIN_TEMP_MAX));
  cfg.putFloat("air_tmax",     EEPROM.readFloat(EEPROM_AIR_TEMP_MAX));
  cfg.putInt  ("gprs_act_p",   EEPROM.readInt(EEPROM_GPRS_ACT_PERIOD));
  cfg.putInt  ("gprs_photo_p", EEPROM.readInt(EEPROM_GPRS_PHOTO_PERIOD));
  cfg.putInt  ("gprs_stby_p",  EEPROM.readInt(EEPROM_GPRS_STBY_PERIOD));
  cfg.putFloat("stby_time",    EEPROM.readFloat(EEPROM_STANDBY_TIME));
  cfg.putFloat("ctrl_act_t",   EEPROM.readFloat(EEPROM_CONTROL_ACTIVE_TIME));
  cfg.putFloat("heat_act_t",   EEPROM.readFloat(EEPROM_HEATER_ACTIVE_TIME));
  cfg.putFloat("fan_act_t",    EEPROM.readFloat(EEPROM_FAN_ACTIVE_TIME));
  cfg.putFloat("hum_act_t",    EEPROM.readFloat(EEPROM_HUMIDIFIER_ACTIVE_TIME));
  cfg.putFloat("photo_act_t",  EEPROM.readFloat(EEPROM_PHOTOTHERAPY_ACTIVE_TIME));

  String ssid = EEPROM.readString(EEPROM_WIFI_SSID);
  String pass = EEPROM.readString(EEPROM_WIFI_PASSWORD);
  cfg.putString("wifi_ssid", ssid);
  cfg.putString("wifi_pass", pass);

  cfg.putUChar("ctrl_active",  EEPROM.read(EEPROM_CONTROL_ACTIVE));
  cfg.putUChar("photo_active", EEPROM.read(EEPROM_PHOTOTHERAPY_ACTIVE));

  EEPROM.end();
  cfg.putBool("migrated", true);
  ESP_LOGW("STORAGE", "Migration complete");
}
```

### Paso 3 — `Storage_Init()`: migrar y cargar

```cpp
void Storage_Init() {
  Preferences cfg;
  cfg.begin("config", false);
  runMigrationIfNeeded(cfg);

  // Leer en in3
  in3.language                    = cfg.getUChar("language",  defaultLanguage);
  in3.controlMode                 = cfg.getUChar("ctrl_mode", CONTROL_AIR);
  in3.desiredControlTemperature   = cfg.getFloat("ctrl_temp", presetTemp[in3.controlMode]);
  in3.desiredControlHumidity      = cfg.getUChar("ctrl_hum",  presetHumidity);
  autoLock                        = cfg.getUChar("auto_lock", DEFAULT_AUTOLOCK);
  in3.fineTuneSkinTemperature     = cfg.getFloat("ft_skin",   0.0f);
  in3.fineTuneAirTemperature      = cfg.getFloat("ft_air",    0.0f);
  RawTemperatureLow[SKIN_SENSOR]  = cfg.getFloat("cal_skin_low", 0.0f);
  RawTemperatureRange[SKIN_SENSOR]= cfg.getFloat("cal_skin_rng", 0.0f);
  ReferenceTemperatureRange       = cfg.getFloat("cal_ref_rng",  0.0f);
  ReferenceTemperatureLow         = cfg.getFloat("cal_ref_low",  0.0f);
  in3.serialNumber                = cfg.getInt  ("serial_num", 0);
  // ... resto de variables con sus valores por defecto ...

  cfg.end();

  // Validaciones (igual que recapVariables())
  if (!ReferenceTemperatureRange) in3.calibrationError = true;
  if (isnan(in3.fineTuneSkinTemperature)) in3.fineTuneSkinTemperature = 0.0f;
  // ...
}
```

### Paso 4 — `Storage_Save()` y helpers

```cpp
void Storage_Save() {
  Preferences cfg;
  cfg.begin("config", false);
  cfg.putUChar("language",  in3.language);
  cfg.putUChar("ctrl_mode", in3.controlMode);
  cfg.putFloat("ctrl_temp", in3.desiredControlTemperature);
  cfg.putUChar("ctrl_hum",  in3.desiredControlHumidity);
  cfg.putUChar("auto_lock", autoLock);
  // ...
  cfg.end();
}

void Storage_SaveCalibration() {
  Preferences cfg;
  cfg.begin("config", false);
  cfg.putFloat("cal_skin_low", RawTemperatureLow[SKIN_SENSOR]);
  cfg.putFloat("cal_skin_rng", RawTemperatureRange[SKIN_SENSOR]);
  cfg.putFloat("cal_ref_rng",  ReferenceTemperatureRange);
  cfg.putFloat("cal_ref_low",  ReferenceTemperatureLow);
  cfg.putFloat("ft_skin",      in3.fineTuneSkinTemperature);
  cfg.putFloat("ft_air",       in3.fineTuneAirTemperature);
  cfg.end();
}
```

### Paso 5 — Sustituir llamadas en todos los ficheros

Buscar y reemplazar en los ficheros afectados:

| Fichero | Llamadas EEPROM aprox. | Acción |
|---|---|---|
| `motherBoard/src/EEPROM.cpp` | 56 | Reemplazar por `Storage_Init()`, `Storage_Save()`, `Storage_SaveCalibration()`. Eliminar fichero cuando esté completo. |
| `motherBoard/src/CommTask.cpp` | 10 | Las escrituras de runtime (`EEPROM_CONTROL_ACTIVE`, etc.) → `Storage_SaveRuntime()` |
| `motherBoard/src/GPRS.cpp` | 8 | Lecturas de configuración → ya en `in3` tras `Storage_Init()`, eliminar reads directos |
| `motherBoard/src/Wifi_OTA.cpp` | 24 | SSID/pass y flags OTA → `cfg.getString("wifi_ssid")`, etc. |
| `motherBoard/src/updateData.cpp` | 9 | Escrituras de tiempo activo → `Storage_SaveRuntime()` |
| `motherBoard/src/initHardware.cpp` | 4 | Lecturas de calibración → ya en `in3` |
| `motherBoard/src/userInterface.cpp` | 15 | Escrituras de configuración → `Storage_Save()` |
| `motherBoard/src/UI_actuatorsProgress.cpp` | 4 | Lecturas/escrituras tiempo → `Storage_SaveRuntime()` |
| `motherBoard/src/main.cpp` | 5 | `initEEPROM()` → `Storage_Init()` |

### Paso 6 — Eliminar dependencia EEPROM

Una vez todos los ficheros estén migrados y validados:

1. Eliminar `#include <EEPROM.h>` de todos los ficheros (solo mantenerlo dentro de `runMigrationIfNeeded` hasta que se retire el soporte de migración).
2. Eliminar `EEPROM.cpp` y `EEPROM.h` del proyecto.
3. Eliminar `EEPROM_*` defines de `main.h`.
4. En `platformio.ini`, eliminar `EEPROM_SIZE` si ya no se usa.

---

## Consideraciones de tamaño de claves NVS

NVS limita las claves a **15 caracteres**. Todas las claves del mapa anterior cumplen este límite. Verificar antes de añadir nuevas.

## Consideraciones de espacio NVS

Cada entrada NVS ocupa ~70 bytes (overhead de página). Con ~35 claves en `"config"` + las existentes en `"diag"` y `"photo"`, el uso estimado es ~3 KB. La partición NVS por defecto en ESP32 es 24 KB. No hay problema de espacio.

---

## Plan de testing antes de OTA a campo

1. **Test de migración**: Flashear firmware antiguo en un equipo de prueba con datos reales (serialNumber, calibración, SSID/pass). Luego flashear firmware nuevo. Verificar por log que `runMigrationIfNeeded` se ejecuta y que `Storage_Init` carga los valores correctos.

2. **Test de boot limpio**: Borrar NVS completo (`nvs_flash_erase()`), flashear firmware nuevo. Verificar que se cargan los valores por defecto correctamente.

3. **Test de idempotencia**: Reiniciar tras migración. Verificar que `migrated=true` impide re-ejecutar la migración y que los valores persisten.

4. **Test de calibración**: Hacer una calibración de temperatura, reiniciar, verificar que los factores se recuperan.

5. **Test de crash-restore**: Con `CRASH_TEST_MB` activo, verificar que tras crash el estado (temperatura, humedad, fototerapia) se recupera correctamente con los valores de Preferences.

---

## Estimación de esfuerzo

| Fase | Tiempo estimado |
|---|---|
| Completar mapa de offsets y claves | 2 h |
| Implementar `StorageManager.cpp/h` | 3 h |
| Sustituir llamadas en 9 ficheros | 4 h |
| Testing local (emulado + hardware) | 3 h |
| OTA a 1-2 equipos de campo y validación | 2 h |
| **Total** | **~14 h** |

---

## Rollback

Si la migración falla en campo (valor corrupto tras OTA):

1. El firmware antiguo sigue siendo compatible con el blob EEPROM — basta con hacer OTA de rollback.
2. Para recuperar desde firmware nuevo: añadir un RPC `"resetConfig"` que haga `cfg.clear()` + reinicio, forzando recarga de defaults.
