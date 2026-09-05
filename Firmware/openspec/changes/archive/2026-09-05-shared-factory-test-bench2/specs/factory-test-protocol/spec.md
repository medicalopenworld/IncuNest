## MODIFIED Requirements

### Requirement: Tabla única de identificadores y estados de test

La tabla de tests SHALL definirse una sola vez en `shared/include/factory_test.h`:
identificadores (`FtestId`), estados (`FtestStatus`) y motivos de rechazo
(`FtestReject`), compilados en ambas placas. Los tests de la
motherBoard SHALL ocupar `0..FTEST_MB_COUNT-1` (28, ≤ 32 para caber en una
máscara de 32 bits); los tests locales del display SHALL empezar en
`FTEST_HMI_BASE` (64) y no viajar nunca por el enlace. El id 6 es
`env_sensor` (sensor ambiental por USB, I2C2 o SHT4x exterior); no existen
tests separados de enlace ni de sensor exterior.

Cada identificador SHALL tener un flag `optional`. Un test opcional que no
encuentra su entorno (AP WiFi, cobertura, señal, servidor, hora de red)
termina en WARN; SKIP queda para los tests que no aplican en cascada o que
están omitidos por ahora.

#### Scenario: Consulta de la tabla
- **WHEN** se consulta `ftest_id_is_mb(id)`, `ftest_id_is_optional(id)` y
  `ftest_id_key(id)` para cada `id` de la tabla
- **THEN** los IDs `0..27` son de motherBoard y los `>= FTEST_HMI_BASE` no lo son
- **AND** `GSM_NET`, `WIFI`, `TB_PROVISION`, `TIME` y `AFE_PROBE` son opcionales
  y `ACTUATORS`, `ENV_SENSOR`, `SB_STATUS` no lo son
- **AND** `ftest_id_key(FTEST_MB_ENV_SENSOR)` es `"env_sensor"` y ninguna
  clave está vacía ni supera 12 caracteres
- *(motherBoard `[env:native]`, `test/test_factory_test/`)*

#### Scenario: Identificador fuera de tabla
- **WHEN** se consulta un `id` que no está en la tabla (por ejemplo 28, 63 o 200)
- **THEN** `ftest_id_is_mb()` devuelve false y `ftest_id_key()` devuelve la
  cadena `"?"`
- *(motherBoard `[env:native]`)*
