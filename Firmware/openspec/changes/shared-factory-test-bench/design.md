## Context

Segunda vuelta de `shared-factory-test` tras la prueba en banco (2026-09-04/05)
con motherBoard V18, SensorBoard y HMI. Los hallazgos están en `proposal.md`.
Dos son de diagnóstico, no de gusto:

- **`sensor_src` falso negativo.** `roomSensorI2CDetected()` marca presencia si
  cualquier `endTransmission()` sobre IO19/IO20 devuelve ACK. Con el SensorBoard
  conectado esas líneas son D+/D− y una de ellas puede estar retenida a nivel
  bajo: el bit de ACK se lee a 0 en todas las direcciones y la placa se
  clasifica como equipo antiguo. Es un problema del sondeo (fuera de este
  change); el test de fábrica no debe fallar por él si la cabina tiene sensor.
- **`gsm_at` con `GPRS.powerUp`.** Ese flag es verdadero solo mientras corre la
  secuencia de arranque del módem (`GPRSPowerUp()` lo baja en el `case 3`). Si
  el módem ya arrancó antes de pulsar el botón, el test esperaba 45 s y daba
  FAIL. Hace falta un flag de "el módem ha respondido" que no se apague.

## Goals / Non-Goals

**Goals**: que la batería en banco refleje el estado real de la unidad (sin
falsos FAIL por el sondeo I2C2 ni por el ciclo del módem); que la falta de
entorno (cobertura, AP, servidor, hora) se vea como aviso y no bloquee; que el
operario vea primero lo que falla, en un panel de 7″, y pueda consultar el
valor de cada test.

**Non-Goals**: arreglar el sondeo de `sensor_source` (rama propia); PIN de
servicio; tests de panel y toque (se retiran, no se reescriben).

## Decisions

### D1. Un test `sensorboard` que acepta cualquiera de los dos caminos

PASA "usb" si `sensorSourceGet()==SENSORBOARD`, el enlace está vivo y hay
`sensor_data` fresco con al menos una SHT40 válida; PASA "i2c" si la fuente
es I2C2 y `updateRoomSensor()` ha escrito una temperatura de cabina finita y
en rango en los últimos 5 s. FALLA con `usb sin datos` / `i2c sin datos`.
Los tests `sb_*` que necesitan `status`, lux, puerta, dBA o cámara siguen
exigiendo USB y hacen SKIP (oculto) en el camino I2C. Se elimina `sb_link`
(su contenido está dentro de `sensorboard`) y se renumeran los ids: el
protocolo `FTEST` no está en ninguna placa de campo.

### D2. Estado `WARN` final y cuarto campo en `FTEST_DONE`

`FTEST_WARN = 6` es final: cuenta en `ftest_summary` (contador y máscara
propios) y viaja como cuarto campo `w` de `CTRL,FTEST_DONE,p,f,s,w`. El parser
acepta 3 o 4 campos para no romper el eco de una placa que no lo mande. Lo
devuelven `gsm_net`, `wifi`, `tb_provision` y `time` al agotar
`FTEST_CONN_TIMEOUT_MS` (30 s), y `tb_provision` sin número de serie. Se
distingue de SKIP a propósito: SKIP es "no aplica" y se oculta; WARN es "no
pude comprobarlo" y el operario tiene que verlo.

### D3. Flags `modemResponded` y `simReady` en `GPRSstruct`

Se ponen en `GPRSPowerUp()` (respuesta a `AT+CPIN?`, sea `READY`, `SIM PIN` o
`ERROR`; y en el `case 3`). Se resetean solos con `GPRS = GPRSstruct()` cuando
el monitor recrea la tarea. `gsm_at` PASA con `modemResponded || simReady ||
connect || post`; `gsm_sim` PASA con `simReady`, y el CCID pasa a ser
informativo.

### D4. Cuadrícula de botones ordenada por severidad

Tres columnas en un contenedor con scroll; orden FALLA → AVISO → en curso →
PASA; SKIP fuera. Se re-ordena solo cuando cambia algún estado (evita el
patrón de repintado continuo de `known_issues.md` #2 aplicado a LVGL) y se
mantiene a la vista el test en curso. Tocar un botón abre un panel de detalle
(descripción de una línea, estado, valor, Reintentar si FALLA/AVISO). Todos
los callbacks siguen siendo hand-off por flag resueltos en `_Poll`.

### D5. Botón "HW test" centrado, sin área extendida y armado a 1,5 s

El arranque espontáneo del test en banco encaja con el toque fantasma que el
GT911 emite en una esquina al inicializarse: el botón estaba en la esquina
inferior izquierda con `TOUCH_EXT_MEDIUM`. Se centra abajo, se le quita el
área extendida, se ignora cualquier `CLICKED` en los primeros 1,5 s del
splash y se exige que el punto del indev caiga dentro del botón.

## Risks / Trade-offs

- [Renumerar ids rompe un HMI o MB con la versión anterior] → no hay ninguno
  en campo; se documenta en `PROTOCOL.md` v2.3.1.
- [El camino I2C del test `sensorboard` acepta como sano un sondeo falso] →
  no: exige una temperatura real y fresca, no solo el ACK. Si el sondeo
  clasifica mal y el USB nunca se levanta, el test FALLA con `i2c sin datos`,
  que es la señal correcta para la rama que arregle el sondeo.
- [WARN en ámbar puede leerse como "casi bien"] → el panel de detalle dice
  qué faltó (`sin AP`, `sin red`…) y el resumen los cuenta aparte.
- [El armado de 1,5 s puede comerse una pulsación legítima muy rápida] → el
  splash dura ≥ 5 s; el operario vuelve a pulsar.
- [`modemResponded` / `simReady` son pegajosos] → solo se reinician cuando
  `GPRSMonitorTask` recrea la tarea (`GPRS = GPRSstruct()`). Un módem que
  respondió una vez y luego enmudece sin llegar a "colgar" la tarea seguiría
  dando PASS en un `RUN` aislado de `gsm_at`/`gsm_sim`. Aceptado: en fábrica
  el test corre minutos después del arranque y lo que se comprueba es que el
  módem y la SIM existen y responden, no su disponibilidad continua (para eso
  está `commStatus` en `CTRL,STATE`).
- [La cuadrícula se reconstruía entera en cada repintado] → corregido tras el
  review: los botones se crean una vez por fila, se actualizan in-place y se
  reordenan con `lv_obj_move_to_index()` solo si cambió el orden; el panel de
  detalle se cierra solo si llega un WAIT/CONFIRM para que la pregunta Sí/No
  nunca quede tapada.
