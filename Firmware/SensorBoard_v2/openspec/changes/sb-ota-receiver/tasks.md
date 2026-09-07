# Tasks — sb-ota-receiver

> El emisor lo aporta `shared-cascade-ota-distribution` en `Firmware/openspec/`.
> Las fases 1-4 se verifican solas; la 5 necesita las dos placas.
>
> Al correr la suite: `idf.py` solo desde PowerShell (desde MSYS da falso verde),
> y `test_sensorboard_frame` arrastra un ERRORED 0xC0000139 previo a este cambio
> — mirar el SUMMARY suite a suite, no el código de salida global.

## 1. Red — tests que fallan

- [ ] 1.1 `SB_PROTO_TYPE_OTA` (0x02) y los subopcodes en `sensorBoard_comm_protocol.h`; header público del componente `ota` con la máquina de estados pura (`sb_ota_*`) sin dependencia de flash
- [ ] 1.2 `TEST_CASE`s `[ota]` en `test_apps/comm_test`, uno por Scenario de `ota-receiver`: subopcode desconocido, payload vacío, oferta sobredimensionada, oferta de tamaño 0, oferta durante transferencia, chunk en secuencia, chunk fuera de secuencia, chunk sobredimensionado, bytes recibidos distintos del tamaño ofertado, digest correcto/discrepante, abort explícito, silencio prolongado
- [ ] 1.3 `TEST_CASE`s `[ota]` para `ota-rollback`: `confirm` fuera de contexto, y `ota_state` en reposo y durante transferencia
- [ ] 1.4 `TEST_CASE`s `[hostwatch]` nuevos: no reinicia con transferencia activa, se rearma al acabar, se rearma al vencer el tope absoluto
- [ ] 1.5 `TEST_CASE`s `[frame]`: despacho del 0x02 al manejador OTA, tipos 0x00/0x01 sin cambio de camino, 0x02 sin manejador registrado, 0x02 con longitud excesiva
- [ ] 1.6 Confirmar el rojo: `idf.py build` de `comm_test` falla por símbolos sin definir (recordar `WHOLE_ARCHIVE` en las test apps)

## 2. Green — `usb_comm`

- [ ] 2.1 Despacho del tipo 0x02 en `sensorBoard_frame.c` hacia un manejador registrable, sin tocar magic, cabecera, CRC ni validación de longitud
- [ ] 2.2 `ota_state` en la resp de `status` (`sensorBoard_cmd_builder.c`) y su fuente en `sensorBoard_status.c`
- [ ] 2.3 Suspensión y rearme del vigilante de host en `sensorBoard_host_watch.c`, con tope absoluto, manteniendo la política como función pura testeable

## 3. Green — componente `ota`

- [ ] 3.1 Máquina de estados pura `sb_ota_*`: validación de oferta, secuencia de chunks, acumulación de SHA-256, transiciones de `ota_state`, abort y timeout
- [ ] 3.2 Capa de flash: `esp_ota_begin`/`write`/`end`/`set_boot_partition` sobre el slot inactivo, chunks de 4096 B alineados a página
- [ ] 3.3 Tarea FreeRTOS del componente y registro del manejador del tipo 0x02; `CMakeLists.txt` del componente y alta en el build raíz
- [ ] 3.4 Rollback: **no** marcar válida la imagen al arrancar; `esp_ota_mark_app_valid_cancel_rollback()` solo al recibir `confirm`; reportar `pending_verify` mientras tanto
- [ ] 3.5 `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y` en `sdkconfig.defaults`
- [ ] 3.6 **Comprobar las dos trampas**: que el componente esté dentro de `set(COMPONENTS ...)` para que sus símbolos Kconfig existan, y borrar `sdkconfig` para que los defaults nuevos entren. Verificar el símbolo en el `sdkconfig` generado, no asumirlo
- [ ] 3.7 Verde: `idf.py build` de `comm_test` pasa con todos los `[ota]`, `[hostwatch]` y `[frame]` nuevos

## 4. Verificación en placa (manual)

- [ ] 4.1 **Manual**: flashear por cable con `ota_data_initial.bin` en `0xD000` para dejar `otadata` en estado conocido, y confirmar que arranca `ota_0`
- [ ] 4.2 **Manual**: transferencia completa desde un host de desarrollo, comprobando `ota_state` en `status` en cada fase y que el slot inactivo queda escrito
- [ ] 4.3 **Manual**: `commit` con digest correcto → reinicio en la imagen nueva, `ota_state:"pending_verify"`
- [ ] 4.4 **Manual**: reinicio sin `confirm` → el bootloader revierte a la imagen anterior
- [ ] 4.5 **Manual**: `confirm` → imagen marcada válida y `ota_state:"idle"`; un reinicio posterior ya no revierte
- [ ] 4.6 **Manual**: corte de la transferencia a mitad (desenchufar) → la placa sigue con su imagen, el vigilante se rearma y una `offer` posterior se acepta
- [ ] 4.7 **Manual**: comprobar que el dispositivo sigue enumerando como 0x303A/0x4001

## 5. Extremo a extremo con la motherboard (manual, requiere el emisor)

- [ ] 5.1 **Manual**: transferencia real desde la motherboard con `shared-cascade-ota-distribution` ya implementado
- [ ] 5.2 **Manual**: confirmar que `ALARM_AIR_SENSOR_FAULT` salta durante el reinicio y se limpia sola cuando la imagen nueva publica — y que ninguna línea de este componente la suprime
- [ ] 5.3 **Manual**: imagen deliberadamente incapaz de publicar → la motherboard no confirma y el bootloader revierte

## 6. Documentación

- [ ] 6.1 `CHANGELOG.md`: entrada Added describiendo el receptor, el tipo 0x02 y el contrato de rollback, con la justificación de por qué se tocó `usb_comm`
- [ ] 6.2 `README.md`: cómo se actualiza esta placa ahora y qué sigue exigiendo cable (bootloader y tabla de particiones no viajan en un OTA)
- [ ] 6.3 Roadmap `docs/superpowers/specs/2026-07-03-sensorboard-roadmap.md`: registrar este trabajo como fuera de las cinco fases originales
- [ ] 6.4 ADR sobre la excepción a la regla de no tocar `usb_comm` y sobre por qué el rollback lo resuelve la confirmación de la motherboard y no el arranque
