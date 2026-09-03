# usb-transport (delta)

## ADDED Requirements

### Requirement: Tolerancia a la inversión de D+/D-

`usb_comm` SHALL tolerar que el conector USB llegue con D+ y D- cruzados: si el dispositivo no alcanza el estado montado (`SET_CONFIGURATION`) en `CONFIG_SB_USB_AUTOSWAP_TIMEOUT_MS` (2000 ms por defecto), SHALL intercambiar D+/D- en el PHY USB del ESP32-S3 (`exchg_pins`), forzar un detach/attach y seguir alternando la orientación en cada plazo vencido mientras no haya enumeración. Con el enlace montado NUNCA intercambia. La decisión SHALL residir en una política pura (`sb_usb_orient_*`) sin dependencia de hardware, alimentada con el estado del enlace y el instante actual. Cada intercambio SHALL registrarse con `ESP_LOGW` (visible en la retención de arranque al conectar).

#### Scenario: Sin enumeración antes del plazo

- **WHEN** la política se inicializa en `t0` con timeout `T` y recibe ticks sin enlace en instantes `< t0 + T`
- **THEN** no devuelve ninguna acción y la orientación sigue siendo la normal

#### Scenario: Plazo vencido sin enumeración

- **WHEN** recibe un tick sin enlace en un instante `>= t0 + T`
- **THEN** devuelve la acción de intercambio exactamente una vez, la orientación pasa a intercambiada y el plazo se rearma `T` ms a partir de ese instante

#### Scenario: Alternancia sostenida sin host

- **WHEN** siguen venciendo plazos sin enlace
- **THEN** la orientación alterna normal → intercambiada → normal en cada vencimiento y el contador de intercambios crece en uno por vencimiento

#### Scenario: Enlace montado

- **WHEN** recibe ticks con enlace montado, incluso pasado el plazo original
- **THEN** no devuelve ninguna acción y el plazo se rearma desde el último tick con enlace, de modo que una caída posterior del enlace cuenta `T` ms desde la caída

#### Scenario: Desbordamiento del reloj

- **WHEN** `now_ms` da la vuelta (uint32) entre la inicialización y el vencimiento
- **THEN** el plazo vence igualmente en `t0 + T` módulo 2^32

#### Scenario: Política desactivada

- **WHEN** el timeout configurado es 0
- **THEN** ningún tick devuelve acción, con o sin enlace
