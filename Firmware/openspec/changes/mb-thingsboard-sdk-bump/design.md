## Context

Estado actual (commiteado y funcionando en `feat/pin-lib-versions`, no tocado
por este change): `motherBoard/platformio.ini` fija
`thingsboard-arduino-sdk#v0.13.0` y `TBPubSubClient@2.9.4` (vía URL git, porque
el tag `@2.9.4` ya no existe en el registro de PlatformIO pero el tag sigue
vivo en GitHub). Eso es un problema de resolución de dependencias, ya resuelto
por separado, y no depende de este change.

`motherBoard/include/main.h:8` define `THINGSBOARD_ENABLE_STREAM_UTILS 1`, y
la misma cabecera define `THINGSBOARD_BUFFER_SIZE 4096`. Esto es intencional:
permite mandar payloads mayores de 4096 bytes (p. ej. el snapshot PPG) vía
streaming en lugar de construir el JSON entero en memoria.

**Hallazgo verificado por compilación real** (no hipotético, no de
documentación de terceros leída por encima): compilar motherBoard
(`pio run -e IncuNest_V18`) contra `thingsboard-arduino-sdk#v0.15.0` +
`TBPubSubClient@2.12.1` falla en
`.pio/libdeps/IncuNest_V18/ThingsBoard/src/ThingsBoard.h:289`:

```
error: 'class IMQTT_Client' has no member named 'get_buffer_size'; did you mean 'set_buffer_size'?
        if (m_client.get_buffer_size() < json_size)  {
```

Causa raíz, confirmada leyendo el código fuente del SDK en GitHub:
`IMQTT_Client.h` en v0.15.0 partió el método único `get_buffer_size()` en
`get_receive_buffer_size()` / `get_send_buffer_size()`, para soportar los
buffers TX/RX separados que `TBPubSubClient>=2.10` introdujo. Pero
`ThingsBoard.h:289` se quedó llamando al nombre viejo — es una **inconsistencia
interna del propio SDK**, no un problema de nuestra configuración ni de la
versión elegida de `TBPubSubClient`.

Esa línea está dentro de `#if THINGSBOARD_ENABLE_STREAM_UTILS`, el flag que
motherBoard activa a propósito. Por tanto el bug solo se dispara cuando se
compila motherBoard (no Display_HMI, que lo tiene a `0`).

Alcance de la reproducción, para no tener que repetirla:
- Se reproduce igual contra el tag `v0.15.0` y contra `#master` actual del
  SDK — no está arreglado más adelante en el momento de esta investigación.
- `v0.14.0` sí compila (usa la API de un solo buffer de forma consistente),
  pero entonces subir no aporta nada frente a v0.13.0: mismo API relevante para
  motherBoard, sin el beneficio de v0.15.0 (fixes de placas Arduino).
- No se puede esquivar apagando `THINGSBOARD_ENABLE_STREAM_UTILS` en
  motherBoard sin perder la capacidad de streaming de payloads >4096 bytes.

Precedente de mantenimiento de fork ya existente en este ecosistema:
`medicalopenworld/incunest_afe4490` — un fork propio con un parche mínimo
mantenido por este mismo proyecto para otra dependencia. Es la referencia que
se usa en la Decisión D1 de abajo, no una decisión ya tomada.

## Goals / Non-Goals

**Goals**
- Dejar documentado el bloqueo técnico exacto, con evidencia de compilación,
  para que quien retome esto no tenga que reinvestigar desde cero.
- Forzar que la resolución del bug de terceros sea una decisión explícita y
  registrada (tarea 1 de `tasks.md`) antes de que exista ningún commit que
  toque `platformio.ini`.
- Dejar sentado que, si se implementa, la implementación no puede perder la
  capacidad de streaming de payloads grandes que `THINGSBOARD_ENABLE_STREAM_UTILS`
  habilita hoy.

**Non-Goals**
- No se implementa ninguna de las tres vías de resolución en este change.
- No se toca `feat/pin-lib-versions` ni el fix ya aplicado ahí para
  `TBPubSubClient@2.9.4` desaparecido del registro de PlatformIO — es un
  problema distinto, ya resuelto.
- No se decide en este documento cuál de las tres vías tomar — eso es
  explícitamente la primera tarea de implementación, no parte de la propuesta.

## Decisions

