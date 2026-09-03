# usb-transport (delta)

## MODIFIED Requirements

### Requirement: Tolerancia a la inversión de D+/D-

`usb_comm` SHALL tolerar que el conector USB (SensorBoard HW_NUM 4) llegue con D+ y D- cruzados, intercambiándolos en el PHY USB del ESP32-S3 (`exchg_pins`) con un detach/attach visible para el host, **solo con evidencia de host**: la política SHALL armar un plazo de `CONFIG_SB_USB_AUTOSWAP_TIMEOUT_MS` (2000 ms por defecto, mínimo 1500) al observar el **primer bus reset** (`DCD_EVENT_BUS_RESET`) y, si al vencer el plazo el host no ha hablado al dispositivo (ni primer SETUP — `tud_connected` — ni `SET_CONFIGURATION` — `tud_mounted`), SHALL intercambiar **una sola vez** y esperar un nuevo bus reset antes de volver a intercambiar. Sin bus reset la orientación NUNCA cambia (arranque y espera sin host en orientación normal). Resets adicionales mientras el plazo está armado NO lo extienden. Con host activo NUNCA intercambia y el plazo se desarma. La decisión SHALL residir en una política pura (`sb_usb_orient_*`) sin dependencia de hardware. Cada intercambio SHALL registrarse con `ESP_LOGW` (visible en la retención de arranque al conectar) y el estado SHALL exponerse en la resp de `status` como `sensors.usb_swap`. Antes de intercambiar, `usb_comm` SHALL re-armar la retención de arranque (el flag DTR es pegajoso sin VBUS sensing), y toda escritura al CDC SHALL exigir DTR visto **y** `tud_cdc_n_connected()`.

#### Scenario: Sin evidencia de host no intercambia

- **WHEN** la política recibe ticks sin bus reset y sin host activo durante un tiempo arbitrariamente largo
- **THEN** no devuelve ninguna acción y la orientación sigue siendo la normal

#### Scenario: Reset sin host: un intercambio y quieto

- **WHEN** ve un bus reset en `t0` y no llega host activo
- **THEN** no actúa antes de `t0 + T`, devuelve la acción de intercambio exactamente una vez en `>= t0 + T`, la orientación pasa a intercambiada, y sin un nuevo reset no vuelve a actuar aunque pase el tiempo

#### Scenario: Resets repetidos no extienden el plazo

- **WHEN** ve bus resets en `t0`, `t0 + T/4` y `t0 + T/2` sin host activo
- **THEN** intercambia en `t0 + T`, no más tarde

#### Scenario: Host activo desarma

- **WHEN** ve un bus reset en `t0` y host activo antes de `t0 + T`
- **THEN** no devuelve ninguna acción en `t0 + T` ni después, y una caída posterior del host sin nuevo reset tampoco provoca intercambio

#### Scenario: Reset y host activo en el mismo tick

- **WHEN** en un mismo tick se observa un bus reset y el host ya está activo
- **THEN** no se arma ningún plazo: los ticks posteriores sin host ni reset no provocan intercambio

#### Scenario: Nueva evidencia tras intercambiar

- **WHEN** tras un intercambio ve otro bus reset sin host activo
- **THEN** intercambia de nuevo (vuelve a la orientación anterior) `T` después de ese reset, y el contador de intercambios vale 2

#### Scenario: Desbordamiento del reloj

- **WHEN** `now_ms` da la vuelta (uint32) entre el reset y el vencimiento
- **THEN** el plazo vence igualmente en `t0 + T` módulo 2^32

#### Scenario: Política desactivada

- **WHEN** el timeout configurado es 0
- **THEN** ningún tick devuelve acción, con o sin reset, con o sin host
