## Why

Placas afectadas: **motherBoard**, **Display_HMI** y **`shared/`** (protocolo). Área con bug documentado en `Firmware/docs/known_issues.md`: la UART MB↔HMI (el cambio añade una línea periódica nueva; ver el riesgo de inundación en `design.md`).

La SensorBoard (por USB, en los equipos nuevos) ya entrega a la motherBoard el estado de la puerta (hall DRV5032), la luz (ALS-PT19) y el sonido (ICS-41350). Hoy ese dato **solo llega a ThingsBoard** como claves `sb_*` (`sb_telemetry.cpp`): quien está junto a la incubadora no ve que la puerta está abierta ni que hay ruido, y es justo el operador quien puede actuar sobre ambas cosas. `SensorBoard_v2/ESTADO.md` lo anota como deuda: "Puerta, luz y sonido **no llegan al HMI**".

Restricciones de partida:

- La puerta **no entra en el lazo térmico** (decisión de producto): esto es solo informativo. La alarma `ALARM_SENSORBOARD_DOOR_FAULT` ya existe y ya señaliza el hall sospechoso; no se duplica.
- El micrófono y el ALS **no están calibrados** (`ESTADO.md`: ~60 dB en reposo cuando deberían ser 35-45; la conversión a lux depende de una resistencia de carga sin calibrar). Un número de dB en pantalla se leería como una medida y no lo es.
- `CTRL,STATE` va al filo de su buffer (`msg[192]`) y otra rama abierta reserva su campo 24: no se le añaden campos.

## What Changes

- **Mensaje nuevo `CTRL,ENV,link,door,lux,dbA`** (MB → HMI), con centinelas de "no disponible" (`-1`) y la misma filosofía que `CTRL,TEL`: un valor que no se sabe no viaja como `0`. Cadencia: **inmediato** ante un cambio de enlace o de puerta, y cada **5 s** si no hay cambios (no a 1 Hz).
- **motherBoard**: módulo puro `sb_env_report` (construcción de la línea desde el snapshot de la SensorBoard + decisión de cuándo enviarla), con test Unity en `[env:native]`; `CommTask.cpp` lo invoca y emite por `hmiSerial`. Solo se emite en equipos con SensorBoard (`SENSOR_SOURCE_SENSORBOARD`): las unidades con I2C2 no mandan nada.
- **Display_HMI**: parser de `CTRL,ENV` y clasificación cualitativa del ruido (**Bajo / Medio / Alto**, umbrales provisionales) en un módulo puro `sb_env` sin LVGL, con test Unity en el nuevo `[env:native]` del HMI. UI: indicador de **puerta abierta** y de **nivel de ruido** sobre la franja libre del botón del reloj del heading de Main (a la derecha de la hora y la fecha) y réplica en el heading de la pantalla de bloqueo. Textos en las 4 lenguas.
- **`shared/include/protocol.h`**: constantes del mensaje (`PROTO_ENV_*`), compartidas por ambas placas.
- **La luz viaja pero no se pinta.** Va en el mensaje (reservada, ya parseada y probada) porque la decisión de producto de pintarla se podrá tomar sin tocar el protocolo cuando el ALS esté calibrado; hoy nada en el HMI la necesita.
- No breaking: un HMI antiguo ignora la línea desconocida (cae al final de la cadena de `parse_message`); una MB antigua no la manda y el HMI nuevo no pinta nada.

## Capabilities

### New Capabilities
- `sb-env-protocol`: formato, centinelas y cadencia del mensaje `CTRL,ENV` y su construcción en la motherBoard desde el estado de la SensorBoard.
- `hmi-sb-env-heading`: cómo el display interpreta `CTRL,ENV`, la clasificación cualitativa del ruido, cuándo se pinta y cuándo no (sin SensorBoard, enlace caído, dato rancio, puerta en fallo) y dónde.

### Modified Capabilities
(ninguna: ningún requisito de las specs existentes cambia.)

## Impact

- **motherBoard**: `src/modules/sensorboard_comm/sb_env_report.{h,cpp}` (nuevo, puro); `src/tasks/CommTask.cpp` (llamada y envío); `platformio.ini` (`[env:native]` `build_src_filter`); `test/test_sensorboard_env_report/` (nuevo).
- **Display_HMI**: `src/modules/comm/sb_env.{h,cpp}` (nuevo, puro); `src/tasks/CommTask.cpp` (rama nueva en `parse_message`, estado compartido con la UI); `src/ui/ElementsCreation.cpp` y `include/ui/ElementsCreation.h` (indicador en Main y en Lock); `src/tasks/UITask.cpp` (`sb_env_heading_update()` con el patrón de `clock_update()`); `include/ui/i18n_strings.def` (textos); `platformio.ini` (`[env:native]`); `test/test_sb_env/` (nuevo).
- **shared**: `include/protocol.h` (constantes). Ambos consumidores se actualizan en este mismo cambio.
- **Documentación**: `PROTOCOL.md` (sección `CTRL,ENV`), `docs/hmi.md`.
- **Presupuesto UART**: ~25 B cada 5 s (0,2 B/s de media frente a los >100 B/s que ya cruza el enlace con PPG a 25 Hz): despreciable; se mide el tamaño de la línea en el test.
- **Compatibilidad**: no requiere flashear las dos placas a la vez. HMI nuevo + MB vieja: no se pinta nada. HMI viejo + MB nueva: ignora la línea.
- **Verificación manual pendiente** (no hay entorno de test para LVGL ni para el cableado de `CommTask`): checklist de banco en `tasks.md`.
