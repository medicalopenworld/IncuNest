## 1. Decisión de arquitectura (bloqueante — nada de lo siguiente empieza sin esto)

Commit: `docs(adr): decidir resolución del bug get_buffer_size en thingsboard-arduino-sdk`.

- [ ] 1.1 Releer el código fuente exacto de `ThingsBoard.h:289` y de
      `IMQTT_Client.h` en la versión objetivo (v0.15.0 o `#master` si ha
      cambiado desde esta investigación) para confirmar si sigue reproduciéndose
      el error y si el contexto de esa comprobación (TX o RX) sigue siendo el
      mismo.
- [ ] 1.2 Comprobar si ya existe un issue abierto en
      `thingsboard/thingsboard-arduino-sdk` sobre esta inconsistencia; si no,
      valorar abrirlo (Opción A de `design.md`).
- [ ] 1.3 Decidir entre las tres opciones de `design.md` (A: reportar y
      esperar, B: fork parcheado propio, C: descartar) y registrar la decisión
      con su razonamiento en `design.md` (sección Decisions) o en un nuevo ADR
      de `Firmware/docs/adr/`.
- [ ] 1.4 Si la decisión es **C (descartar)**: cerrar este change sin
      implementar nada más, dejando el registro de la decisión como el
      artefacto final. Las tareas 2-4 no aplican.
- [ ] 1.5 Si la decisión es **A (esperar upstream)**: fijar una fecha o evento
      de revisión explícito (p. ej. "revisar la próxima vez que se toque
      `platformio.ini` de motherBoard por otro motivo") y dejarlo anotado en
      `design.md`. Las tareas 2-4 quedan pendientes hasta que se resuelva
      upstream.
- [ ] 1.6 Si la decisión es **B (fork parcheado)**: continuar con la tarea 2.

## 2. Fork parcheado (solo si la decisión de la tarea 1 es la Opción B)

Commit: `chore(motherboard): fork parcheado de thingsboard-arduino-sdk`.

- [ ] 2.1 Crear el fork bajo la organización/cuenta de GitHub que se decida
      (ver Open Questions de `design.md`), siguiendo el precedente de
      `medicalopenworld/incunest_afe4490`.
- [ ] 2.2 Aplicar el parche mínimo: renombrar la llamada de
      `ThingsBoard.h:289` al método correcto (`get_receive_buffer_size()` o
      `get_send_buffer_size()` según el contexto confirmado en la tarea 1.1),
      sin tocar nada más del SDK.
- [ ] 2.3 Documentar en el propio fork (README o comentario en el diff) qué
      commit/versión del SDK oficial se parcheó y por qué, para poder
      reaplicar el parche si se sube de versión otra vez en el futuro.

## 3. Subida de versión (solo si la tarea 1 concluye en A-resuelto o B-completado)

Commit: `chore(motherboard): subir thingsboard-arduino-sdk a v0.15.0 y TBPubSubClient a v2.12.1`.

- [ ] 3.1 Actualizar `motherBoard/platformio.ini`: `thingsboard-arduino-sdk` a
      v0.15.0 (o al fork parcheado de la tarea 2) y `TBPubSubClient` a
      v2.12.1.
- [ ] 3.2 `pio run -e IncuNest_V18` en verde, sin errores de
      `get_buffer_size`/`get_receive_buffer_size`/`get_send_buffer_size`.
- [ ] 3.3 `pio run -e IncuNest_V17` y `-e IncuNest_V16` en verde (misma lista
      de `lib_deps` heredada vía `extends = common`).
- [ ] 3.4 `pio test -e native` sigue pasando sin regresión — el bump de
      versión no debería afectar a `[env:native]` (no compila el SDK real),
      pero confirmar que nada rompe por transitividad.

## 4. Verificación en hardware (manual — obligatoria, no hay entorno de test)

- [ ] 4.1 **Verificación manual en hardware real** (documentar qué se probó y
      con qué resultado): enviar un snapshot PPG (>4096 bytes) por streaming y
      confirmar que ThingsBoard lo recibe íntegro, comparando con el
      comportamiento de v0.13.0.
- [ ] 4.2 **Verificación manual en hardware real**: comparar el payload de
      telemetría recurrente recibido en ThingsBoard antes y después de la
      subida, confirmando que no faltan campos.
- [ ] 4.3 Actualizar `Firmware/docs/known_issues.md` si la investigación de
      este change revela algún matiz nuevo del bug que no estuviera ya
      cubierto (no se espera, pero confirmar).

## 5. Cierre

- [ ] 5.1 `openspec archive mb-thingsboard-sdk-bump` una vez implementado y
      verificado (o cerrado sin implementar, si la decisión de la tarea 1 fue
      C).
- [ ] 5.2 Revisar si `hmi-thingsboard-sdk-bump` necesita actualizarse a la luz
      de la decisión tomada aquí (coherencia de versiones entre placas).
