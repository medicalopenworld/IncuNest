## Why

Afecta solo a **Display_HMI** (no toca `shared/` ni la motherBoard: nada
viaja por el protocolo).

Hoy la retroiluminación del HMI es un interruptor: el arranque escribe un byte
fijo (`DISPLAY_BL_ON_VALUE`, 10) al STC8H1K28 (I2C `0x30`) y el apagado por
`pwroff` escribe otro (`DISPLAY_BL_OFF_VALUE`, 245). La documentación del
controlador (v1.3, citada en `UITask.cpp`) dice que el byte **ya es un nivel**
(0 = máximo, 245 = apagado), pero el firmware nunca lo usa así. Una incubadora
neonatal vive en una sala donde de noche se baja la luz: una pantalla de 7"
a pleno brillo, encendida toda la noche junto al bebé, molesta al bebé y al
personal, y no hay forma de bajarla.

Este cambio añade:

- un **control de brillo** (slider 20-100 %) en Ajustes > Modos;
- un **modo noche** (switch, desactivado por defecto) que atenúa la pantalla
  tras un tiempo bloqueada sin toques;
- la garantía de que **cualquier toque o cualquier alarma devuelve el brillo**
  (alarma: 100 %, mientras dure). Nunca se deja la pantalla a oscuras con una
  alarma activa.

## What Changes

- **Driver `drivers/backlight`**: encapsula la escritura del byte al STC8H1K28
  (conversión porcentaje a byte, reintentos, comprobación del resultado) y
  sustituye las escrituras `Wire` en línea del arranque y del apagado.
  Expone `Backlight_SetPercent()` y `Backlight_Off()`. Nunca emite un byte
  >= 246 como brillo (son los opcodes del zumbador y del altavoz).
- **Política pura `modules/brightness/brightness_policy`**: dada la entrada
  (nivel de usuario, modo noche, bloqueada, segundos sin tocar, alarma
  activa) devuelve el porcentaje objetivo. Sin Arduino ni LVGL: se prueba en
  `[env:native]` de Display_HMI. También contiene la conversión
  porcentaje a byte, con el modo degradado.
- **Ajustes**: dos filas nuevas en el panel Modos (slider de brillo y switch
  de modo noche), con cadenas en español, inglés, francés y portugués.
- **Persistencia** en NVS (`hmi_cfg`): `bl_pct` y `bl_night`, escritas en el
  bloque diferido fuera de `LVGL_Lock`.
- **Servicio de brillo en `UI_Task`**: cada pasada calcula el objetivo dentro
  del bloqueo de LVGL (lee pantalla activa, inactividad, alarmas) y escribe
  por I2C **después** de soltarlo (ARQ-LOCK-001), solo cuando cambia o cuando
  la última escritura falló.
- **Degradación segura**: si en el banco el STC8H1K28 no acepta niveles
  intermedios, `DISPLAY_BL_LEVELS_SUPPORTED=0` hace que todo nivel > 0 escriba
  el byte de encendido: el control de brillo queda inerte pero la pantalla
  nunca se apaga ni se queda en un nivel desconocido.

No cambia `FWversion`.

## Capabilities

### New Capabilities

- `hmi-brightness`: brillo regulable, modo noche y su política de
  restauración por toque y alarma.

### Modified Capabilities

Ninguna.

## Impact

- Código: `Display_HMI/src/drivers/backlight.{h,cpp}` (nuevo),
  `src/modules/brightness/brightness_policy.{h,cpp}` (nuevo), `UITask.cpp`,
  `ElementsCreation.cpp`, `EEPROM.cpp`, `EEPROM_defines.h`,
  `display_config.h`, `i18n_strings.def`, `platformio.ini` (filtro de
  `[env:native]`), `test/test_brightness_policy/` (nuevo).
- Hardware: **riesgo abierto, no probado**. Que el STC8H1K28 acepte niveles
  intermedios es una suposición basada en el comentario "doc v1.3". La
  verificación en banco es obligatoria antes de dar la función por buena.
- Known issues: no toca alarmas, serie ni carga de firmware. Lee el estado de
  alarmas (solo lectura, ya expuesto por `UI_IsAnyAlarmActive()`).
