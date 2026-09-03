# usb-transport (delta)

## ADDED Requirements

### Requirement: Tolerancia a la inversión de D+/D-

`usb_comm` SHALL tolerar que el conector USB (SensorBoard HW_NUM 4) llegue con D+ y D- cruzados: si no hay **host activo** — ni primer paquete SETUP recibido (`tud_connected`) ni `SET_CONFIGURATION` (`tud_mounted`) — en `CONFIG_SB_USB_AUTOSWAP_TIMEOUT_MS` (2000 ms por defecto, mínimo 1500), SHALL intercambiar D+/D- en el PHY USB del ESP32-S3 (`exchg_pins`), forzar un detach/attach visible para el host y seguir alternando la orientación en cada plazo vencido mientras no haya host activo. Con host activo NUNCA intercambia. La decisión SHALL residir en una política pura (`sb_usb_orient_*`) sin dependencia de hardware, alimentada con el estado del enlace y el instante actual. Cada intercambio SHALL registrarse con `ESP_LOGW` (visible en la retención de arranque al conectar) y el estado SHALL exponerse en la resp de `status` como `sensors.usb_swap` (`false` desde el arranque, `true` con los pines intercambiados). Antes de intercambiar, `usb_comm` SHALL re-armar la retención de arranque (el flag DTR es pegajoso sin VBUS sensing), y toda escritura al CDC SHALL exigir DTR visto **y** `tud_cdc_n_connected()`.

#### Scenario: Sin enumeración antes del plazo

- **WHEN** la política se inicializa en `t0` con timeout `T` y recibe ticks sin enlace en instantes `< t0 + T`
- **THEN** no devuelve ninguna acción y la orientación sigue siendo la normal

#### Scenario: Plazo vencido sin enumeración

- **WHEN** recibe un tick sin enlace en un instante `>= t0 + T`
- **THEN** devuelve la acción de intercambio exactamente una vez, la orientación pasa a intercambiada y el plazo se rearma `T` ms a partir de ese instante

#### Scenario: Alternancia sostenida sin host

- **WHEN** siguen venciendo plazos sin enlace
- **THEN** la orientación alterna normal → intercambiada → normal en cada vencimiento y el contador de intercambios crece en uno por vencimiento

#### Scenario: Host activo

- **WHEN** recibe ticks con host activo (SETUP recibido o configurado), incluso pasado el plazo original
- **THEN** no devuelve ninguna acción y el plazo se rearma desde el último tick con host activo, de modo que una caída posterior cuenta `T` ms desde la caída

#### Scenario: Desbordamiento del reloj

- **WHEN** `now_ms` da la vuelta (uint32) entre la inicialización y el vencimiento
- **THEN** el plazo vence igualmente en `t0 + T` módulo 2^32

#### Scenario: Política desactivada

- **WHEN** el timeout configurado es 0
- **THEN** ningún tick devuelve acción, con o sin enlace
