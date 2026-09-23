# door-sensor (delta)

## ADDED Requirements

### Requirement: Evento por cambio estable de estado

El componente `door_sensor` SHALL publicar `door_open`/`door_closed` exactamente una vez por cada cambio **estable** del nivel del hall (tras la ventana de debounce), y SHALL suprimir repeticiones del mismo estado.

#### Scenario: Cambio estable emite una vez

- **WHEN** el nivel estable pasa de cerrado a abierto
- **THEN** se decide un único evento `door_open`

#### Scenario: Mismo estado no re-emite

- **WHEN** tras un evento el nivel estable leído vuelve a ser el mismo (rebote absorbido por la ventana)
- **THEN** no se decide ningún evento

#### Scenario: Estado inicial se publica una vez

- **WHEN** el componente arranca con la puerta cerrada
- **THEN** la primera evaluación decide `door_closed` (la motherboard conoce el estado sin esperar un cambio)

### Requirement: Polaridad configurable

La interpretación nivel→estado SHALL depender de `SB_DOOR_ACTIVE_LOW` (por defecto: nivel bajo = imán presente = puerta cerrada).

#### Scenario: Activo-bajo

- **WHEN** `active_low` y el nivel estable es 0
- **THEN** el estado es "cerrada"

#### Scenario: Activo-alto

- **WHEN** no `active_low` y el nivel estable es 0
- **THEN** el estado es "abierta"

### Requirement: Formato del evento

El evento SHALL ser `{"type":"event","cmd":"door_open","ts":<ms>}` o `{"type":"event","cmd":"door_closed","ts":<ms>}`.

#### Scenario: JSON exacto

- **WHEN** se construye el evento de apertura con ts=5100
- **THEN** el resultado es exactamente `{"type":"event","cmd":"door_open","ts":5100}`
