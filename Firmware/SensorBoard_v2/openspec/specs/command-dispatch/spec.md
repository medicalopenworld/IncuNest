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

### Requirement: Disponibilidad de sensores en status

La respuesta de `status` SHALL incluir un objeto `"sensors"` con una clave booleana por sensor registrado vía `sensorBoard_status_set_sensor(name, available)`. El registro es agnóstico (nombres opacos, tabla estática de 8 entradas); si no hay sensores registrados, el campo `sensors` se omite.

#### Scenario: Sensores registrados

- **WHEN** se registran `sht0:true` y `als:false` y llega un cmd `status`
- **THEN** la resp contiene `"sensors":{"sht0":true,"als":false}` además de los campos de Fase 1

#### Scenario: Sin sensores registrados

- **WHEN** ningún sensor se ha registrado y llega un cmd `status`
- **THEN** la resp es exactamente la de Fase 1 (sin campo `sensors`)

#### Scenario: Re-registro actualiza

- **WHEN** `sht0` se registra dos veces (true, luego false)
- **THEN** la tabla mantiene una sola entrada `sht0` con el último valor

#### Scenario: Tabla llena

- **WHEN** se intenta registrar un noveno nombre distinto
- **THEN** el registro devuelve error sin corromper la tabla

### Requirement: Registro de comandos por componente

`usb_comm` SHALL exponer `sensorBoard_cmd_register(cmd, handler)` (tabla estática, nombres opacos): el dispatcher, tras sus comandos propios (`status`), SHALL invocar el handler registrado que coincida con `cmd`, pasándole el `id`. Sin coincidencia, se mantiene la resp `cmd not found`.

#### Scenario: Comando registrado se despacha

- **WHEN** `camera_sensor` registra `capture` y llega `{"type":"cmd","cmd":"capture","id":7}`
- **THEN** el handler registrado se invoca con id=7 y no se emite `cmd not found`

#### Scenario: Re-registro reemplaza

- **WHEN** se registra dos veces el mismo nombre
- **THEN** la tabla mantiene una entrada con el último handler

#### Scenario: Tabla llena

- **WHEN** se intenta registrar más comandos que la capacidad
- **THEN** el registro devuelve error sin corromper la tabla

#### Scenario: Desconocido sigue fallando

- **WHEN** llega un cmd no registrado ni propio
- **THEN** la resp es `status:"error"`, `msg:"cmd not found"` (comportamiento de Fase 1 intacto)

