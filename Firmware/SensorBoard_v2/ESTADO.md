# Estado del proyecto

> Testigo de momentum: qué se está haciendo ahora y cuál es el siguiente paso. Una ventana nueva de Claude Code empieza leyendo este archivo. Lo actualiza `retro-improver` al cerrar cada tarea (paso 7).

## Épica activa

**Enlace motherboard ↔ SensorBoard por USB** — rama `feat/sensorboard-usb-comm`
(worktree `Firmware/.worktrees/sensorboard-usb-comm`). Implementado y **validado
en banco con las tres placas reales** el 2026-09-02/03. Pendiente de merge a `dev`
(gate `guard-merge`, siempre humano).

**Lo que cambió respecto a lo que decía este archivo:** el SensorBoard **no es
telemetría auxiliar**. En los equipos nuevos, sus 3× SHT40 son el sensor de aire y
de humedad de la incubadora — la variable del PID y la fuente de los cortes
térmicos. Los pines 19/20 de la motherboard son el bus I2C2 hacia la PCBA de
sensores en los equipos **antiguos** y el USB hacia el SensorBoard en los
**nuevos**: multiplexado por generación, resuelto sondeando el bus I2C2 al
arrancar (sin NVS, una decisión por arranque).

## Verificado en hardware (2026-09-02/03)

- Detección de fuente, enumeración (VID `0x303A` / PID `0x4001`, interfaz 0),
  framing y CRC (0 errores en 3 capturas), camino host→device (`status` con `id`
  ecoado), cadencia real de `sensor_data` **1034 ms** (min 1034 / max 1036).
- **La prueba que cierra el diseño:** una MB que estaba en `AIR SENSOR FAULT` con
  el calefactor cortado (sus sensores ya no están en el I2C2) pasa a
  `alarmBitmask = 0x0` sostenido al subir el enlace. El lazo de control queda
  alimentado por el SensorBoard.
- Margen de arranque de la alarma de enlace, log limitado, texto de alarma.

## Trabajo puntual (fuera de épica)

- **`feat/sb-usb-pin-autoswap` — cerrado el 2026-09-03.** Tolerancia al conector USB
  invertido (HW_NUM 4; la V5 lo corrige en hardware): autoswap de D+/D- en el PHY
  del S3 **solo con evidencia de host** — tras un bus reset sin SETUP en 2 s,
  una vez por reset, quieto después (ADR-0003 + enmienda, changes archivados
  `2026-09-03-sb-usb-pin-autoswap` y `2026-09-03-sb-usb-autoswap-host-evidence`,
  retro `docs/retro/2026-09-03-usb-pin-autoswap.md`). La primera versión
  alternaba a ciegas cada 2 s y enganchaba en fase con la pila host de la
  motherboard (banco 2026-09-03): funcionaba con el PC y fallaba con la MB. Rama en el **checkout principal** desde el 2026-09-03 (worktree `sb-usb-autoswap` retirado). Antes worktree:
  `Firmware/.worktrees/sb-usb-autoswap`. **Integra `feat/sensorboard-usb-comm`
  por merge (2026-09-03)**, así que esta rama contiene el enlace completo más el
  autoswap y el vigilante de host; es la candidata a merge a `dev`. Verificación
  on-target pendiente (checklist en el `design.md` del change).

## Próximo paso (requiere a Pablo)

1. **Flashear y verificar el vigilante de host del SensorBoard** (implementado y
   compilado, *sin probar en placa*): al perder el host >15 s habiéndolo tenido,
   se reinicia para forzar la re-enumeración. Sin esto, un reinicio de la
   motherboard deja la incubadora sin sensor de aire hasta que alguien
   desenchufa el conector a mano. Es el pendiente **bloqueante**.
   **Corregido el 2026-09-03 (commits `cbb3faf`/`d563483`):** la primera versión
   miraba solo el DTR y no se disparaba nunca en el caso real — desenchufar el
   cable o reiniciar la motherboard no cambia el DTR (TinyUSB solo lo notifica
   ante `SET_CONTROL_LINE_STATE`) y sin sensado de VBUS el bus solo reporta
   `SUSPEND`. Ahora la señal es `DTR && tud_ready()` y, además, se emite un
   heartbeat inmediato al volver el host. **Prueba de banco:** con el SensorBoard
   alimentado, desenchufar el cable a la motherboard, esperar ≥15 s, volver a
   enchufar; la motherboard debe loguear "SensorBoard conectado por USB" y el
   enlace levantarse en ~1 s. Tests Unity: `idf.py -C test_apps/comm_test`
   (`[hostwatch]`).
