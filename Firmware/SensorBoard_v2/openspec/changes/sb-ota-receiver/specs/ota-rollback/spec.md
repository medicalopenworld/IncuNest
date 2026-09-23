## ADDED Requirements

### Requirement: La imagen nueva arranca en pending-verify

Con `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y`, el primer arranque de una imagen recién comprometida SHALL quedar en `ESP_OTA_IMG_PENDING_VERIFY`. El firmware SHALL NOT llamar a `esp_ota_mark_app_valid_cancel_rollback()` en el arranque ni por el mero hecho de haber arrancado.

#### Scenario: Estado tras el primer arranque

- **WHEN** la placa arranca por primera vez una imagen recién comprometida
- **THEN** la partición está en pending-verify y el firmware no la ha marcado válida
- (Verificación manual con `idf.py -p COMx flash monitor`: implica flash y reinicio.)

#### Scenario: La opción de Kconfig está realmente activa

- **WHEN** se compila tras añadir `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y` a `sdkconfig.defaults`
- **THEN** el símbolo aparece activo en el `sdkconfig` generado, no descartado en silencio por `confgen`
- (Verificación manual con `idf.py build` tras borrar `sdkconfig`, comprobando el símbolo — es la trampa documentada en el CHANGELOG.)

### Requirement: Solo la confirmación de la motherboard resuelve el pending-verify

El firmware SHALL llamar a `esp_ota_mark_app_valid_cancel_rollback()` únicamente al recibir el subopcode `confirm`, que la motherboard emite tras haber visto el heartbeat de la imagen nueva. Ausencia de `confirm` SHALL dejar que el bootloader revierta al siguiente reinicio. El criterio es deliberadamente más estricto que "he arrancado": el fallo que importa es una imagen que arranca pero no publica lecturas, porque deja a la motherboard sin la variable de su PID de aire.

#### Scenario: Confirmación recibida

- **WHEN** llega `confirm` estando la imagen en pending-verify
- **THEN** se marca válida, se cancela el rollback y `ota_state` pasa a `idle`
- (Verificación manual en banco con la motherboard.)

#### Scenario: Sin confirmación, el bootloader revierte

- **WHEN** una imagen en pending-verify se reinicia sin haber recibido nunca `confirm`
- **THEN** el bootloader arranca la partición anterior
- (Verificación manual con una imagen deliberadamente incapaz de publicar.)

#### Scenario: Confirmación fuera de contexto

- **WHEN** llega `confirm` sin que haya ninguna imagen en pending-verify
- **THEN** se ignora sin error y sin efecto sobre `otadata`
- (Verificado con Unity en `test_apps/comm_test` para la máquina de estados.)

### Requirement: Reversión y arranque anterior no dejan rastro que impida reintentar

Tras una reversión del bootloader, la placa SHALL quedar operativa con su imagen anterior y SHALL aceptar una nueva `offer` con normalidad. El slot que contenía la imagen revertida SHALL poder reescribirse sin intervención manual.

#### Scenario: Reintento tras una reversión

- **WHEN** el bootloader ha revertido a la imagen anterior y llega una `offer` nueva
- **THEN** se acepta, se borra el slot inactivo y la transferencia procede
- (Verificación manual en banco.)

### Requirement: `ota_state` en la resp de `status`

La resp de `status` SHALL incluir `ota_state` con uno de `idle`, `receiving`, `pending_verify` o `error`, para que la motherboard consulte el estado del receptor en vez de mantener un estado espejo que un reinicio de cualquiera de las dos placas dejaría desincronizado.

#### Scenario: Estado en reposo

- **WHEN** llega `status` sin transferencia en curso y sin imagen pendiente de verificar
- **THEN** la resp incluye `"ota_state":"idle"` junto a los campos que ya tenía
- (Verificado con Unity en `test_apps/comm_test`.)

#### Scenario: Estado durante una transferencia

- **WHEN** llega `status` con una transferencia activa
- **THEN** la resp incluye `"ota_state":"receiving"`
- (Verificado con Unity en `test_apps/comm_test`.)

#### Scenario: Estado tras comprometer una imagen

- **WHEN** llega `status` en el primer arranque de una imagen recién comprometida y aún no confirmada
- **THEN** la resp incluye `"ota_state":"pending_verify"`
- (Verificación manual: requiere un arranque real en pending-verify.)
