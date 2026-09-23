## Context

Estado actual (commiteado y funcionando en `feat/pin-lib-versions`, no tocado
por este change): `Display_HMI/platformio.ini` fija
`thingsboard-arduino-sdk#v0.13.0` y `TBPubSubClient@2.9.4` (vía URL git, mismo
motivo de resolución de dependencias que en motherBoard, resuelto por
separado). `THINGSBOARD_ENABLE_STREAM_UTILS=0` está definido en los tres
entornos de `platformio.ini` (`[env:main]` y el resto).

Con ese flag a `0`, Display_HMI **no** reproduce el bug de
`IMQTT_Client::get_buffer_size()` documentado en `mb-thingsboard-sdk-bump` —
confirmado compilando: esa línea de `ThingsBoard.h:289` vive dentro de
`#if THINGSBOARD_ENABLE_STREAM_UTILS`, que aquí nunca se activa.

**Hallazgo verificado por compilación real**: compilar Display_HMI contra
`thingsboard-arduino-sdk#v0.15.0` + `TBPubSubClient@2.12.1` sí falla, pero por
un motivo distinto — en `Display_HMI/src/tasks/Wifi_OTA.cpp`, con varios
errores porque el "Overhaul Library API Design" de v0.14 renombró/reestructuró
la API de actualización OTA y de provisioning:

```
error: 'OTA_Update_Callback' does not name a type
error: 'class ThingsBoardSized<>' has no member named 'Firmware_Send_Info'
error: 'class ThingsBoardSized<>' has no member named 'Start_Firmware_Update'
error: 'Provision_Callback' does not name a type
error: 'class ThingsBoardSized<>' has no member named 'Provision_Request'
error: cannot convert 'ArduinoJson::V6215PB2::JsonObject' to 'const ArduinoJson::V6215PB2::JsonDocument&' (sendTelemetryJson)
```

A diferencia del bloqueo de motherBoard, esto **no es un bug del SDK**: es una
migración de API real y documentada a medias por el propio proyecto
ThingsBoard (el CHANGELOG habla de "Overhaul Library API Design" sin dar una
guía de migración línea por línea). Arreglarlo significa reescribir las
llamadas contra la API nueva, no renombrar un símbolo.

## Goals / Non-Goals

**Goals**
- Dejar documentados, con las citas de error reales, todos los puntos de
  `Wifi_OTA.cpp` que requieren reescritura para que quien implemente esto no
  tenga que volver a compilar contra v0.15.0 para descubrirlos.
- Dejar explícito que esta migración, a diferencia de la de motherBoard, no
  tiene una decisión de arquitectura previa que tomar — es directamente un
  trabajo de reescritura + verificación en hardware real, pero **no debe
  empezar** hasta que `mb-thingsboard-sdk-bump` haya decidido su vía (ver
  Impact de `proposal.md`), para no dejar las dos placas en SDKs distintos sin
  una razón deliberada.

**Non-Goals**
- No se reescribe `Wifi_OTA.cpp` en este change.
- No se toca `platformio.ini`.
- No se investiga aquí si la API nueva del SDK trae alguna mejora funcional
  más allá de compilar — el alcance es "seguir haciendo lo mismo que hoy, con
  la API nueva", no ampliar el alcance de OTA/provisioning.

## Decisions

### D1: La migración se hace de una vez, no símbolo a símbolo con shims

*Alternativa descartada*: mantener wrappers/`#define` que traduzcan los
nombres viejos a los nuevos para minimizar el diff. Se descarta porque el
cambio no es solo de nombre — `Firmware_Send_Info`/`Start_Firmware_Update`/
`Provision_Request` fueron reestructurados (parámetros, callbacks), y
`sendTelemetryJson` cambió de firma de tipo (`JsonObject` -> `JsonDocument`).
Un shim que solo renombre compilaría pero probablemente con semántica
incorrecta, que es peor que un error de compilación explícito.

### D2: Verificación en hardware real es obligatoria antes de dar el cambio
por cerrado, no solo compilación

*Por qué*: `.claude/rules/security.md` marca arranque/carga de firmware como
zona sensible. Un `pio run -e main` en verde no prueba que un ciclo de
actualización OTA real complete, ni que el provisioning obtenga credenciales
válidas de ThingsBoard — ambos son I/O de red contra el backend real, no lógica
pura verificable en el host. No existe entorno de test en Display_HMI
(`.claude/rules/embedded-display-hmi.md`), así que esta verificación no es
automatizable hoy.

### D3: No se decide en este design.md el orden relativo a
`mb-thingsboard-sdk-bump`

Ambas librerías van acopladas (mismo SDK, mismo cliente MQTT), pero los dos
bloqueos son independientes técnicamente: nada impide implementar esta
migración de API sin que motherBoard haya resuelto su bug de terceros. La
razón para no implementar esto en solitario es de coherencia de proyecto (dos
placas en versiones de SDK distintas sin que sea una decisión explícita), no
una dependencia técnica dura — se deja como pregunta abierta, no como bloqueo
duro, porque quien retome esto puede decidir que sí vale la pena desacoplarlas.

## Risks / Trade-offs

- **[Riesgo] Regresión silenciosa en OTA o provisioning** tras la
  reescritura, que solo se manifiesta en campo → Mitigación: D2, verificación
  manual obligatoria en hardware real antes de mergear, documentando
  explícitamente qué se probó (ver tasks.md).
- **[Riesgo] `sendTelemetryJson(JsonObject)` -> `sendTelemetryJson(const
  JsonDocument&)` cambia semántica de vida útil del objeto** (un `JsonObject`
  es una vista sobre un documento existente; según la reestructuración de
  v0.14+, el nuevo API podría esperar poseer o serializar el documento
  completo de otra forma) → Mitigación: leer el código fuente de la firma
  nueva en el SDK v0.15.0 antes de portar la llamada, no asumir que es un
  simple cambio de tipo de parámetro.
- **[Riesgo] Documentación oficial de migración incompleta** ("Overhaul
  Library API Design" sin guía paso a paso) → Mitigación: el `CHANGELOG`/diff
  del propio repo del SDK entre v0.13.0 y v0.14.0 es la fuente más confiable;
  presupuestar tiempo de lectura de código fuente, no solo de changelog.

## Migration Plan

No aplica todavía: este change no modifica código ni `platformio.ini`. Cuando
se implemente (fuera de este proposal), el plan deberá cubrir: reescribir las
llamadas de OTA y provisioning contra la API nueva, `pio run -e main` en
verde, y un ciclo completo de actualización OTA + provisioning verificado en
hardware real contra un backend ThingsBoard real antes de mergear. Rollback:
revertir a v0.13.0 pineado si la verificación en hardware falla.

## Open Questions

- ¿Se implementa después, junto con, o independientemente de
  `mb-thingsboard-sdk-bump`? (D3 — no es una dependencia técnica dura, es una
  decisión de coherencia de proyecto.)
- ¿Cuál es exactamente la firma nueva de `Firmware_Send_Info`/
  `Start_Firmware_Update`/`Provision_Request` en v0.15.0? Pendiente de lectura
  del código fuente del SDK al empezar la implementación — no se ha
  investigado a ese nivel de detalle en este proposal.