Este design.md **no fija** la decisión de arquitectura — la deja enmarcada
para que la tarea 1 de `tasks.md` la resuelva y la registre aquí (o en un ADR
de `Firmware/docs/adr/`) antes de tocar código. Las tres vías identificadas:

### Opción A — Reportar upstream y esperar
Abrir un issue en `thingsboard/thingsboard-arduino-sdk` señalando la
inconsistencia `get_buffer_size()` vs `get_receive_buffer_size()`/
`get_send_buffer_size()` dentro de `THINGSBOARD_ENABLE_STREAM_UTILS`, y esperar
a que se publique un patch release.

*A favor*: cero mantenimiento propio, cero riesgo de fork desincronizado.
*En contra*: sin fecha; motherBoard se queda en v0.13.0 indefinidamente, que ya
es aceptable (Non-Goal de este change no es "hay que subir sí o sí").

### Opción B — Fork parcheado propio (1-2 líneas)
Mantener un fork de `thingsboard-arduino-sdk` (patrón ya usado en
`medicalopenworld/incunest_afe4490`) que solo renombra la llamada de
`ThingsBoard.h:289` a `get_receive_buffer_size()` (o a la que corresponda según
qué buffer se esté comprobando — hay que leer el contexto exacto de esa
comprobación en `Send_Json_Common`/donde viva `json_size`, no asumir cuál de
los dos buffers aplica).

*A favor*: desbloquea la subida a v0.15.0 ya, sin esperar upstream.
*En contra*: mantenimiento propio de un fork — hay que re-verificar el parche
en cada futura versión del SDK, y `platformio.ini` pasaría a apuntar a una URL
git propia en vez del repo oficial. Solo-developer: coste asumible pero real.

### Opción C — Descartar la subida
Quedarse en v0.13.0 indefinidamente. El firmware funciona bien hoy; v0.15.0 no
resuelve ningún bug de producción documentado en `known_issues.md`, solo trae
fixes de placas Arduino que no están confirmados como aplicables a este
hardware (ESP32-S3).

*A favor*: cero riesgo, cero esfuerzo.
*En contra*: motherBoard se queda cada vez más lejos del SDK oficial; si algún
día aparece un fix de seguridad o un bug real que sí aplique, la distancia de
versiones hace más costoso adoptarlo.

## Risks / Trade-offs

- **[Riesgo] Elegir la Opción B (fork) sin identificar bien qué buffer aplica
  en `ThingsBoard.h:289`** → Mitigación: antes de escribir el parche, leer el
  contexto completo de esa comprobación (qué se envía, TX o RX) en el propio
  código del SDK v0.15.0, no asumir por el nombre.
- **[Riesgo] Bloquear indefinidamente por depender de un fix upstream (Opción
  A) sin fecha** → Mitigación: si se elige A, fijar una fecha de revisión (p.
  ej. próxima vez que se toque `platformio.ini` por otro motivo) para
  reevaluar, no dejarlo abierto sin más.
- **[Riesgo] Desalineación de versiones entre motherBoard y Display_HMI** si
  cada placa termina en un SDK distinto → Mitigación: la decisión de este
  change y la de `hmi-thingsboard-sdk-bump` deben tomarse juntas o al menos
  revisarse cruzadas antes de mergear cualquiera de las dos a `dev` (ver
  Impact de ambos `proposal.md`).

## Migration Plan

No aplica todavía: este change no modifica código ni `platformio.ini`. Cuando
se implemente (fuera de este proposal), el plan de migración deberá cubrir:
compilar `pio run -e IncuNest_V18`/`V17`/`V16` en verde con
`THINGSBOARD_ENABLE_STREAM_UTILS=1`, verificar en banco que el snapshot PPG
(>4096 bytes) sigue llegando íntegro por streaming, y decidir el rollback
(revertir a v0.13.0 pineado) si algo de eso falla.

## Open Questions

- ¿Cuál de las tres opciones (A/B/C) se elige? — Bloqueante para toda
  implementación futura; es la tarea 1 de `tasks.md`.
- Si se elige B (fork), ¿bajo qué cuenta/organización de GitHub se aloja,
  siguiendo el precedente de `medicalopenworld/incunest_afe4490`?
- ¿Hay que sincronizar la decisión con `hmi-thingsboard-sdk-bump` antes de
  implementar cualquiera de las dos, o pueden avanzar de forma independiente
  aceptando que las dos placas queden temporalmente en SDKs distintos?
