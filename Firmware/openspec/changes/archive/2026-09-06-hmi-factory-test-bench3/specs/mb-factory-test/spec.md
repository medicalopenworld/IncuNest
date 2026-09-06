## MODIFIED Requirements

### Requirement: Tests con estímulo del operario

`SB_DOOR` SHALL devolver SKIP con `detail = omitido` sin emitir WAIT mientras el
jig de fábrica no monte la puerta (pendiente de reactivar). `SB_LIGHT` SHALL
medir la base de lux durante 2 s, emitir WAIT y dar PASS si el lux cae por
debajo del 50 % de la base dentro de 20 s; con base < 20 lux SHALL dar SKIP con
`detail = poca luz`.

#### Scenario: Puerta omitida
- **WHEN** la batería llega a `SB_DOOR`
- **THEN** emite SKIP `omitido` de inmediato, sin instrucción al operario
- *(Verificación manual en banco.)*

#### Scenario: Operario no tapa el sensor de luz
- **WHEN** pasan 20 s sin caída de lux tras el WAIT de `SB_LIGHT`
- **THEN** `SB_LIGHT` da FAIL con `detail = timeout` y la batería continúa
- *(Verificación manual en banco.)*

### Requirement: Zumbador verificado con el micrófono, con confirmación de respaldo

`BUZZER` SHALL tomar la base de dBA del snapshot, activar el zumbador
(`ledcWrite(BUZZER_PWM_CHANNEL, BUZZER_HALF_PWM)`) hasta recibir una nueva
muestra de `sound_level` (≤ 7 s), apagarlo y dar PASS si la subida es ≥
`FTEST_BUZZER_DBA_DELTA` (6 dB). Si el snapshot no tiene `sound_seen`, SHALL
dar SKIP con `detail = sin microfono` sin hacer sonar el zumbador ni preguntar
al operario: el test de fábrica no hace preguntas de audio.

#### Scenario: Con SensorBoard con micrófono
- **WHEN** hay `sound_seen` y el zumbador suena
- **THEN** `BUZZER` da PASS con `detail` con la base y el pico en dBA
- *(Verificación manual en banco.)*

#### Scenario: Sin micrófono
- **WHEN** el snapshot no tiene `sound_seen`
- **THEN** `BUZZER` da SKIP `sin microfono` y no se emite ningún CONFIRM
- *(Verificación manual en banco.)*

### Requirement: Los tests no compiten por el bus I2C

Ningún cuerpo de test SHALL emitir transacciones I2C propias, salvo dentro de
`actuatorsTest()` y `testStandByCurrent()`. `CHARGER` SHALL esperar ≤ 12 s a
`g_bq_status_valid` con sello `g_bq_status_ms` fresco (que refresca
`sensors_Task` cada 5 s) y dar PASS si el BQ25730 respondió, informando VBAT,
VSYS y `ac`/`bat` en `detail`; si no responde SHALL dar WARN `sin vbus`: sin
alimentación externa el BQ25730 no está alimentado y no es un fallo de placa.
`POWER_SRC` SHALL devolver SKIP `omitido` mientras el jig no alimente por
VBUS. `SKIN_ADC` SHALL usar `skinProbeLastReading()` y el sello
`lastSuccesfullSensorUpdate[SKIN_SENSOR]`; `INA3221` SHALL usar los flags de
presencia. El runner SHALL esperar 60 ms entre el resultado final de un test y
el RUNNING del siguiente para no desbordar el anillo del display.

#### Scenario: Placa alimentada solo con batería
- **WHEN** no hay VBUS y el BQ25730 no responde
- **THEN** `CHARGER` da WARN `sin vbus` en ≤ 12 s y `POWER_SRC` da SKIP
  `omitido`; ningún test queda en RUNNING
- *(Verificación manual en banco.)*

#### Scenario: Ráfaga de tests omitidos
- **WHEN** varios tests consecutivos terminan en SKIP de inmediato
- **THEN** entre cada resultado y el RUNNING siguiente pasan al menos 60 ms
- *(Verificación manual en banco con el monitor serie.)*
