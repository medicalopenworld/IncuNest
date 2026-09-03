## ADDED Requirements

### Requirement: La batería solo arranca con el control apagado

La motherBoard SHALL rechazar `HMI,FTEST,START` y `HMI,FTEST,RUN` con
`CTRL,FTEST_REJECT,1` si `in3.actuation != ACTUATION_OFF` o
`in3.phototherapy` está activa, y con `CTRL,FTEST_REJECT,0` si ya hay una
batería o un test único en curso. La comprobación SHALL hacerse antes de crear
la tarea de test.

#### Scenario: Control térmico activo
- **WHEN** llega `HMI,FTEST,START` con `in3.actuation = ACTUATION_TEMPERATURE`
- **THEN** la motherBoard responde `CTRL,FTEST_REJECT,1` y no toca ningún
  actuador
- *(Verificación manual en banco.)*

#### Scenario: Batería ya en curso
- **WHEN** llega un segundo `HMI,FTEST,START` mientras la tarea `FTEST` existe
- **THEN** la motherBoard responde `CTRL,FTEST_REJECT,0` y la batería en curso
  continúa sin alterarse
- *(Verificación manual en banco.)*

### Requirement: Estado seguro durante la batería y restauración incondicional

Antes del primer test la tarea SHALL: poner `in3.alarmsEnabled = false`,
levantar `g_factoryTestActive` (que hace que `PIDHandler()` y `turnFans()`
retornen sin escribir PWM), pasar los cuatro PIDs a MANUAL, escribir 0 en los
canales PWM de calefactor, fototerapia, ventilador y zumbador, apagar el
humidificador y poner `ACTUATORS_EN` en LOW. Al terminar, abortar o fallar,
SHALL restaurar `in3.alarmsEnabled` a su valor previo, bajar
`g_factoryTestActive` y dejar todos los PWM a 0 y `ACTUATORS_EN` LOW.

#### Scenario: Abort a mitad del test de actuadores
- **WHEN** llega `HMI,FTEST,ABORT` mientras corre `ACTUATORS`
- **THEN** el test en curso termina con `CTRL,FTEST,14,3,abort`, se emite
  `CTRL,FTEST_DONE`, todos los PWM quedan a 0, `ACTUATORS_EN` LOW,
  `in3.alarmsEnabled` vuelve a true y `PIDHandler()` vuelve a escribir PWM
- *(Verificación manual en banco.)*

#### Scenario: Ninguna alarma de actuador durante la batería
- **WHEN** la batería enciende el calefactor y la fototerapia en lazo abierto
- **THEN** no se declara `ALARM_HEATER_FAULT`, `ALARM_FAN_FAILURE` ni
  `ALARM_AIR_OUTLET_BLOCKED`, y `checkUsbFault()` no apaga el humidificador
- *(Verificación manual en banco.)*

### Requirement: Batería completa en orden fijo, un resultado por test

La tarea SHALL recorrer los `FTEST_MB_COUNT` tests en el orden de la tabla,
emitir `CTRL,FTEST,id,0` (RUNNING) al empezar cada uno y exactamente un
resultado final (`PASS`, `FAIL` o `SKIP`) al terminarlo, y cerrar con
`CTRL,FTEST_DONE,pass,fail,skip` cuyos contadores SHALL coincidir con la
secuencia emitida. `HMI,FTEST,RUN,id` SHALL ejecutar solo ese test dentro del
mismo estado seguro y cerrar igualmente con `FTEST_DONE`.

#### Scenario: Acumulación de resultados
- **WHEN** `ftest_summary` recibe la secuencia PASS, PASS, FAIL, SKIP, PASS
  para los IDs 0..4
- **THEN** los contadores son `pass = 3, fail = 1, skip = 1`, la máscara PASA
  vale `0b10011`, la máscara FALLA `0b00100` y la máscara EJECUTADO `0b11111`
- *(motherBoard `[env:native]`, `test/test_factory_test/`)*

#### Scenario: RUNNING, WAIT y CONFIRM no cuentan como resultado
- **WHEN** la secuencia para un ID es RUNNING, WAIT, WAIT, PASS
- **THEN** los contadores suman un único PASS y el ID aparece una vez en la
  máscara EJECUTADO
- *(motherBoard `[env:native]`)*

#### Scenario: Test único
- **WHEN** llega `HMI,FTEST,RUN,13` con el control apagado
- **THEN** solo se emiten `CTRL,FTEST,13,0`, un resultado final de 13 y
  `CTRL,FTEST_DONE` con la suma de ese único test
- *(Verificación manual en banco.)*

### Requirement: Detección de la generación de hardware

El test `SENSOR_SRC` SHALL dar PASS si `sensorSourceGet() ==
SENSOR_SOURCE_SENSORBOARD` y FAIL con `detail = i2c2 responde` en caso
contrario. Los tests `SB_*` SHALL terminar en SKIP si `SENSOR_SRC` falló.

