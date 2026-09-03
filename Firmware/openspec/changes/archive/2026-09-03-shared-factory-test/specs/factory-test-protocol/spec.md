## ADDED Requirements

### Requirement: Tabla única de identificadores y estados de test

La tabla de tests SHALL definirse una sola vez en `shared/include/factory_test.h`:
identificadores (`FtestId`), estados (`FtestStatus`) y motivos de rechazo
(`FtestReject`), compilados en ambas placas. Los tests de la
motherBoard SHALL ocupar `0..FTEST_MB_COUNT-1` (≤ 32, para caber en una máscara
de 32 bits); los tests locales del display SHALL empezar en `FTEST_HMI_BASE`
(64) y no viajar nunca por el enlace.

Cada identificador SHALL tener un flag `optional`. Un test opcional que no
encuentra su entorno (AP WiFi, SIM, sonda, hora de red) termina en SKIP, nunca
en FAIL.

#### Scenario: Consulta de la tabla
- **WHEN** se consulta `ftest_id_is_mb(id)`, `ftest_id_is_optional(id)` y
  `ftest_id_key(id)` para cada `id` de la tabla
- **THEN** los IDs `0..FTEST_MB_COUNT-1` son de motherBoard y los
  `>= FTEST_HMI_BASE` no lo son
- **AND** `GSM_NET`, `WIFI`, `TB_PROVISION`, `TIME` y `AFE_PROBE` son opcionales
  y `ACTUATORS`, `SENSOR_SRC`, `SB_LINK` no lo son
- **AND** ninguna clave está vacía ni supera 12 caracteres
- *(motherBoard `[env:native]`, `test/test_factory_test/`)*

#### Scenario: Identificador fuera de tabla
- **WHEN** se consulta un `id` que no está en la tabla (por ejemplo 63 o 200)
- **THEN** `ftest_id_is_mb()` devuelve false y `ftest_id_key()` devuelve la
  cadena `"?"`
- *(motherBoard `[env:native]`)*

### Requirement: Línea de resultado `CTRL,FTEST`

La motherBoard SHALL informar cada cambio de estado de un test con una línea
`CTRL,FTEST,<id>,<status>,<detail>` donde `status` ∈ `{0 RUNNING, 1 PASS,
2 FAIL, 3 SKIP, 4 WAIT, 5 CONFIRM}` y `detail` es el último campo, de longitud
≤ `FTEST_DETAIL_MAX` (40), sin comas ni saltos de línea. El codificador SHALL
sanear el `detail` sustituyendo `,`, `\r` y `\n` por `;` y truncando.

El receptor SHALL descartar la línea entera si faltan campos, si `id` o
`status` no son numéricos o están fuera de la tabla. `detail` vacío es válido.

#### Scenario: Codificar un resultado
- **WHEN** se llama a `ftest_format_result(buf, n, FTEST_MB_SB_ENV, FTEST_PASS,
  "36.1/36.2/36.0")`
- **THEN** `buf` contiene exactamente `CTRL,FTEST,10,1,36.1/36.2/36.0\n`
- *(motherBoard `[env:native]`)*

#### Scenario: Detail con comas y demasiado largo
- **WHEN** se codifica un `detail` con comas y 60 caracteres
- **THEN** las comas aparecen como `;` en la línea y el campo queda truncado a
  `FTEST_DETAIL_MAX` caracteres
- **AND** la línea completa sigue cabiendo en `FTEST_TX_LINE_MAX` (64) bytes
- *(motherBoard `[env:native]`)*

#### Scenario: Parsear un resultado válido
- **WHEN** el display recibe `CTRL,FTEST,11,4,\n`
- **THEN** `ftest_parse_result()` devuelve true con `id = 11`, `status = WAIT`
  y `detail` vacío
- *(motherBoard `[env:native]`)*

#### Scenario: Descarte de línea malformada
- **WHEN** se parsea `CTRL,FTEST,11` (faltan campos), `CTRL,FTEST,x,1,` (id no
  numérico), `CTRL,FTEST,11,9,` (status fuera de rango) o `CTRL,FTEST,99,1,`
  (id fuera de tabla)
- **THEN** `ftest_parse_result()` devuelve false en los cuatro casos y no
  modifica la estructura de salida
- *(motherBoard `[env:native]`)*

### Requirement: Cierre y rechazo de la batería

Al terminar la batería (o el test único) la motherBoard SHALL emitir
`CTRL,FTEST_DONE,<pass>,<fail>,<skip>`. Si no puede arrancarla SHALL emitir
`CTRL,FTEST_REJECT,<reason>` con `reason` ∈ `{0 BUSY, 1 CONTROL_ACTIVE,
2 UNKNOWN_ID}`.

#### Scenario: Codificar y parsear el cierre
- **WHEN** se codifica `ftest_format_done(buf, n, 25, 2, 3)` y se parsea el
  resultado
- **THEN** la línea es `CTRL,FTEST_DONE,25,2,3\n` y el parser recupera los tres
  contadores
- *(motherBoard `[env:native]`)*

#### Scenario: Cierre malformado
- **WHEN** se parsea `CTRL,FTEST_DONE,25,2` o `CTRL,FTEST_DONE,a,b,c`
- **THEN** el parser devuelve false
- *(motherBoard `[env:native]`)*

### Requirement: Comandos `HMI,FTEST`

El display SHALL ordenar la batería con `HMI,FTEST,START`, un test único con
`HMI,FTEST,RUN,<id>`, cancelar con `HMI,FTEST,ABORT` y contestar a un CONFIRM
con `HMI,FTEST,CONFIRM,<id>,<0|1>`. La motherBoard SHALL descartar con log
cualquier variante con campos de más, de menos o no numéricos, y un `RUN` con
`id` que no sea de motherBoard.

#### Scenario: Parsear los cuatro comandos
- **WHEN** `ftest_parse_hmi_cmd()` recibe `START`, `RUN,14`, `ABORT` y
  `CONFIRM,17,1` (lo que sigue a `HMI,FTEST,`)
- **THEN** devuelve el tipo correcto en cada caso, con `id = 14` en RUN y
  `id = 17`, `ok = true` en CONFIRM
- *(motherBoard `[env:native]`)*

#### Scenario: Comandos inválidos
- **WHEN** recibe `RUN` (sin id), `RUN,64` (id de display), `CONFIRM,17`
  (falta ok), `CONFIRM,17,2` (ok fuera de 0/1) o `FOO`
- **THEN** devuelve false en todos los casos
- *(motherBoard `[env:native]`)*

### Requirement: Compatibilidad con una placa sin soporte

Una placa que no conozca los mensajes `FTEST` SHALL descartarlos como ya hace
con cualquier prefijo desconocido, sin efecto sobre el resto del protocolo. El
display SHALL tratar la ausencia de cualquier `CTRL,FTEST*` durante
`FTEST_MB_RESPONSE_TIMEOUT_MS` (10 s) tras `START` como "motherBoard sin
soporte" y continuar con el resumen.

#### Scenario: motherBoard antigua
- **WHEN** el display envía `HMI,FTEST,START` a una motherBoard que no
  implementa este cambio
- **THEN** la motherBoard sigue emitiendo `CTRL,STATE` y `CTRL,TEL` con
  normalidad
- **AND** a los 10 s el display marca la sección de motherBoard como "sin
  soporte" y muestra el resumen de sus tests locales
- *(Verificación manual en banco: Display_HMI no tiene entorno de test.)*
