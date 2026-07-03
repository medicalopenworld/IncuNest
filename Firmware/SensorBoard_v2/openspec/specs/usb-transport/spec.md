# usb-transport Specification

## Purpose
TBD - created by archiving change sb-phase1-usb-cdc. Update Purpose after archive.
## Requirements
### Requirement: CRC16-CCITT FALSE

El componente `usb_comm` SHALL calcular CRC16-CCITT FALSE (polinomio 0x1021, init 0xFFFF) sobre Type+Length+Payload, con API incremental (`sb_crc16_byte`) y por bloque (`sb_crc16`).

#### Scenario: Vector conocido

- **WHEN** se calcula el CRC de los 9 bytes ASCII `"123456789"`
- **THEN** el resultado es `0x29B1`

#### Scenario: Datos vacíos

- **WHEN** se calcula el CRC de 0 bytes
- **THEN** el resultado es el valor inicial `0xFFFF`

#### Scenario: Incremental equivale a bloque

- **WHEN** se calcula el CRC de un bloque byte a byte con `sb_crc16_byte`
- **THEN** el resultado es idéntico al de `sb_crc16` sobre el bloque completo

### Requirement: Frame encoder

`sb_frame_encode` SHALL producir `Magic(0xAB,0xCD) + Type(1B) + Length(4B little-endian) + Payload + CRC16(2B big-endian)`.

#### Scenario: Layout del frame

- **WHEN** se codifica un payload de N bytes
- **THEN** la salida mide N+9 bytes, empieza por `0xAB 0xCD`, lleva el tipo en el byte 2, la longitud little-endian en los bytes 3-6 y el payload desde el byte 7

#### Scenario: Buffer insuficiente

- **WHEN** el buffer de salida es menor que payload+9
- **THEN** el encoder devuelve 0 y no escribe fuera del buffer

### Requirement: Frame decoder con resincronización

El decoder SHALL ser una máquina de estados byte a byte que entrega frames válidos por callback, descarta silenciosamente frames con CRC inválido, se resincroniza tras bytes basura y rechaza longitudes mayores que su buffer.

#### Scenario: Round-trip

- **WHEN** se alimenta el decoder con un frame codificado por `sb_frame_encode`
- **THEN** el callback recibe exactamente una vez el mismo tipo y payload

#### Scenario: CRC corrupto

- **WHEN** se alimenta un frame con el CRC alterado
- **THEN** el callback no se invoca

#### Scenario: Resync tras basura

- **WHEN** se alimentan bytes arbitrarios que no forman un frame y después un frame válido
- **THEN** el callback se invoca exactamente una vez con el frame válido

#### Scenario: Payload vacío

- **WHEN** se alimenta un frame válido de longitud 0
- **THEN** el callback se invoca con longitud 0

#### Scenario: Longitud excesiva

- **WHEN** el campo Length excede el tamaño del buffer del decoder
- **THEN** el decoder descarta el frame y vuelve a buscar magic sin desbordar el buffer

### Requirement: Emisión única por cola TX

Todo dato saliente SHALL pasar por `sensorBoard_comm_send_json()` (o `send_binary()` en el futuro), que codifica el frame y lo encola; solo `usb_tx_task` escribe en el endpoint CDC.

#### Scenario: JSON demasiado grande

- **WHEN** `send_json` recibe una cadena mayor que `SB_PROTO_MAX_JSON_PAYLOAD` (256 B)
- **THEN** devuelve error sin encolar nada

#### Scenario: send_binary sin implementar

- **WHEN** se invoca `sensorBoard_comm_send_binary()` en Fase 1
- **THEN** devuelve `ESP_ERR_NOT_SUPPORTED`

### Requirement: Logs como frames JSON

`usb_comm` SHALL interceptar `ESP_LOG` vía `esp_log_set_vprintf()` y emitirlo como frames JSON `{"type":"log","ts":...,"msg":"..."}` con `"` y `\` escapados; si la cola TX no existe aún, el log se descarta sin bloquear.

#### Scenario: Log antes de init

- **WHEN** se emite un `ESP_LOG` antes de que exista la cola TX
- **THEN** el interceptor retorna sin bloquear ni crashear

