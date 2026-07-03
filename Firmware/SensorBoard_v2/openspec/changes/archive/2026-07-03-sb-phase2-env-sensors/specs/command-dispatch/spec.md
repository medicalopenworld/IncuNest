# command-dispatch (delta)

## ADDED Requirements

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
