# Porte de motherBoard y Display_HMI a ESP-IDF nativo

Rama: `refactor/idf-native-port` · Worktree: `Firmware/.worktrees/idf-native-port`
ESP-IDF de referencia: **v6.0.1** (`IDF_PATH=C:\esp\v6.0.1\esp-idf`), la misma
que ya usa la SensorBoard.

Objetivo: quitar el framework Arduino de las dos placas y construir con
`idf.py`/CMake, dejando las tres placas del equipo sobre la misma toolchain.

> **Recordatorio de contexto.** Esto es firmware de un equipo medico con
> unidades en campo (Togo) y una tirada de 200 en curso. El criterio de todo el
> porte es **preservar comportamiento**: donde algo se cambia a mejor, va en un
> commit aparte y etiquetado. Lo que no se puede verificar sin banco, se marca
> como pendiente de banco y no se da por hecho.

---

## 1. Estado actual (medido, no estimado)

| | Fuentes | Pendientes | Compila |
|---|---|---|---|
| `components/incunest_platform` | 5 | 0 | ✅ |
| `shared/` | 6 | 0 | ✅ |
| `components/thingsboard` (parcheado) | 44 | 0 | ✅ |
| **Display_HMI** | ~100 | **16** | configura y enlaza dependencias |
| **motherBoard** | ~85 | **37** | configura y enlaza dependencias |

Gate barato para la capa base, sin construir una placa entera:

```
idf.py -C Firmware/tools/platform_smoke build
```

---

## 2. Decisiones tomadas y por que

### 2.1 `millis()` y `micros()` conservan el nombre

390 llamadas entre las dos placas, casi todas dentro del patron de resta sin
signo `(uint32_t)(millis() - t0) >= PERIODO`, y muchas en caminos de alarma y
control. La implementacion de `components/incunest_platform/plat_time.h` ya es
ESP-IDF puro (esp_timer): no queda una linea de Arduino. Renombrarlas solo
habria anadido 390 ediciones a mano sin ganar nada tecnico.

Se preserva a proposito el desbordamiento: `millis()` da la vuelta a los 49,7
dias y `micros()` a los 71,6 minutos, igual que antes. Para plazos largos hay
`millis64()`.

### 2.2 El PWM conserva el mapeo canal→timer de Arduino

`timer = (canal / 2) % 4`, la regla interna de `esp32-hal-ledc.c`. No es un
detalle: `board.h:167` documenta que el ventilador se movio al canal 7
**justo para que su frecuencia no pisara los 400 Hz del calefactor**, y esa
colocacion solo tiene sentido con ese mapeo. Cambiarlo por un reparto "mejor"
de timers alteraria en silencio la frecuencia real de un actuador termico.

### 2.3 `NvsPrefs` reproduce byte a byte el formato de `Preferences`

Es el punto de mayor riesgo de datos de todo el porte. Las unidades en campo
tienen la NVS escrita por la `Preferences` de Arduino, y ahi viven los
perfiles de bebe, la configuracion de control y las claves. El mapeo se
verifico leyendo el `Preferences.cpp` real de arduino-esp32.

**La trampa:** `putFloat`/`putDouble` guardaban un **BLOB**, no un tipo
numerico de NVS. Son 52 escrituras y 18 lecturas de float en el firmware. Si
alguien las "mejora" a `nvs_set_u32` o similar, un OTA deja a las unidades con
los valores por defecto y sin perfiles de bebe. Tambien se conserva el
`nvs_commit()` tras **cada** escritura, porque cambia el desgaste de flash y el
comportamiento ante un corte de corriente.

Los nombres de metodo se conservan (`putFloat`, `getUChar`...), asi que
convertir un fichero es cambiar el tipo `Preferences` → `NvsPrefs` y poco mas.

### 2.4 Estandar de C++ fijado al que ya se usaba

ESP-IDF 6 compila por defecto en **gnu++26**, y el codigo venia de gnu++2b
(HMI) y gnu++17 (motherBoard). C++26 convierte en **error** las operaciones de
bits entre enums de distinto tipo, patron que LVGL usa por todas partes.
Modernizar el estandar sobre 62k lineas es un proyecto propio, no algo que
deba entrar de gorra en un cambio de toolchain.