#### Scenario: Pads de IO19/IO20 en corto
- **WHEN** el sondeo de arranque encontró un dispositivo en el bus I2C2 en una
  unidad de nueva generación
- **THEN** `SENSOR_SRC` da FAIL y `SB_LINK`, `SB_STATUS`, `SB_ENV`, `SB_DOOR`,
  `SB_LIGHT`, `SB_CAMERA` dan SKIP con `detail = sin sensorboard`
- *(Verificación manual en banco con una unidad forzada a modo I2C.)*

### Requirement: Tests de la SensorBoard por el enlace USB

`SB_LINK` SHALL exigir `sensorboard_comm_connected()` y `link_ok` en ≤ 10 s.
`SB_STATUS` SHALL pedir `status` y exigir, en ≤ 5 s, las seis
disponibilidades (`sht0`, `sht1`, `sht2`, `als`, `door`, `cam`) a true,
informando `sb_fw` y `usb_swap` en `detail`. `SB_ENV` SHALL exigir las tres
temperaturas válidas y frescas, dispersión entre ellas ≤
`FTEST_SB_SPREAD_MAX_C` (1.0 °C) y diferencia entre su media y el SHT4x
exterior ≤ `FTEST_SB_VS_EXT_MAX_C` (3.0 °C). `SB_CAMERA` SHALL pedir una
captura y exigir un JPEG de ≥ 1000 bytes en ≤ 10 s, liberando el buffer.

#### Scenario: Decodificar la respuesta `status`
- **WHEN** `sb_json_decode_status_resp()` recibe
  `{"type":"resp","cmd":"status","id":7,"status":"ok","fw":"1.0.0","sensors":{"sht0":true,"sht1":true,"sht2":false,"als":true,"door":true,"cam":true,"usb_swap":false}}`
- **THEN** devuelve true con `fw = "1.0.0"`, `avail_sht = {true,true,false}`,
  `avail_als/door/cam = true`, `usb_swap = false`
- *(motherBoard `[env:native]`, `test/test_sensorboard_json/`)*

#### Scenario: Respuesta `status` incompleta
- **WHEN** falta el objeto `sensors` o `status` no es `"ok"`
- **THEN** el decodificador devuelve false y el snapshot no cambia
- *(motherBoard `[env:native]`)*

#### Scenario: Una SHT40 mal soldada
- **WHEN** `status` devuelve `sht2:false`
- **THEN** `SB_STATUS` da FAIL con `detail` que nombra `sht2`
- *(Verificación manual en banco.)*

### Requirement: Tests con estímulo del operario

`SB_DOOR` SHALL emitir `CTRL,FTEST,11,4` y dar PASS si observa `door_open`
pasar a true y después a false dentro de `FTEST_STIMULUS_TIMEOUT_MS` (30 s);
si no, FAIL con `detail = timeout`. `SB_LIGHT` SHALL medir la base de lux
durante 2 s, emitir WAIT y dar PASS si el lux cae por debajo del 50 % de la
base dentro de 20 s; con base < 20 lux SHALL dar SKIP con `detail = poca luz`.

#### Scenario: Puerta abierta y cerrada
- **WHEN** el operario abre y cierra la puerta tras ver la instrucción
- **THEN** `SB_DOOR` da PASS y el siguiente test arranca de inmediato, sin
  esperar el resto del plazo
- *(Verificación manual en banco.)*

#### Scenario: Operario no actúa
- **WHEN** pasan 30 s sin transición de puerta
- **THEN** `SB_DOOR` da FAIL con `detail = timeout` y la batería continúa
- *(Verificación manual en banco.)*

### Requirement: Zumbador verificado con el micrófono, con confirmación de respaldo

`BUZZER` SHALL tomar la base de dBA del snapshot, activar el zumbador
(`ledcWrite(BUZZER_PWM_CHANNEL, BUZZER_HALF_PWM)`) hasta recibir una nueva
muestra de `sound_level` (≤ 7 s), apagarlo y dar PASS si la subida es ≥
`FTEST_BUZZER_DBA_DELTA` (6 dB). Si el snapshot no tiene `sound_seen`, SHALL
emitir `CTRL,FTEST,17,5`, hacer sonar el zumbador 500 ms y dar PASS o FAIL
según `HMI,FTEST,CONFIRM,17,ok`; sin respuesta en `FTEST_CONFIRM_TIMEOUT_MS`
(60 s), FAIL con `detail = timeout`.

#### Scenario: Con SensorBoard presente
- **WHEN** hay `sound_seen` y el zumbador suena
- **THEN** `BUZZER` da PASS con `detail` con la base y el pico en dBA, sin
  preguntar al operario
