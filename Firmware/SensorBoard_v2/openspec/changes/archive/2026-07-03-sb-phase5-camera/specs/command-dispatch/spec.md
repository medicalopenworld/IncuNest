# command-dispatch (delta)

## ADDED Requirements

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
