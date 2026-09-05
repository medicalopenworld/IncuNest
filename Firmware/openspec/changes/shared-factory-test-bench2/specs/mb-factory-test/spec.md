## REMOVED Requirements

### Requirement: Sensor de cabina por USB o por I2C2
**Reason**: el hardware monta SensorBoard **o** sensor ambiental SHT4x, nunca
ambos; el test de cabina y el de exterior fallaban por separado en unidades
sanas.
**Migration**: sustituido por "Sensor ambiental por cualquiera de sus tres
caminos" (id 6, `env_sensor`), que absorbe `sensorboard` y `ext_sht4x`.

## ADDED Requirements

### Requirement: Sensor ambiental por cualquiera de sus tres caminos

El test `ENV_SENSOR` (id 6) SHALL dar PASS si, en ≤ 10 s, cualquiera de estos
caminos entrega una lectura fresca (< 5 s; < 3 s para USB), finita y en rango:
`usb` (`sensorSourceGet() == SENSOR_SOURCE_SENSORBOARD`, enlace vivo,
`env_seen`, alguna temperatura válida), `i2c` (fuente I2C2 y
`in3.temperature[ROOM_DIGITAL_TEMP_SENSOR]` en `DIG_TEMP_ROOM_MIN..MAX`) o
`sht4x` (`ambientSensorPresent` e `in3.temperature[AMBIENT_DIGITAL_TEMP_SENSOR]`
en −10..60 °C). El camino va en `detail`. En otro caso FAIL con
`detail = sin sensor ambiental`. La cascada `sb_usb` SHALL marcarse OK solo
si el camino fue `usb`; `SB_STATUS`, `SB_ENV`, `SB_DOOR`, `SB_LIGHT` y
`SB_CAMERA` SHALL dar SKIP con `detail = sin usb` en otro caso. `SB_ENV` SHALL
comparar con el exterior solo si hay SHT4x con lectura fresca.

#### Scenario: Unidad con SensorBoard y sin SHT4x
- **WHEN** la SensorBoard publica `sensor_data` y no hay SHT4x exterior
- **THEN** `ENV_SENSOR` da PASS `usb` y `SB_ENV` evalúa solo la dispersión
  entre las tres SHT40
- *(Verificación manual en banco.)*

#### Scenario: Unidad antigua con SHT4x
- **WHEN** no hay SensorBoard, la fuente es I2C2 sin datos y el SHT4x lee
- **THEN** `ENV_SENSOR` da PASS `sht4x` y los cinco `SB_*` dan SKIP
- *(Verificación manual en banco.)*

### Requirement: Los tests no compiten por el bus I2C

Ningún cuerpo de test SHALL emitir transacciones I2C propias, salvo dentro de
`actuatorsTest()` y `testStandByCurrent()`. `CHARGER` SHALL esperar ≤ 12 s a
`g_bq_status_valid` (que refresca `PowerManagement_Task` cada 5 s) y dar PASS
si el BQ25730 respondió, con o sin alimentación externa, informando VBAT, VSYS
y `ac`/`bat` en `detail`; si no, FAIL `sin respuesta`. `SKIN_ADC` SHALL usar
`skinProbeLastReading()` y el sello `lastSuccesfullSensorUpdate[SKIN_SENSOR]`;
`INA3221` SHALL usar los flags de presencia.

#### Scenario: Placa alimentada solo con batería
- **WHEN** no hay VBUS y `PowerManagement_Task` refresca `g_bq_status`
- **THEN** `CHARGER` da PASS con `detail` que termina en `bat` en menos de 12 s
  y nunca se queda en RUNNING
- *(Verificación manual en banco.)*

### Requirement: Timeout cooperativo por test

El runner SHALL registrar el inicio de cada test y, si supera
`FTEST_TEST_TIMEOUT_MS` (90 s), el cuerpo SHALL terminar en su siguiente paso
de espera con FAIL y `detail = timeout`; la batería SHALL continuar con el
test siguiente. Un cuerpo bloqueado en una llamada no cooperativa solo lo
cubre el task WDT.