- *(Verificación manual en banco.)*

#### Scenario: CONFIRM con id distinto
- **WHEN** la tarea espera `CONFIRM,17` y llega `HMI,FTEST,CONFIRM,11,1`
- **THEN** se descarta con log y la espera de 17 continúa
- *(Verificación manual en banco.)*

### Requirement: Tests de actuadores y humidificador reutilizan el autotest

`ACTUATORS` SHALL invocar `actuatorsTest()` y dar FAIL si aparece en
`HW_error` cualquier bit nuevo de calefactor, fototerapia o ventilador
respecto al valor previo a la llamada. `FAN_RPM` SHALL exigir
`in3.fanHasSpeedFeedback && in3.fan_rpm >= FAN_MIN_RPM` tras `ACTUATORS`.
`HUMID_USB` SHALL poner `USB_EN` en HIGH, esperar `USB_FAULT_SETTLE_MS`,
exigir `GPIORead(USB_FAULT)` true y corriente en `USB_SHUNT_CHANNEL` >
`FTEST_HUMID_MIN_MA` (20 mA), y volver a LOW siempre. `STANDBY` SHALL invocar
`testStandByCurrent()` con el mismo criterio de bits nuevos.

#### Scenario: Ventilador sin tacómetro
- **WHEN** `ACTUATORS` pasa pero `in3.fanHasSpeedFeedback` queda false
- **THEN** `FAN_RPM` da FAIL con `detail = sin feedback`
- *(Verificación manual en banco.)*

### Requirement: Tests de comunicaciones pasivos

`GSM_AT` SHALL dar PASS si `GPRS.powerUp` es true en ≤ 45 s; `GSM_SIM` si
`GPRS.CCID` no está vacío en ≤ 15 s; `GSM_SIGNAL` si `1 <= GPRS.CSQ <= 31` en
≤ 15 s; `GSM_NET` (opcional) si `GPRS.post` en ≤ 60 s, si no SKIP. `WIFI`
(opcional) SHALL dar PASS con RSSI si `WIFIIsConnected()` en ≤ 30 s, si no
SKIP con `detail = sin AP`. `TB_PROVISION` (opcional) SHALL dar SKIP si
`in3.serialNumber == 0`, PASS si `WIFIIsConnectedToServer() ||
GPRSIsConnectedToServer()` en ≤ 30 s, SKIP si ningún transporte está
conectado, y FAIL si hay transporte pero no servidor. `TIME` (opcional) SHALL
dar PASS si `time(nullptr) >= 1609459200` con la fuente en `detail`, si no
SKIP. Ninguno de estos tests SHALL escribir en `Serial2`, ni modificar
credenciales WiFi, ni forzar reconexiones.

#### Scenario: Sin número de serie
- **WHEN** `in3.serialNumber == 0`
- **THEN** `TB_PROVISION` da SKIP con `detail = sin serie` sin esperar los 30 s
- *(Verificación manual en banco.)*

#### Scenario: Sin SIM
- **WHEN** el módem responde pero `GPRS.CCID` sigue vacío 15 s después
- **THEN** `GSM_SIM` da FAIL, y `GSM_SIGNAL` y `GSM_NET` dan SKIP con
  `detail = sin sim`
- *(Verificación manual en banco.)*

### Requirement: Persistencia del resultado

Al emitir `FTEST_DONE` tras una batería completa, la motherBoard SHALL
guardar en NVS `mb_ftest`: `epoch` (0 si no hay hora), máscaras PASA, FALLA y
EJECUTADO, `FWversion` y `sb_fw`. Un test único SHALL actualizar solo los bits
de su ID en las tres máscaras.

#### Scenario: Reintento de un test fallido
- **WHEN** la batería guardó el ID 13 en FALLA y luego `RUN,13` da PASS
- **THEN** en NVS el bit 13 pasa de FALLA a PASA y el resto de bits no cambia
- *(motherBoard `[env:native]` para la lógica de máscaras de `ftest_summary`;
  la escritura NVS, verificación manual.)*

### Requirement: El TX hacia el display se serializa en una cola

Toda línea `CTRL,FTEST*` SHALL entregarse con `CommunicationHost_Enqueue()`,
una cola de `FTEST_TX_QUEUE_LEN` (16) líneas de `FTEST_TX_LINE_MAX` (64)
bytes que solo `Communication_Task` drena y escribe en `hmiSerial`. Con la
cola llena la línea SHALL descartarse con log de error, sin bloquear.

#### Scenario: Resultados durante la traza PPG
- **WHEN** la batería emite resultados mientras `CTRL,PPG` sale a 25 Hz
- **THEN** ninguna línea llega al display entrelazada con otra
- *(Verificación manual en banco con la sonda SpO2 aplicada.)*
