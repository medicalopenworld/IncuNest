## ADDED Requirements

### Requirement: La resolución del bug de terceros queda decidida y registrada antes de tocar código

El proyecto SHALL tener registrada una decisión explícita (en `design.md` de
este change o en un ADR de `Firmware/docs/adr/`) entre las tres vías
identificadas — reportar upstream y esperar, mantener un fork parcheado
propio, o descartar la subida — antes de que exista ningún commit que
modifique `motherBoard/platformio.ini` para subir de versión
`thingsboard-arduino-sdk`/`TBPubSubClient`.

Esto no es negociable porque el bug de `IMQTT_Client::get_buffer_size()` no es
arreglable eligiendo otra versión de `TBPubSubClient` ni de
`thingsboard-arduino-sdk` distinta de v0.14.0 (que no aporta el beneficio
buscado) — cualquier commit que suba a v0.15.0 o superior sin esta decisión
tomada estaría reintroduciendo un fallo de compilación ya conocido.

#### Scenario: Intento de subir de versión sin decisión registrada
- **WHEN** alguien propone un commit que cambia
  `thingsboard-arduino-sdk`/`TBPubSubClient` en `motherBoard/platformio.ini`
- **AND** no existe una decisión registrada en `design.md` o en un ADR sobre
  cómo se resuelve el bug de `get_buffer_size()`
- **THEN** el commit se rechaza en revisión, con independencia de que compile
  o no en ese momento (podría compilar por casualidad contra v0.14.0, que no
  es el objetivo)
- *(Verificación manual: no hay forma automatizada de comprobar la existencia
  de una decisión documentada; es una revisión de proceso, no de código.)*

### Requirement: motherBoard compila con streaming de payloads grandes habilitado en la versión objetivo

Si se decide subir de versión, motherBoard SHALL seguir compilando con
`THINGSBOARD_ENABLE_STREAM_UTILS=1` y `THINGSBOARD_BUFFER_SIZE=4096` (o el
valor vigente en `motherBoard/include/main.h` en ese momento) sin errores de
compilación, en los tres entornos de hardware (`IncuNest_V16`, `V17`, `V18`).

Esto es la restricción central de este change: no vale ninguna resolución del
bug de terceros que obligue a apagar `THINGSBOARD_ENABLE_STREAM_UTILS`, porque
esa capacidad es la que permite mandar el snapshot PPG (>4096 bytes) por
streaming en vez de construir el JSON completo en memoria.

#### Scenario: Build con stream utils habilitado en la versión objetivo
- **WHEN** se compila `pio run -e IncuNest_V18` (y `V17`, `V16`) contra la
  versión objetivo de `thingsboard-arduino-sdk`/`TBPubSubClient`, con
  `THINGSBOARD_ENABLE_STREAM_UTILS=1` definido como hoy
- **THEN** la compilación termina sin errores relacionados con
  `IMQTT_Client`/`get_buffer_size`/`get_receive_buffer_size`/`get_send_buffer_size`
- *(Verificación manual/compilación: motherBoard no tiene un entorno de test
  que compile código Arduino real; `[env:native]` no aplica aquí. Se verifica
  con `pio run`, no con `pio test -e native`.)*

#### Scenario: El snapshot PPG sigue llegando íntegro por streaming
- **WHEN** motherBoard, ya en la versión objetivo del SDK, envía un payload de
  telemetría mayor de 4096 bytes (p. ej. el snapshot PPG) por streaming
- **THEN** ThingsBoard recibe el payload completo, sin truncar, igual que en
  v0.13.0
- *(Verificación manual en banco, con conectividad real a ThingsBoard: no hay
  forma de probar esto en `[env:native]`, requiere hardware y backend reales.)*

### Requirement: No hay regresión en la publicación de telemetría existente

La subida de versión, si se implementa, SHALL preservar el comportamiento
observable actual de publicación de telemetría de motherBoard (los campos y
la cadencia que ya publica hoy contra v0.13.0) — el objetivo de este change es
únicamente actualizar la dependencia, no cambiar qué se publica.

#### Scenario: Los campos de telemetría existentes no cambian
- **WHEN** motherBoard publica telemetría a ThingsBoard tras la subida de
  versión
- **THEN** los mismos campos que publica hoy contra v0.13.0 siguen presentes,
  con los mismos nombres y tipos
- *(Verificación manual en banco, comparando el payload recibido en
  ThingsBoard antes y después de la subida.)*
