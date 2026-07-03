# command-dispatch Specification

## Purpose
TBD - created by archiving change sb-phase1-usb-cdc. Update Purpose after archive.
## Requirements
### Requirement: Comando status

El SensorBoard SHALL responder a `{"type":"cmd","cmd":"status","id":N}` con `{"type":"resp","cmd":"status","id":N,"status":"ok","device":"SensorBoard","fw":"<versión>","uptime":<ms>}`.

#### Scenario: status responde ok

- **WHEN** llega un frame JSON con `cmd:"status"` e `id` numérico
- **THEN** se emite una resp con `status:"ok"`, el mismo `id`, `device`, `fw` y `uptime` en ms

### Requirement: Comando desconocido

Un comando no reconocido SHALL producir `{"type":"resp","cmd":"<cmd>","id":N,"status":"error","msg":"cmd not found",...}` sin afectar al resto del sistema.

#### Scenario: cmd desconocido

- **WHEN** llega un frame JSON con `cmd:"foo"`
- **THEN** se emite una resp con `status:"error"` y `msg:"cmd not found"`

#### Scenario: JSON sin campo cmd

- **WHEN** llega un frame JSON válido sin campo `cmd`
- **THEN** se emite una resp de error indicando el campo ausente

#### Scenario: JSON malformado

- **WHEN** llega un payload que no parsea como JSON
- **THEN** el dispatcher lo descarta sin crashear ni responder

### Requirement: Heartbeat

El firmware SHALL emitir `{"type":"event","cmd":"heartbeat","uptime":<ms>}` cada 30 s como señal de vida.

#### Scenario: heartbeat periódico

- **WHEN** el firmware lleva más de 30 s arrancado
- **THEN** se ha emitido al menos un evento `heartbeat` con `uptime` creciente

