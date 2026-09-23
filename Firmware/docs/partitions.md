# Particiones de flash

Reparto de flash de las tres placas, revisado el **2026-09-06**. Antes de esa
fecha no había ningún sitio donde estuviera todo junto y las tres tablas habían
derivado por separado.

Fuentes de verdad (esto es un resumen, no un duplicado):

| Placa | Tabla | Declarada en |
|---|---|---|
| Motherboard | `motherBoard/partitions/ESP32S3_8MB.csv` | `motherBoard/platformio.ini` |
| Display HMI | `Display_HMI/partitions/hmi_16mb_ota.csv` | `Display_HMI/platformio.ini` |
| SensorBoard | `SensorBoard_v2/partitions.csv` | `SensorBoard_v2/sdkconfig.defaults` |

---

## Motherboard — ESP32-S3, 8 MB

| Partición | Offset | Tamaño | Uso |
|---|---|---|---|
| `nvs` | `0x9000` | 20 KB | Preferences, número de serie |
| `otadata` | `0xE000` | 8 KB | |
| `app0` | `0x10000` | 2,625 MB | 1,48 MB — **54 %** |
| `app1` | `0x2B0000` | 2,625 MB | slot OTA |
| `spiffs` | `0x550000` | 2,625 MB | **LittleFS**: perfiles de bebé, histórico de pesos |
| `coredump` | `0x7F0000` | 64 KB | |

Suma exacta: 8 MB.

La partición de datos se llama `spiffs` pero monta LittleFS: es la etiqueta que
busca `LittleFS.begin()` de Arduino-ESP32 por defecto. Renombrarla obligaría a
pasar la etiqueta explícita en cada `begin()` y a reformatear, perdiendo los
datos de paciente. Se deja el nombre histórico a propósito.

**Pendiente de verificar:** `platformio.ini` no declara `board_build.flash_size`,
así que hereda los 8 MB del manifest de `esp32-s3-devkitc-1`. Si el módulo real
es un WROOM-1 N16R8 como las otras dos placas, sobra media flash sin usar.
Comprobar con `esptool.py --port COMx flash_id`; si son 16 MB, está lista
`motherBoard/partitions/ESP32S3_16MB.csv` (4 MB por slot y 7,875 MB de
filesystem). Declarar 16 MB sobre un módulo de 8 MB impide el arranque, así que
hay que verificarlo antes, no asumirlo.

## Display HMI — ESP32-S3, 16 MB

| Partición | Offset | Tamaño | Uso |
|---|---|---|---|
| `nvs` | `0x9000` | 20 KB | |
| `otadata` | `0xE000` | 8 KB | |
| `app0` | `0x10000` | 5 MB | 2,53 MB — **48 %** |
| `app1` | `0x510000` | 5 MB | slot OTA |
| `spiffs` | `0xA10000` | 5,875 MB | `heartbeat.mp3` (543 KB) |
| `coredump` | `0xFF0000` | 64 KB | |

Suma exacta: 16 MB.

Antes eran 3 MB por slot y 8 MB de SPIFFS, y estaba al revés de lo que consume
el firmware. Los PNG de `Display_HMI/data/` no se leían del filesystem: son las
fuentes que se convierten a arrays C en `src/ui/assets/` y se compilan dentro de
la app. Con `app0` al 78 %, cada icono o fuente nueva competía por 675 KB
mientras 8 MB de SPIFFS y 1,9 MB sin asignar estaban parados. Los originales
viven ahora en `Display_HMI/assets_src/` (ver su README) y `data/` contiene solo
lo que se lee en runtime.

**El presupuesto de assets es `app0`, no SPIFFS.** Los arrays LVGL son
`LV_IMG_CF_TRUE_COLOR` a 16 bpp: 2 bytes por píxel sin comprimir, así que lo que
manda son las dimensiones y no el peso del PNG. Ver
`docs/alarm_popup_image_prompts.md` §5.1 para el cálculo de las 20 imágenes de
alarma pendientes.

## SensorBoard — ESP32-S3-WROOM-1-N16R8, 16 MB

| Partición | Offset | Tamaño | Uso |
|---|---|---|---|
| `nvs` | `0x9000` | 16 KB | sin usar todavía |
| `otadata` | `0xD000` | 8 KB | |
| `phy_init` | `0xF000` | 4 KB | sin usar (radio apagada) |
| `ota_0` | `0x10000` | 2 MB | 374 KB — **18 %** |
| `ota_1` | `0x210000` | 2 MB | slot OTA |
| `coredump` | `0x410000` | 64 KB | |
| `storage` | `0x420000` | 11,875 MB | reservado para los JPEG de la fase 5 |

Suma exacta: 16 MB. Ojo: `otadata` está en `0xD000`, no en `0xE000` como en las
dos placas Arduino — es el reparto estándar de ESP-IDF.

La tabla anterior declaraba `factory` + `ota_0` + `ota_1` de 3 MB **sin
`otadata`**, así que los 6 MB de slots OTA no servían para nada: sin esa
partición el bootloader arranca siempre `factory`.

---

## Al tocar una tabla

1. **Los offsets están duplicados a mano en `flasher_tool/flasher/flasher.py`**
   (`_BOARD_FILES`). Si se mueve una partición en el CSV y no allí, el flasheo
   no falla: escribe en el sitio equivocado. Hay tests que fijan los offsets.
2. **La tabla de particiones no viaja en un OTA.** Una unidad desplegada se
   queda con la suya para siempre; cambiar tamaños obliga a reflashear por USB.
   La revisión de septiembre de 2026 se hizo aprovechando que aún no había
   ninguna unidad en campo.
3. Validar el CSV generando el binario, no a ojo:
   `python <framework>/tools/gen_esp32part.py --flash-size 16MB tabla.csv /tmp/p.bin`
   — falla si algo se solapa o se sale de la flash.
4. Regenerar los binarios de `flasher_tool/data/firmware/*/` (bootloader,
   `partitions.bin`, `firmware.bin`, y la imagen SPIFFS / `ota_data_initial.bin`
   donde apliquen).

## Core dump

Las tres placas tienen partición de `coredump` y el volcado activado. Antes solo
la tenía el HMI.

- **Motherboard y HMI**: el sdkconfig de Arduino-ESP32 trae
  `CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH=y` de serie, así que basta con que exista
  la partición. En la motherboard no existía: cada panic de la placa que gobierna
  calefactor y humidificador fallaba al volcar y se perdía el post-mortem.
- **SensorBoard**: en ESP-IDF viene desactivado; se activa en
  `sdkconfig.defaults`. Además `espcoredump` tiene que estar en
  `set(COMPONENTS ...)` del `CMakeLists.txt`: con el build mínimo quedaba fuera,
  y entonces sus símbolos Kconfig no existen y confgen descarta las opciones en
  silencio.

Leer un volcado: `idf.py -p COMx coredump-info` (SensorBoard), o
`espcoredump.py` contra el ELF correspondiente en las placas Arduino.