#### Scenario: Test que no progresa
- **WHEN** un cuerpo lleva 90 s en sus bucles de espera sin resultado
- **THEN** emite `CTRL,FTEST,<id>,2,timeout` y arranca el siguiente test
- *(Verificación manual en banco forzando un plazo corto.)*

## MODIFIED Requirements

### Requirement: Tests de actuadores y humidificador reutilizan el autotest

`ACTUATORS` SHALL invocar `actuatorsTest()` y dar FAIL si aparece en
`HW_error` cualquier bit nuevo de calefactor, fototerapia o ventilador
respecto al valor previo a la llamada. `FAN_RPM` SHALL exigir
`in3.fanHasSpeedFeedback && in3.fan_rpm >= FAN_MIN_RPM` tras `ACTUATORS`.
`HUMID_USB` SHALL devolver SKIP con `detail = omitido` sin tocar `USB_EN`
mientras el jig de fábrica no pueda medir el humidificador. `STANDBY` SHALL
invocar `testStandByCurrent()` con el mismo criterio de bits nuevos.

#### Scenario: Ventilador sin tacómetro
- **WHEN** `ACTUATORS` pasa pero `in3.fanHasSpeedFeedback` queda false
- **THEN** `FAN_RPM` da FAIL con `detail = sin feedback`
- *(Verificación manual en banco.)*

#### Scenario: Humidificador omitido
- **WHEN** la batería llega a `HUMID_USB`
- **THEN** emite SKIP `omitido` de inmediato y `USB_EN` sigue en LOW
- *(Verificación manual en banco.)*

### Requirement: Tests de comunicaciones pasivos

`GSM_AT` SHALL dar PASS si el módem ha respondido a algún comando AT
(`GPRS.modemResponded`, `GPRS.simReady`, `GPRS.connect` o `GPRS.post`) en ≤
45 s; `GSM_SIM` si `GPRS.simReady` (`+CPIN: READY`) en ≤ 15 s, con el CCID
solo informativo; `GSM_SIGNAL` (opcional) PASS si `1 <= GPRS.CSQ <= 31` en ≤
15 s, si no WARN `sin señal`; `GSM_NET` (opcional) PASS si `GPRS.post` en ≤
`FTEST_CONN_TIMEOUT_MS` (30 s), si no WARN `sin red`. `WIFI` (opcional) SHALL
dar PASS con RSSI si `WIFIIsConnected()` en ≤ 30 s, si no WARN `sin AP`.
`TB_PROVISION` (opcional) SHALL dar WARN `sin serie` si
`in3.serialNumber == 0`, PASS si `WIFIIsConnectedToServer() ||
GPRSIsConnectedToServer()` en ≤ 30 s, y WARN `sin servidor` en otro caso.
`TIME` (opcional) SHALL dar PASS si `time(nullptr) >= 1609459200` con la
fuente en `detail`, si no WARN `sin hora` a los 30 s. Ninguno de estos tests
SHALL escribir en `Serial2`, ni modificar credenciales WiFi, ni forzar
reconexiones. Solo `GSM_AT` y `GSM_SIM` pueden dar FAIL: conectarse es
opcional.

#### Scenario: Módem arrancado antes de pulsar el botón
- **WHEN** el módem completó su secuencia de arranque antes de `HMI,FTEST,START`
- **THEN** `GSM_AT` y `GSM_SIM` dan PASS de inmediato, sin esperar los 45 s
- *(Verificación manual en banco.)*

#### Scenario: Sin cobertura
- **WHEN** hay SIM pero `GPRS.CSQ` no es válido en 15 s
- **THEN** `GSM_SIGNAL` da WARN `sin señal` y `GSM_NET` WARN `sin red`; ningún
  test GSM queda en rojo
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
