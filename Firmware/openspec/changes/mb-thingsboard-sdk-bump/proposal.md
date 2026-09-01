## Why

Afecta a **motherBoard** exclusivamente. `thingsboard-arduino-sdk` v0.14.0 hizo un
"Overhaul Library API Design" (soporte de SSL cert bundle en
`Espressif_MQTT_Client`) y v0.15.0 añade fixes orientados a placas Arduino —
motivo real pero no bloqueante: hoy el firmware funciona bien en v0.13.0, la
versión actualmente pineada y commiteada en `feat/pin-lib-versions`.

Investigación ya hecha y verificada por compilación real (no hipotética): subir
motherBoard a `thingsboard-arduino-sdk#v0.15.0` + `TBPubSubClient@2.12.1` con
`pio run -e IncuNest_V18` falla en un **bug interno del propio SDK**, no
arreglable eligiendo otra versión intermedia salvo volviendo a v0.14.0 (que
compila pero no aporta nada nuevo frente a v0.13.0). No hay ningún bug de este
tipo documentado en `Firmware/docs/known_issues.md`; es una incompatibilidad de
librería de terceros descubierta en esta investigación, no un fallo de
producción propio.

## What Changes

- **No se cambia ningún `platformio.ini` en este change.** El objetivo de este
  change es dejar documentada la investigación y forzar una **decisión de
  arquitectura explícita** antes de tocar código — ver `tasks.md`, tarea 1.
- Se documenta el bloqueo técnico exacto (ver `design.md`): `ThingsBoard.h:289`
  llama a `IMQTT_Client::get_buffer_size()`, método que v0.15.0 eliminó en favor
  de `get_receive_buffer_size()`/`get_send_buffer_size()` (para soportar los
  buffers TX/RX separados que introdujo `TBPubSubClient>=2.10`), pero la propia
  librería no actualizó esa llamada. Se reproduce igual contra `#v0.15.0` y
  contra `#master` actual del SDK.
- Esa línea vive dentro de `#if THINGSBOARD_ENABLE_STREAM_UTILS`, que
  motherBoard define a `1` en `motherBoard/include/main.h:8` (junto con
  `THINGSBOARD_BUFFER_SIZE 4096`) a propósito, para poder mandar payloads
  mayores de 4096 bytes (p. ej. el snapshot PPG) vía streaming. No se puede
  esquivar el bug apagando ese flag sin perder esa capacidad — a diferencia de
  Display_HMI, que sí lo tiene apagado (ver `hmi-thingsboard-sdk-bump`).
- Se documentan tres caminos de resolución posibles (report upstream y esperar
  / fork parcheado propio de 1-2 líneas, con el precedente ya existente en este
  repo de `medicalopenworld/incunest_afe4490` / descartar la subida), sin
  decidir cuál en este proposal — esa decisión es la primera tarea de
  `tasks.md` y debe registrarse en `design.md` antes de que exista ningún commit
  de código.

## Capabilities

### New Capabilities
- `thingsboard-sdk-compatibility`: el conjunto de condiciones que motherBoard
  debe cumplir para poder subir de versión el SDK de ThingsBoard y su cliente
  MQTT sin perder la capacidad de streaming de payloads grandes, y el requisito
  de que la resolución del bug de terceros quede decidida y registrada antes de
  tocar `platformio.ini`.

### Modified Capabilities
(ninguna — `openspec/specs/` no tiene hoy ninguna capability sobre dependencias
de ThingsBoard; el pineo actual de v0.13.0/TBPubSubClient@2.9.4 en
`feat/pin-lib-versions` nunca se formalizó como capability de OpenSpec)

## Impact

- **Afectado si se implementa** (no en este change): `motherBoard/platformio.ini`
  (`lib_deps` de `thingsboard-arduino-sdk` y `TBPubSubClient`), y potencialmente
  un fork propio si esa es la vía elegida.
- **No afectado**: ningún fichero de código de motherBoard cambia en este
  proposal. `motherBoard/include/main.h:8` (`THINGSBOARD_ENABLE_STREAM_UTILS`,
  `THINGSBOARD_BUFFER_SIZE`) se documenta como restricción, no se toca.
- **Rama de trabajo actual `feat/pin-lib-versions`**: no se toca ni se depende
  de ella — ya resolvió, por separado, el problema real de que
  `TBPubSubClient@2.9.4` desapareció del registro de PlatformIO, pineando el
  mismo v2.9.4 vía URL git. Ese fix es independiente de este proposal.
- **Coordinación con `hmi-thingsboard-sdk-bump`**: ambas librerías van
  acopladas (mismo SDK, mismo cliente MQTT) pero los bloqueos son de naturaleza
  distinta — un bug de terceros aquí, una migración de API propia allí. Si la
  decisión de la tarea 1 de este change es "descartar la subida", el change de
  Display_HMI queda igualmente sin ejecutar aunque su bloqueo sea técnicamente
  superable.
