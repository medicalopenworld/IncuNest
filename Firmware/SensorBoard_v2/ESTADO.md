# Estado del proyecto

> Testigo de momentum: qué se está haciendo ahora y cuál es el siguiente paso. Una ventana nueva de Claude Code empieza leyendo este archivo. Lo actualiza `retro-improver` al cerrar cada tarea (paso 7).

## Épica activa

**Enlace motherboard ↔ SensorBoard por USB** — **integrado en `dev` el 2026-09-03**
(merge `24600af` de `feat/sb-usb-pin-autoswap`, que ya contenía
`feat/sensorboard-usb-comm`), y empujado a `origin/dev`. Validado en banco con las
tres placas reales el 2026-09-02/03. Sigue activa por la verificación on-target
pendiente (vigilante de host, autoswap, cámara — ver "Próximo paso").

> El worktree `Firmware/.worktrees/sensorboard-usb-comm` está **obsoleto**: su rama
> ya está en `dev` y quedaron ahí ficheros modificados sin commitear que son copia
> de lo que ya se subió. Descártalos o retira el worktree antes de volver a tocarlo,
> y trabaja desde el checkout principal.

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
  retro `docs/retro/2026-09-03-usb-pin-autoswap.md`). **Verificado en banco con la
  motherboard V18 en ambas orientaciones y re-enchufe en caliente (2026-09-03).**
  La primera versión alternaba a ciegas cada 2 s; la causa raíz de que fallara con
  la MB y no con el PC era `gpio_reset_pin(19/20)` en la motherboard dejando el
  pull-up interno sobre las líneas USB (fix `5b8af22` en `sensorboard_comm.cpp`). Rama en el **checkout principal** desde el 2026-09-03 (worktree `sb-usb-autoswap` retirado). Antes worktree:
  `Firmware/.worktrees/sb-usb-autoswap`. **Integraba `feat/sensorboard-usb-comm`
  por merge (2026-09-03)**, así que llevó a `dev` el enlace completo más el
  autoswap y el vigilante de host. **Mergeada a `dev` en `24600af`.** Verificación
  on-target pendiente (checklist en el `design.md` del change).

- **Telemetría posicional de los 3 SHT40 + GPRS — 2026-09-03.** Commits `0000711`
  (refactor), `485e35a` (11 tests en host), `26c01e9` (docs) y `12b2584` (GPRS),
  directos en `dev` y ya en `origin/dev`.
  - Las tres lecturas crudas viajan a ThingsBoard como `sb_temp0/1/2_C` y
    `sb_hum0/1/2_pct`, omitiendo la clave de la posición caída en vez de mandar un
    0 (un 0 en la nube es indistinguible de una medida). `Air_temp` sigue siendo la
    mediana, que es la variable del PID y del corte térmico.
  - El mapeo se extrajo a `sb_telemetry.{h,cpp}` (puro, en el `build_src_filter` de
    `native`) para poder probarlo en host: `test/test_sensorboard_telemetry/`.
  - `TX_GROUP_SENSORBOARD_GPRS` a 1 y `THINGSBOARD_FIELDS_AMOUNT` 96 → 112
    (+512 B de `.bss` medidos). Antes no cabía: 87 claves de peor caso GPRS + 12
    del bloque = 99 > 96.
  - Verificado sobre `dev`: `check_transport_matrix.py` OK, **293/293** tests
    nativos, `pio run -e IncuNest_V18` SUCCESS (RAM 86 504 B). **Sin probar en
    placa** — nadie ha visto todavía estas claves en ThingsBoard.

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
3. **Ver las claves nuevas en ThingsBoard.** Flashear desde el checkout principal
   (`cd Firmware/motherBoard && pio run -e IncuNest_V18 -t upload`). Requisitos para
   que aparezca cualquier `sb_*`: el I2C2 vacío (log `No air sensor on I2C2:
   starting SensorBoard USB link ...`) y el enlace arriba. Cadencia WiFi 5 s;
   por GPRS, según `TX_GPRS_PERIOD_*`.
   **La prueba que importa:** desconectar un SHT40 → su clave debe **desaparecer**
   del payload (no salir a 0), `sb_env_used` bajar a 2 y `Air_temp` quedarse plano.
   Y que no aparezca `TELEMETRIA TRUNCADA` en el log.
   Al tocar `shared/` (`ALARM_COUNT` 18 → 20), **motherboard y display deben
   flashearse juntos**: un HMI viejo descarta las alarmas 18/19 por id fuera de rango.
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
- **Mediana, no media**, al fusionar las 3 posiciones: con una media simple un
  solo SHT40 sesgado engañaba a la vez al PID, al corte térmico y a la alarma de
  desviación (todos leen la misma variable). La media es la única opción que puede
  devolver un valor que **ningún** sensor está midiendo. Con 2 plausibles se
  promedia (no hay tercero que arbitre), con 1 se acepta, con 0 no hay medida y
  salta `AIR_SENSOR_FAULT`.