### 2.5 LVGL usa el `lv_conf.h` del proyecto

El componente de LVGL para ESP-IDF, si se le deja, se configura por menuconfig
y **se salta** `include/config/lv_conf.h`. Se apaga ese camino con
`LV_KCONFIG_IGNORE` y se apunta con `LV_CONF_PATH`, para que el fichero de
siempre siga siendo la unica fuente de verdad.

### 2.6 El SDK de ThingsBoard: copia local con dos parches

**v0.13.0 no compila contra ESP-IDF 6** (mbedtls 4) y **no hay ninguna version
publicada que lo haga**: v0.15.0 esta rota de otra forma y el proyecto no
publica desde diciembre de 2024. Detalle completo y delta exacto en
`components/thingsboard/PATCHES.md`. La alternativa era quedarse en ESP-IDF
5.5 (mbedtls 3), descartada porque fragmentaria de nuevo las toolchains.

---

## 3. Trabajo pendiente, en orden

El orden importa: cada bloque desbloquea al siguiente y es verificable por su
cuenta con `idf.py build`.

### Fase A — Display_HMI (16 ficheros)

| # | Trabajo | Ficheros | Notas |
|---|---|---|---|
| A1 | `Preferences` → `NvsPrefs` | `EEPROM.cpp`, `maintenance.cpp`, `training_progress.cpp`, `main.cpp` | Mecanico. Solo cambia el tipo. |
| A2 | `Wire` → `I2cBus` y driver propio de **PCA9557** | `UITask.cpp`, `FactoryTest.cpp` | El PCA9557 es un expansor trivial (un registro de salida). |
| A3 | Driver propio de **GT911** (tactil) | `lib/TAMC_GT911_Fixed` | 208 lineas, I2C puro. Reescribir sobre `I2cBus`. |
| A4 | **Enlace con la placa**: `Stream*`/`COMM_SERIAL` → driver UART de IDF | `display_comms.h`, `CommTask.cpp`, `main.cpp` | **El mas delicado: por aqui viajan las alarmas.** Ver la nota en `display_comms.h`. Conservar `COMM_RX_RING_BYTES = 1024`. En IDF el tamano del anillo es argumento de `uart_driver_install()`, lo que ademas resuelve de raiz el fallo de las lineas perdidas. |
| A5 | Red y nube: `WiFi`/`WebServer`/`Update`/`ESPmDNS` → `esp_wifi` + `esp_http_server` + `esp_https_ota` + `espressif/mdns`; `Arduino_MQTT_Client` → `Espressif_MQTT_Client` | `Wifi_OTA.cpp` (623 l.), `support_report.cpp` | Bloque grande pero acotado a dos ficheros. |
| A6 | Restos de `Arduino.h` | `hmi_audio_module.cpp`, `training_mode.cpp`, `AlarmCenter.cpp`, `BabyHistory.cpp`, `ElementsCreation.cpp` | Solo necesitan `millis()`, que ya existe. |

`AudioManager.cpp` sigue **fuera del build**, igual que en `platformio.ini`. No
se reactiva dentro del porte.

### Fase B — motherBoard (37 ficheros)

