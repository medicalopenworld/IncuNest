## Why

Afecta a **Display_HMI** exclusivamente. Igual que en `mb-thingsboard-sdk-bump`,
la motivación de subir `thingsboard-arduino-sdk` (v0.13.0 -> v0.15.0) y
`TBPubSubClient` (v2.9.4 -> v2.12.1) es real pero no bloqueante: hoy el firmware
funciona bien en v0.13.0, versión actualmente pineada y commiteada en
`feat/pin-lib-versions`.

Display_HMI ya tiene `THINGSBOARD_ENABLE_STREAM_UTILS=0` en su
`platformio.ini` (`[env:main]` y el resto de entornos), así que **no** sufre el
bug de `IMQTT_Client::get_buffer_size()` que bloquea a motherBoard (ver
`mb-thingsboard-sdk-bump`) — comprobado compilando. Pero el "Overhaul Library
API Design" de v0.14 renombró y reestructuró la API de actualización OTA y de
provisioning que usa `Display_HMI/src/tasks/Wifi_OTA.cpp`, y compilar contra
v0.15.0 falla ahí con varios errores reales (ver `design.md` para las citas
exactas del compilador). Esto no es un bug de terceros: es una migración de API
propia pendiente de escribir.

Esta ruta toca actualización de firmware por OTA y provisioning, área que
`Firmware/.claude/rules/security.md` marca como sensible (arranque/carga de
firmware) — cualquier implementación futura de este change debe verificarse en
hardware real antes de darse por buena, nunca solo por compilación.

## What Changes

- **No se cambia ningún `platformio.ini` en este change**, ni se toca
  `Wifi_OTA.cpp`. El objetivo de este change es documentar la migración de API
  pendiente y dejarla lista para implementar cuando se decida acometerla.
- Se documentan los errores de compilación reales obtenidos al compilar
  Display_HMI contra `thingsboard-arduino-sdk#v0.15.0` +
  `TBPubSubClient@2.12.1`: `OTA_Update_Callback` y `Provision_Callback` ya no
  existen con ese nombre, `ThingsBoardSized<>::Firmware_Send_Info()`,
  `::Start_Firmware_Update()` y `::Provision_Request()` fueron renombrados o
  reestructurados, y `sendTelemetryJson()` deja de aceptar un
  `ArduinoJson::JsonObject` y pasa a exigir un `const JsonDocument&`.
- **BREAKING** (a nivel de API interna de `Wifi_OTA.cpp`, no de protocolo
  motherBoard<->HMI): cualquier implementación futura de este change requiere
  reescribir las llamadas de actualización OTA y de provisioning contra la API
  nueva del SDK, no solo renombrar símbolos — la reestructuración es funcional,
  no cosmética (ver `design.md`).
- Se deja explícito que, a diferencia de motherBoard, aquí no hay decisión de
  arquitectura previa que tomar (no hay bug de terceros que resolver): el
  camino es directo, pero requiere trabajo de migración y verificación en
  hardware real antes de mergear.

## Capabilities

### New Capabilities
- `thingsboard-ota-provisioning-migration`: el comportamiento que la
  actualización OTA y el provisioning de Display_HMI deben seguir cumpliendo
  tras migrar a la API nueva del SDK (v0.14+) — mismo resultado observable
  (actualización de firmware exitosa, aprovisionamiento exitoso, telemetría
  publicada sin pérdida de campos) con la API reestructurada.

### Modified Capabilities
(ninguna — `openspec/specs/` no tiene hoy ninguna capability sobre OTA,
provisioning ni dependencias de ThingsBoard en Display_HMI)

## Impact

- **Afectado si se implementa** (no en este change): `Display_HMI/platformio.ini`
  (`lib_deps` de `thingsboard-arduino-sdk` y `TBPubSubClient`),
  `Display_HMI/src/tasks/Wifi_OTA.cpp` (llamadas de OTA y provisioning,
  `sendTelemetryJson`).
- **No afectado**: `THINGSBOARD_ENABLE_STREAM_UTILS=0` en
  `Display_HMI/platformio.ini` no cambia — es precisamente lo que evita el
  bloqueo de motherBoard aquí. Ningún fichero de código cambia en este
  proposal.
- **Verificación futura de alto riesgo**: el ciclo de actualización OTA y el
  flujo de provisioning deben volver a probarse en hardware real tras
  cualquier implementación — no hay entorno de test en Display_HMI y esta ruta
  toca arranque/carga de firmware (`.claude/rules/security.md`).
- **Coordinación con `mb-thingsboard-sdk-bump`**: mismo par de librerías,
  bloqueo de naturaleza distinta (migración de API propia, no bug de
  terceros). Si la tarea 1 de `mb-thingsboard-sdk-bump` decide descartar la
  subida, este change queda igualmente sin ejecutar por coherencia de
  versiones entre placas, aunque su bloqueo sea técnicamente superable de forma
  independiente.
