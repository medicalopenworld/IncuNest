# Fuentes de imagen del HMI

PNG/JPG originales de los que salen los arrays C de `src/ui/assets/ui_img_*.c`.
**No se flashean**: la conversion es en tiempo de desarrollo y lo que acaba en
la placa es el array compilado dentro de la app.

## Por que no estan en `data/`

Hasta septiembre de 2026 vivian en `Display_HMI/data/`, que para PlatformIO no
es una carpeta cualquiera: es el contenido de la **imagen SPIFFS**
(`pio run -t buildfs` / `-t uploadfs`). Tenerlas ahi tenia tres consecuencias,
las tres invisibles porque nada fallaba en el build normal:

1. La particion SPIFFS se dimensiono a 8 MB para "que cupiera `data/`", cuando
   el firmware solo lee de SPIFFS `/heartbeat.mp3` (543 KB). Esos 8 MB salieron
   de los 16 MB totales a costa de los slots de app, que se quedaron en 3 MB
   con la app al 78 %.
2. `pio run -t buildfs` llevaba tiempo roto y nadie lo notaba: mkspiffs peta con
   `SPIFFS_write error(-10010)` en el tercer fichero
   (`Baby_phototherapy_eyes_covering.png`, 2,2 MB) tanto con 8 MB como con
   5,875 MB — SPIFFS no lleva bien ficheros de ese tamano.
3. `flasher_tool` nunca escribio una imagen SPIFFS, asi que en una unidad de
   fabrica esas imagenes no estaban de todas formas.

Ahora `data/` contiene solo lo que de verdad se lee en runtime y la imagen
SPIFFS se construye y se flashea de fabrica.

## Al anadir un asset

1. Deja el original aqui.
2. Conviertelo al array C en `src/ui/assets/` (LVGL image converter, o
   `tools/gen_alarm_images.py` para las imagenes de alarma).
3. Declara el simbolo en `include/ui/ElementsCreation.h`.

Cuidado con el presupuesto: **cada asset nuevo come de `app0`, no de SPIFFS**.
Hoy la app usa 2,53 MB de los 5 MB del slot.

## Si algun dia se leen del filesystem

Haria falta habilitar un driver de FS en LVGL (`LV_USE_FS_*` en
`include/config/lv_conf.h`, hoy todos a 0) y registrar un `lv_fs_drv_t` sobre
SPIFFS. Ademas habria que optimizar los PNG antes: a peso original no caben en
SPIFFS ni sobrando particion.