| # | Trabajo | Notas |
|---|---|---|
| B1 | `Preferences` → `NvsPrefs` (`EEPROM.cpp`, `baby_profile_store.cpp`, `main.cpp`) | 117 referencias, mecanico. |
| B2 | `String` → `std::string` | **290 usos.** El bloque mas tedioso, pero de riesgo bajo y revisable. |
| B3 | Borrar `TFT_eSPI` | Codigo muerto: `main.cpp:88` declara `TFT_eSPI tft`, tres ficheros la declaran `extern` y **nadie la usa**. La placa ya no tiene pantalla propia. |
| B4 | Drivers de sensor sobre `I2cBus`: SHTC3, STS3x, SHT4x, INA3221, TCA9555 | Los envoltorios `drv_*.cpp` ya existen y son finos; el trabajo es sustituir las libs de Arduino que hay detras. Sensores simples de comando + lectura de palabra. |
| B5 | `BQ25730` y `AFE4490` (SPO2) | `BQ25730` ya es nuestro (17 KB): solo cambia el transporte I2C. El AFE4490 va por SPI (`incunest_afe4490`, libreria externa pineada) y hay que reescribirlo sobre `spi_master`. |
| B6 | `PID_v1` y `RotaryEncoder` | Portables casi tal cual. El encoder puede ir a PCNT. |
| B7 | `LittleFS` → `joltwallet/littlefs` | **Montar la particion existente etiquetada `spiffs`, sin reformatear.** Ahi estan los perfiles de bebe y el archivo de pesos. |
| B8 | Red y nube (`Wifi_OTA.cpp`, 1679 l.) | Igual que A5. |
| B9 | **`TinyGSM` → `esp_modem`** (`GPRS.cpp`, 1339 l.) | **El bloque de mas riesgo de todo el porte.** No es una sustitucion: TinyGSM es AT sobre `Stream` y `esp_modem` es PPP/lwIP, otra arquitectura. De aqui cuelgan las SIM Onomondo y la tirada de 200. **Debe ir el ultimo y con banco dedicado.** |

### Fase C — alrededores

| # | Trabajo | Notas |
|---|---|---|
| C1 | **Tests de host** (26 suites Unity) | Hoy corren con `pio test -e native`. Hay que rehospedarlos (CMake+Unity, o el target `linux` de IDF). **No dar el porte por terminado sin esto**: es la unica cobertura automatizada que existe. |
| C2 | `flasher_tool` | Lee `.pio/build/<env>/`; pasa a leer `build/`. Los nombres de los binarios cambian (`firmware.bin` → `<proyecto>.bin`). |
| C3 | `.github/workflows/release.yml` | `pio run` → `idf.py build`, y las rutas de los artefactos. |
| C4 | `.claude/` (11 ficheros mencionan `pio`) | Reglas, hooks y skills. El hook de Stop debe pasar a `idf.py build`. |
| C5 | Documentacion | `README`, `docs/partitions.md`, `CLAUDE.md`. |

---

## 4. Riesgos abiertos

1. **`esp_modem` (B9).** El unico bloque que cambia arquitectura y no solo API.
   Con 200 unidades en curso, merece rama y banco propios.
2. **Presupuesto de SRAM del HMI.** Los dos bounce buffers de 38,4 KB
   contiguos tenian ~87 KB de margen medido. Otra toolchain mueve ese numero.
   **Hay que volver a medir el mayor bloque contiguo (no el libre total)**
   antes de dar el HMI por bueno.
3. **Tamano de flash de la motherBoard sin confirmar.** `platformio.ini` no
   declaraba `flash_size` y heredaba 8 MB del manifest. Si el modulo real es
   N16R8, sobra media flash. Comprobar con `esptool.py flash_id`.
4. **Compatibilidad de NVS.** Verificar en una unidad real que, tras el OTA,
   los perfiles de bebe y la configuracion siguen ahi. Es lo primero que hay
   que probar en banco, antes que ninguna funcionalidad.
5. **Copia local del SDK de ThingsBoard.** Deuda asumida a conciencia sobre
   una dependencia sin mantenimiento. Revisar si upstream revive.

## 5. Lo que no se ha tocado a proposito

- `AudioManager.cpp`, fuera del build desde la migracion a Arduino 3.x.
- `src/hal/hal_hmi.*`: **no lo llama nadie** (comprobado con grep). Se porto en
  vez de borrarlo, porque quitar codigo es una decision de producto. Candidato
  a borrar en un commit aparte, junto con `TFT_eSPI` (B3).
- Los `usb_host_*` de la motherBoard, que ya eran ESP-IDF nativo. Candidatos a
  sustituirse por los componentes gestionados `espressif/usb_host_cdc_acm` y
  `espressif/usb_host_ch34x_vcp`, pero eso es un cambio aparte.
