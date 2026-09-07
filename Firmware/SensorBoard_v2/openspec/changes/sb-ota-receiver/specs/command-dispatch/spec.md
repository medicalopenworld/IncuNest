## MODIFIED Requirements

### Requirement: Comando status

El SensorBoard SHALL responder a `{"type":"cmd","cmd":"status","id":N}` con `{"type":"resp","cmd":"status","id":N,"status":"ok","device":"SensorBoard","fw":"<versión>","uptime":<ms>,"ota_state":"<estado>"}`. El campo `ota_state` está siempre presente y toma uno de `idle`, `receiving`, `pending_verify` o `error`, para que la motherboard consulte el estado del receptor OTA en vez de mantener un estado espejo que un reinicio de cualquiera de las dos placas dejaría desincronizado.

#### Scenario: status responde ok

- **WHEN** llega un frame JSON con `cmd:"status"` e `id` numérico
- **THEN** se emite una resp con `status:"ok"`, el mismo `id`, `device`, `fw`, `uptime` en ms y `ota_state`
- (Verificado con Unity en `test_apps/comm_test`.)

#### Scenario: ota_state refleja una transferencia en curso

- **WHEN** llega un cmd `status` mientras hay una transferencia OTA activa
- **THEN** la resp lleva `"ota_state":"receiving"` y el resto de campos sin cambios
- (Verificado con Unity en `test_apps/comm_test`.)
