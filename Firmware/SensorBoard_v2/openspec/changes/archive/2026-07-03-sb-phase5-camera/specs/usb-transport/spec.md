# usb-transport (delta)

## MODIFIED Requirements

### Requirement: Emisión única por cola TX

Todo dato saliente SHALL pasar por `sensorBoard_comm_send_json()` o `sensorBoard_comm_send_binary()`, que codifican el frame y lo encolan; solo `usb_tx_task` escribe en el endpoint CDC. `send_binary` SHALL enmarcar el payload en un buffer asignado en PSRAM (fallback a heap interna) cuyo **ownership pasa a la cola TX**: `usb_tx_task` lo transmite por chunks y lo libera siempre (también si el host no está conectado o el envío falla).

#### Scenario: JSON demasiado grande

- **WHEN** `send_json` recibe una cadena mayor que `SB_PROTO_MAX_JSON_PAYLOAD` (256 B)
- **THEN** devuelve error sin encolar nada

#### Scenario: send_binary con argumentos inválidos

- **WHEN** `send_binary` recibe buf NULL, longitud 0 o mayor que `SB_PROTO_MAX_BINARY_PAYLOAD`
- **THEN** devuelve error sin asignar memoria ni encolar

#### Scenario: send_binary sin init

- **WHEN** se invoca `send_binary` antes de `sensorBoard_comm_init()`
- **THEN** devuelve `ESP_ERR_INVALID_STATE`

#### Scenario: Round-trip de frame binario grande

- **WHEN** se codifica un payload `TYPE=0x01` de varios KB con `sb_frame_encode` y se alimenta a un decoder con buffer suficiente
- **THEN** el callback recibe el mismo tipo y payload exactos (el framing es el mismo de Fase 1)