2. **Verificación on-target del autoswap USB** (4 casos del checklist en el
   `design.md` del change archivado).
3. Aprobar el merge de `feat/sb-usb-pin-autoswap` (que ya incluye
   `feat/sensorboard-usb-comm`) a `dev` con `merge --no-ff`. Al tocar `shared/`
   (`ALARM_COUNT` 18 → 20), **motherboard y display deben flashearse juntos**: un
   HMI viejo descarta las alarmas 18/19 por id fuera de rango.
4. **Flasher (`Firmware/flasher_tool`):** indicar "gira el cable USB" si no detecta
   el SensorBoard — el bootloader ROM no aplica el intercambio de pines.
5. Verificación on-target de la cámara: la unidad de pruebas **no lleva cámara**
   (`sensors.cam:false`, `capture` responde `"cmd not found"`), así que todo el
   camino de captura sigue sin probarse en hardware.

## Épicas cerradas

- **EPIC-001 · Roadmap SensorBoard Fases 1-5** — 5 changes OpenSpec archivados, 5
  retros. Mergeado a `dev` vía `claude_agents_tests` (commit `d31d7c3`), al
  contrario de lo que decía la versión anterior de este archivo.
- **EPIC-000 · Adaptación del framework Genesis a ESP-IDF** — archivada como
  `2026-07-02-adapt-genesis-esp-idf`.

## Últimas decisiones relevantes

- TX con dos colas: JSON siempre prioritario; máx. 1 binario en vuelo (Fase 5).
- Enlace USB = canal de confianza intra-dispositivo (riesgo documentado en README).
- Gates de plausibilidad en todos los sensores (temp, dB, señal viva PDM) — CRC/lectura OK ≠ dato válido.
- La verificación automática (Stop hook) es solo `idf.py build`; flash/Unity on-target siempre manual. En Windows, `idf.py` debe lanzarse desde PowerShell: desde Git Bash (MSYS) sale con código 0 sin compilar.
- Host activo = `tud_mounted() || tud_connected()` (`connected` = primer SETUP, imposible con D+/D- cruzados); autoswap D+/D- en el PHY antes que cualquier fallback UART/I2C, y **solo con evidencia de host** (bus reset), nunca alternando a ciegas: la pila host ESP-IDF 4.4.6 de la MB se recupera solo con una desconexión y una alternancia periódica engancha en fase (ADR-0003). El hook `tud_event_hook_cb` corre en ISR en el port DWC2/S3.
- Los `TEST_CASE` Unity en ficheros sin `app_main` exigen `WHOLE_ARCHIVE` en el test app.
- La cadencia de `sensor_data` es **1 s**, no 5: era igual al umbral de caducidad
  del sensor de aire de la motherboard (5 s), así que una sola publicación
  perdida cortaba el calefactor. Cadena viva: publica 1 s → rancio a 3.6 s → deja
  de refrescarse el sello → `AIR_SENSOR_FAULT` a los 5 s.
- **Votación, no media**, al fusionar las 3 posiciones: con una media simple un
  solo SHT40 sesgado engañaba a la vez al PID, al corte térmico y a la alarma de
  desviación (todos leen la misma variable). Mediana + descarte por dispersión.
- El SensorBoard **no transmite nada sin DTR asertado** por el host.
- Su PID USB es un contrato entre placas: lo calcula TinyUSB desde las clases
  habilitadas, así que activar MSC lo movería y rompería el enlace en silencio.
- "Host presente" = DTR **y** `tud_ready()`: el DTR solo no detecta un cable
  desenchufado ni un reinicio de la motherboard (vigilante de host, 15 s).

## Seguimientos diferidos

- **Contrato de puerta a medias**: se detecta el flapping pero no el `door_open`
  sostenido implausible (hall desconectado), que el README exige igual.
- **Micrófono descalibrado**: ~60 dB en reposo medidos en banco; deberían ser
  35-45. Sin ponderación A y sin calibrar contra sonómetro.
- Puerta, luz y sonido **no llegan al HMI**: solo a ThingsBoard como `sb_*`.
- Subida del JPEG a Drive: vía mapeada (patrón `DriveUpload`), pero pide un **ADR
  de protección de datos** antes del código — es la imagen de un paciente.
- Calibraciones (ALS, dBA), recovery de bus I2C colgado, pool estático cJSON.