- **Sin cribado por dispersión** (se quitó el `max_spread` de la primera versión):
  descartar por desviarse de los compañeros se diseñará con datos reales de flota,
  y para eso las tres crudas viajan a la nube. El único filtro que queda es el gate
  de plausibilidad de cabina (15-50 °C).
- **Los tres SHT40 comparten ubicación física**, y eso es lo que hace válida la
  mediana: si estuvieran repartidos por la cabina estaría mezclando un gradiente
  real. Mapeo índice → sensor en `docs/hardware.md`.
- **Las crudas se llaman `sb_temp0/1/2_C`, no `Air_temp_0/1/2`**, y a propósito: la
  magnitud clínica es `Air_temp` (la que lee el PID); las tres crudas son datos de
  mantenimiento. Nombrarlas en la familia `Air_temp_*` invitaría a promediarlas en
  un widget de dashboard, que es justo el peligro que evita la mediana.
- **`MAX_MESSAGE_SIZE` (1024 B) no limita la telemetría**: `main.h` define
  `THINGSBOARD_ENABLE_STREAM_UTILS`, y con eso el SDK publica en streaming
  (`begin_publish` + `BufferingPrint`) rodeando el buffer MQTT. El único techo real
  es `THINGSBOARD_FIELDS_AMOUNT` (el pool de ArduinoJson), y desbordarlo **descarta
  las claves últimas insertadas** — que son las `sb_*`, por ir al final.
- El SensorBoard **no transmite nada sin DTR asertado** por el host.
- Su PID USB es un contrato entre placas: lo calcula TinyUSB desde las clases
  habilitadas, así que activar MSC lo movería y rompería el enlace en silencio.
- "Host presente" = DTR **y** `tud_ready()`: el DTR solo no detecta un cable
  desenchufado ni un reinicio de la motherboard (vigilante de host, 15 s).

## Seguimientos diferidos

- **Los hooks de gitflow no protegen las sesiones abiertas desde `SensorBoard_v2`.**
  Detectado el 2026-09-03: un `git push origin dev` pasó sin que `guard-push` lo
  bloqueara. Están registrados solo en `Firmware/.claude/settings.json`, y
  `SensorBoard_v2/.claude/` únicamente tiene `settings.local.json` y `logs`. El
  agujero afecta igual a **`guard-merge`**, que es el gate humano obligatorio del
  dispositivo médico: un merge a `main` desde esta carpeta pasaría sin aprobación.
  `CLAUDE.md` describe ambos como activos. Arreglo: registrarlos también en
  `SensorBoard_v2/.claude/settings.json`, o abrir las sesiones desde `Firmware/`.
- **Contrato de puerta a medias**: se detecta el flapping pero no el `door_open`
  sostenido implausible (hall desconectado), que el README exige igual.
- **Micrófono descalibrado**: ~60 dB en reposo medidos en banco; deberían ser
  35-45. Sin ponderación A y sin calibrar contra sonómetro.
- Puerta, luz y sonido **no llegan al HMI**: solo a ThingsBoard como `sb_*`.
- Subida del JPEG a Drive: vía mapeada (patrón `DriveUpload`), pero pide un **ADR
  de protección de datos** antes del código — es la imagen de un paciente.
- Calibraciones (ALS, dBA), recovery de bus I2C colgado, pool estático cJSON.
