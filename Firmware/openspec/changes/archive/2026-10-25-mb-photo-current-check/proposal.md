## Why

Placas afectadas: **motherBoard** (el estado que emite y la detección) y **shared/** (nuevo `AlarmId`, prioridad y textos); **Display_HMI** no cambia de código pero **debe recompilar**, porque consume `shared/` y el id nuevo cambia `ALARM_COUNT`. Toca el área de alarmas y el enlace serie con el display, así que aplican los known issues nº 1 («Phantom Alarms»: la condición nueva nace con antirrebote e histéresis) y nº 2 (no se añade ninguna línea ni cadencia nueva al enlace).

Dos huecos del mismo origen, descritos en `Firmware/docs/known_issues.md` §20:

1. **La placa miente sobre la lámpara.** `send_state_to_hmi()` (`tasks/CommTask.cpp`) emite en el campo 8 de `CTRL,STATE` `g_last_cmd.phototherapyMode`, es decir, lo último que *pidió el display*, y no `in3.phototherapy`, lo que la placa *aplica*. Es el mismo patrón que ya se corrigió para el audio (`audioSilenced`). Consecuencia cierta: durante una sesión de fototerapia de depuración (`PHOTO,1` por la consola) la pantalla dice OFF con la lámpara ON.
2. **Nadie comprueba que la lámpara responde.** La corriente de la lámpara ya se mide por INA3221 (`in3.phototherapy_current`) y el lazo de intensidad de `sensors_module.cpp` la regula hacia 0,45 A, pero si la lámpara no consume nada (LED abierto, conector, driver) el equipo sigue anunciando una sesión de fototerapia que no está iluminando a nadie. Es un fallo silencioso en un equipo médico.

## What Changes

- **Campo 8 de `CTRL,STATE` = estado real.** Pasa a emitir `in3.phototherapy`. El resto de ecos del mismo mensaje (actuación, modo de control, consignas, modo piel) **no se tocan** — ver `design.md` D2: no tienen un valor «real» equivalente de una línea, o su semántica es dudosa.
- **Nueva condición de alarma `ALARM_PHOTO_LAMP_FAULT`** (id 20, final del enum), prioridad **BAJA**, no latching, **no corta el calefactor**. Se declara cuando, con la fototerapia ordenada ON, pasado `PHOTO_SETTLE_MS` y con el sensor INA presente, `in3.phototherapy_current` queda por debajo de `PHOTO_FAULT_MIN_A` (0,05 A) durante `PHOTO_FAULT_CYCLES` (5) ciclos consecutivos del lazo de 1 s. Se retira cuando la corriente supera `PHOTO_FAULT_CLEAR_A` (0,08 A, histéresis) o cuando la fototerapia deja de estar ON.
- **Lógica pura** (contador de ciclos, umbral, histéresis) en `modules/control/photo_fault.{h,cpp}`, C-linkage y sin Arduino, con tests Unity en `[env:native]`. El cableado a `alarm_machine_condition()` queda en `sensors_module.cpp`.
- **Telemetría**: la clave `Phototherapy_active` de ThingsBoard ya publica `in3.phototherapy` (estado real); no cambia. Se añade la clave de la alarma nueva, siguiendo el patrón de las dos del SensorBoard.
- **Docs**: `PROTOCOL.md` (campo 8), `known_issues.md` §20 resuelto, `alarms.md` §1/§2/§9.

## Capabilities

### New Capabilities

- `phototherapy-lamp-monitor`: qué reporta la placa sobre la lámpara de fototerapia (estado real en `CTRL,STATE`) y cómo se detecta y anuncia que la lámpara no responde a la orden.

## Impact

- **Código — motherBoard**: `src/tasks/CommTask.cpp` (campo 8), `src/modules/control/photo_fault.{h,cpp}` (nuevo), `src/modules/sensors/sensors_module.cpp` (cableado), `src/tasks/GPRS.cpp` y `src/tasks/Wifi_OTA.cpp` + `include/config/telemetry_keys.h` (clave de la alarma), `platformio.ini` (`build_src_filter` de `[env:native]`).
- **Código — shared/**: `include/alarm_ids.h` (id 20 **al final**, antes de `ALARM_COUNT`), `src/alarm_policy.cpp` (BAJA, no corta), `src/alarm_text.cpp` (título y acción en EN/ES/FR/PT).
- **Código — Display_HMI**: ninguno. Compila contra `shared/` y los textos de alarma salen de `alarm_text.cpp`; hay que comprobar que **compila** (`pio run` en `Display_HMI`). Una HMI sin actualizar recibiría el bit 20 del bitmask y lo ignoraría (`id >= ALARM_COUNT` descarta la línea `CTRL,ALM`): el aviso no se vería en esa pantalla hasta actualizarla, sin otro efecto.
- **Docs**: `Firmware/PROTOCOL.md`, `Firmware/docs/known_issues.md`, `Firmware/docs/alarms.md`, `Firmware/docs/alarm_popup_image_prompts.md` (fila de la imagen nueva, si la tabla lista todas las condiciones).
- **Heap**: sin buffers dinámicos nuevos; el módulo puro es una struct de 2 bytes en BSS.
- **Out of scope (Non-goals)**:
  - Corregir actuación, modo de control, consignas y modo piel en `CTRL,STATE` (D2).
  - Cambiar el comportamiento del lazo de intensidad ni sus constantes.
  - Cortar o limitar la lámpara ante el fallo: es un aviso, no una protección. Una lámpara sin corriente no es un riesgo; es una terapia que no se está dando.
  - Detectar sobrecorriente de la lámpara.
