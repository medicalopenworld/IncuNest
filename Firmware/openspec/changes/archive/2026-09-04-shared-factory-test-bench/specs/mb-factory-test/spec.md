## REMOVED Requirements

### Requirement: Detección de la generación de hardware
**Reason**: el sondeo I2C2 sobre las líneas USB del SensorBoard devuelve ACK
falso y clasificaba como "equipo antiguo" una unidad con SensorBoard, así que
el test daba FAIL con hardware sano. Lo que importa en fábrica es que la
cabina tenga sensor, por el camino que sea.
**Migration**: sustituido por "Sensor de cabina por USB o por I2C2" (id 7,
`sensorboard`), que absorbe también el antiguo `SB_LINK`.

## ADDED Requirements

### Requirement: Sensor de cabina por USB o por I2C2

El test `SENSORBOARD` (id 7) SHALL dar PASS con `detail = usb` si
`sensorSourceGet() == SENSOR_SOURCE_SENSORBOARD`, `sensorboard_comm_connected()`,
`link_ok`, `env_seen` con `last_env_ms` de menos de 3 s y al menos una posición
de temperatura válida; SHALL dar PASS con `detail = i2c` si la fuente es I2C2 y
la temperatura de cabina escrita por `updateRoomSensor()` es finita, está en
`DIG_TEMP_ROOM_MIN..MAX` y tiene menos de 5 s; en otro caso, tras 10 s, FAIL con
`detail = usb sin datos` o `i2c sin datos`. Los tests `SB_STATUS`, `SB_ENV`,
`SB_DOOR`, `SB_LIGHT` y `SB_CAMERA` SHALL dar SKIP con `detail = sin usb` si el
camino que pasó no fue el USB.

#### Scenario: SensorBoard conectada con el sondeo I2C2 confundido
- **WHEN** el sondeo de arranque clasificó la unidad como I2C2 pero el
  SensorBoard está conectado
- **THEN** `SENSORBOARD` da FAIL con `detail = i2c sin datos` (no llega ninguna
  temperatura por I2C2), que señala el sondeo y no el SensorBoard
- *(Verificación manual en banco.)*

#### Scenario: Equipo antiguo con PCBA I2C2
- **WHEN** la fuente es I2C2 y los STS35/SHTC3 publican temperatura
- **THEN** `SENSORBOARD` da PASS con `detail = i2c` y los cinco `SB_*` dan SKIP
- *(Verificación manual en banco.)*

## MODIFIED Requirements

### Requirement: Tests de la SensorBoard por el enlace USB

`SB_STATUS` SHALL pedir `status` y exigir, en ≤ 5 s, las seis
disponibilidades (`sht0`, `sht1`, `sht2`, `als`, `door`, `cam`) a true,
informando `sb_fw` y `usb_swap` en `detail`; si no hay respuesta SHALL dar
FAIL con `detail = sin respuesta`. `SB_ENV` SHALL exigir las tres
temperaturas válidas y frescas, dispersión entre ellas ≤
`FTEST_SB_SPREAD_MAX_C` (1.0 °C) y diferencia entre su media y el SHT4x
exterior ≤ `FTEST_SB_VS_EXT_MAX_C` (3.0 °C). `SB_CAMERA` SHALL pedir una
captura y exigir un JPEG de ≥ 1000 bytes en ≤ 10 s, liberando el buffer. Los
cinco tests `SB_*` dependen de que `SENSORBOARD` haya pasado por USB.

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

### Requirement: Tests de comunicaciones pasivos

`GSM_AT` SHALL dar PASS si el módem ha respondido a algún comando AT
(`GPRS.modemResponded`, `GPRS.simReady`, `GPRS.connect` o `GPRS.post`) en ≤
45 s; `GSM_SIM` si `GPRS.simReady` (`+CPIN: READY`) en ≤ 15 s, con el CCID
solo informativo; `GSM_SIGNAL` si `1 <= GPRS.CSQ <= 31` en ≤ 15 s; `GSM_NET`
(opcional) PASS si `GPRS.post` en ≤ `FTEST_CONN_TIMEOUT_MS` (30 s), si no
WARN `sin red`. `WIFI` (opcional) SHALL dar PASS con RSSI si
`WIFIIsConnected()` en ≤ 30 s, si no WARN `sin AP`. `TB_PROVISION` (opcional)
SHALL dar WARN `sin serie` si `in3.serialNumber == 0`, PASS si
`WIFIIsConnectedToServer() || GPRSIsConnectedToServer()` en ≤ 30 s, y WARN
`sin servidor` en otro caso. `TIME` (opcional) SHALL dar PASS si
`time(nullptr) >= 1609459200` con la fuente en `detail`, si no WARN
`sin hora` a los 30 s. Ninguno de estos tests SHALL escribir en `Serial2`, ni
modificar credenciales WiFi, ni forzar reconexiones. `GPRSPowerUp()` SHALL
poner `modemResponded` al ver cualquier respuesta a `AT+CPIN?` y `simReady` al
ver `+CPIN: READY`; ambos se reinician con la estructura `GPRS`.

#### Scenario: Módem arrancado antes de pulsar el botón
- **WHEN** el módem completó su secuencia de arranque antes de `HMI,FTEST,START`
- **THEN** `GSM_AT` y `GSM_SIM` dan PASS de inmediato, sin esperar los 45 s
- *(Verificación manual en banco.)*

#### Scenario: Sin número de serie
- **WHEN** `in3.serialNumber == 0`
- **THEN** `TB_PROVISION` da WARN con `detail = sin serie` sin esperar los 30 s
- *(Verificación manual en banco.)*

#### Scenario: Sin SIM
- **WHEN** el módem responde pero `GPRS.simReady` sigue en false 15 s después
- **THEN** `GSM_SIM` da FAIL, y `GSM_SIGNAL` y `GSM_NET` dan SKIP con
  `detail = sin sim`
- *(Verificación manual en banco.)*

#### Scenario: Fábrica sin cobertura ni AP
- **WHEN** no hay AP `in3wifi` ni cobertura móvil
- **THEN** `GSM_NET`, `WIFI`, `TB_PROVISION` y `TIME` terminan en WARN a los
  30 s cada uno y `FTEST_DONE` los cuenta en el cuarto campo
- *(Verificación manual en banco.)*

### Requirement: Persistencia del resultado

Al emitir `FTEST_DONE` tras una batería completa, la motherBoard SHALL
guardar en NVS `mb_ftest`: `epoch` (0 si no hay hora), máscaras PASA, FALLA,
AVISO y EJECUTADO, `FWversion` y `sb_fw`. Un test único SHALL actualizar solo
los bits de su ID en las cuatro máscaras.

#### Scenario: Reintento de un test fallido
- **WHEN** la batería guardó el ID 12 en FALLA y luego `RUN,12` da PASS
- **THEN** en NVS el bit 12 pasa de FALLA a PASA y el resto de bits no cambia
- *(motherBoard `[env:native]` para la lógica de máscaras de `ftest_summary`;
  la escritura NVS, verificación manual.)*

#### Scenario: Aviso que pasa a correcto
- **WHEN** `WIFI` quedó en AVISO y un `RUN` posterior da PASS
- **THEN** el bit pasa de la máscara AVISO a la máscara PASA
- *(motherBoard `[env:native]`)*
