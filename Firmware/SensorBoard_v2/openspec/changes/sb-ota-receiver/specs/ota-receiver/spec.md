## ADDED Requirements

### Requirement: Subopcodes OTA sobre el tipo de frame 0x02

El componente `ota` SHALL manejar frames `SB_PROTO_TYPE_OTA` (0x02) cuyo primer byte de payload es el subopcode: `offer`, `accept`, `refuse`, `chunk`, `chunk_ack`, `commit`, `abort` y `confirm`. Un subopcode desconocido SHALL descartarse sin afectar a una transferencia en curso ni al resto del sistema, igual que hace el dispatcher de comandos con un `cmd` no reconocido.

#### Scenario: Subopcode desconocido

- **WHEN** llega un frame de tipo 0x02 cuyo primer byte no corresponde a ningún subopcode conocido
- **THEN** se descarta sin escribir en flash, sin abortar una transferencia en curso y sin crashear
- (Verificado con Unity en `test_apps/comm_test`.)

#### Scenario: Frame 0x02 con payload vacío

- **WHEN** llega un frame de tipo 0x02 de longitud 0, sin siquiera el byte de subopcode
- **THEN** se descarta sin leer fuera del buffer
- (Verificado con Unity en `test_apps/comm_test`.)

### Requirement: Validación de la oferta antes de tocar flash

Una `offer` SHALL declarar tamaño de imagen y digest SHA-256. El receptor SHALL rechazarla, respondiendo `refuse` con un motivo, cuando el tamaño exceda la capacidad del slot inactivo, cuando el tamaño sea 0, cuando el digest no tenga la longitud esperada, o cuando ya haya una transferencia en curso. Ninguna oferta rechazada SHALL borrar el slot inactivo ni abrir un handle OTA.

#### Scenario: Imagen mayor que el slot

- **WHEN** llega una `offer` con un tamaño mayor que los 2 MB del slot inactivo
- **THEN** se responde `refuse` con motivo de tamaño y no se borra el slot
- (Verificado con Unity en `test_apps/comm_test`.)

#### Scenario: Oferta con tamaño cero

- **WHEN** llega una `offer` con tamaño 0
- **THEN** se responde `refuse` y no se abre ningún handle OTA
- (Verificado con Unity en `test_apps/comm_test`.)

#### Scenario: Oferta durante una transferencia en curso

- **WHEN** llega una `offer` mientras hay otra transferencia activa
- **THEN** se responde `refuse` y la transferencia en curso continúa intacta
- (Verificado con Unity en `test_apps/comm_test`.)

#### Scenario: Oferta válida acepta y borra el slot

- **WHEN** llega una `offer` con tamaño y digest válidos y no hay transferencia en curso
- **THEN** se responde `accept`, se borra el slot inactivo y el estado pasa a `receiving`
- (Verificación manual con `idf.py -p COMx flash monitor`: implica flash real.)

### Requirement: Escritura por chunks secuenciales de 4 KB

Los `chunk` SHALL llevar número de secuencia y datos, con un tamaño de datos de hasta 4096 bytes (el buffer RX de CDC, que coincide con la página de borrado de flash). El receptor SHALL escribir con `esp_ota_write()` únicamente el chunk cuya secuencia sea la esperada, responder `chunk_ack` con esa secuencia, y rechazar sin escribir cualquier chunk fuera de secuencia o de longitud mayor a la máxima.

#### Scenario: Chunk en secuencia se escribe y se confirma

- **WHEN** llega el chunk N siendo N el esperado
- **THEN** se escribe, se acumula en el digest y se responde `chunk_ack` con N
- (Lógica de secuencia y digest verificada con Unity en `test_apps/comm_test`; la escritura en flash es verificación manual.)

#### Scenario: Chunk fuera de secuencia

- **WHEN** llega un chunk con una secuencia distinta de la esperada
- **THEN** no se escribe nada y no se avanza el contador de secuencia
- (Verificado con Unity en `test_apps/comm_test`.)

#### Scenario: Chunk sobredimensionado

- **WHEN** llega un chunk cuyos datos superan el máximo
- **THEN** se rechaza sin escribir y sin desbordar el buffer
- (Verificado con Unity en `test_apps/comm_test`.)

#### Scenario: Suma de chunks distinta del tamaño ofertado

- **WHEN** llega un `commit` habiendo recibido menos bytes de los que declaraba la `offer`
- **THEN** la transferencia se aborta sin cambiar la partición de arranque
- (Verificado con Unity en `test_apps/comm_test`.)

### Requirement: Verificación del digest antes del commit

El receptor SHALL acumular SHA-256 sobre los bytes escritos y compararlo con el de la `offer` al recibir `commit`. Solo con `esp_ota_end()` correcto **y** digest coincidente SHALL llamar a `esp_ota_set_boot_partition()`. Un digest discrepante SHALL abortar la transferencia dejando `otadata` apuntando a la imagen anterior.

#### Scenario: Digest correcto

- **WHEN** llega `commit` y el digest acumulado coincide con el ofertado
- **THEN** se fija la partición de arranque al slot recién escrito y la placa se reinicia
- (Verificación manual: implica flash y reinicio.)

#### Scenario: Digest discrepante

- **WHEN** llega `commit` y el digest acumulado no coincide
- **THEN** no se cambia la partición de arranque, se reporta el error y la placa sigue con su imagen actual
- (Acumulación y comparación verificadas con Unity en `test_apps/comm_test`; el efecto sobre `otadata` es verificación manual.)

### Requirement: Abort y transferencia huérfana

Un `abort` de la motherboard, la pérdida del host, o la ausencia de chunks durante un tiempo límite SHALL cerrar la transferencia, liberar el handle OTA y volver al estado `idle` sin cambiar la partición de arranque. Una transferencia abandonada SHALL NOT dejar el receptor bloqueado para una oferta posterior.

#### Scenario: Abort explícito

- **WHEN** llega `abort` a mitad de una transferencia
- **THEN** se libera el handle, el estado vuelve a `idle` y una `offer` posterior se acepta con normalidad
- (Verificado con Unity en `test_apps/comm_test` para la máquina de estados.)

#### Scenario: Silencio prolongado

- **WHEN** una transferencia activa no recibe ningún chunk durante el tiempo límite
- **THEN** se aborta sola y el estado vuelve a `idle`
- (Verificado con Unity en `test_apps/comm_test`.)

### Requirement: El vigilante de host no reinicia durante una transferencia

`sensorBoard_host_watch` SHALL suspenderse mientras hay una transferencia OTA activa y rearmarse al terminar, con éxito o sin él. La suspensión SHALL tener un tope de tiempo absoluto, de modo que una transferencia colgada no desactive el vigilante indefinidamente.

#### Scenario: No reinicia a mitad de escritura

- **WHEN** el vigilante concluiría que ha perdido el host mientras una transferencia OTA está activa
- **THEN** no ordena reinicio
- (Verificado con Unity en `test_apps/comm_test`, con la política pura como el resto de `[hostwatch]`.)

#### Scenario: Se rearma al acabar

- **WHEN** la transferencia termina, con éxito o abortada
- **THEN** el vigilante vuelve a su comportamiento normal
- (Verificado con Unity en `test_apps/comm_test`.)

#### Scenario: Transferencia colgada

- **WHEN** una transferencia queda activa más allá del tope absoluto de suspensión
- **THEN** el vigilante se rearma aunque la transferencia no haya terminado
- (Verificado con Unity en `test_apps/comm_test`.)
