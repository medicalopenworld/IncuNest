## ADDED Requirements

### Requirement: Tipo de frame binario de entrada 0x02 (OTA)

`usb_comm` SHALL definir `SB_PROTO_TYPE_OTA` (0x02) y despachar los frames de ese tipo al manejador registrado por el componente `ota`. El framing no cambia: magic `0xAB,0xCD`, cabecera de 7 bytes, longitud little-endian, CRC16-CCITT big-endian y la validación de longitud contra el buffer del decoder siguen siendo exactamente los de la fase 1, y los tipos `0x00` (JSON) y `0x01` (JPEG) conservan su comportamiento.

Es el primer tipo binario **de entrada**: hasta ahora `usb_comm` solo recibía JSON y solo emitía binario. La excepción a la regla de no tocar `usb_comm` después de la fase 1 está justificada en el design de este cambio.

#### Scenario: Frame 0x02 se despacha al componente OTA

- **WHEN** el decoder entrega un frame válido de tipo 0x02
- **THEN** se invoca el manejador OTA con el payload exacto, y no el dispatcher de comandos JSON
- (Verificado con Unity en `test_apps/comm_test`.)

#### Scenario: Los tipos existentes no cambian de camino

- **WHEN** llegan frames de tipo 0x00 y 0x01 después de añadir el tipo 0x02
- **THEN** siguen yendo al dispatcher JSON y al camino binario existentes, sin cambio de comportamiento
- (Verificado con Unity en `test_apps/comm_test`: los escenarios de `usb-transport` de la fase 1 siguen pasando sin modificación.)

#### Scenario: Frame 0x02 sin manejador registrado

- **WHEN** llega un frame de tipo 0x02 y el componente `ota` no ha registrado manejador
- **THEN** el frame se descarta silenciosamente, sin bloquear ni crashear
- (Verificado con Unity en `test_apps/comm_test`.)

#### Scenario: Longitud excesiva en un frame 0x02

- **WHEN** un frame de tipo 0x02 declara una longitud mayor que el buffer del decoder
- **THEN** se descarta y el decoder vuelve a buscar magic sin desbordar, igual que con cualquier otro tipo
- (Verificado con Unity en `test_apps/comm_test`.)

### Requirement: El conjunto de clases USB no cambia

Este cambio SHALL NOT habilitar ninguna clase USB adicional ni modificar los flags del descriptor. El PID (0x4001) lo deriva TinyUSB de las clases habilitadas y la motherboard abre el dispositivo por VID/PID; habilitar una clase lo movería y el enlace desaparecería sin ningún error de compilación.

#### Scenario: Descriptor intacto

- **WHEN** se compila el firmware con el receptor OTA incluido
- **THEN** `CONFIG_TINYUSB_CDC_COUNT`, `CONFIG_TINYUSB_DESC_USE_ESPRESSIF_VID` y `CONFIG_TINYUSB_DESC_USE_DEFAULT_PID` conservan sus valores y el dispositivo enumera como 0x303A/0x4001
- (Verificación manual: comprobar la enumeración con la motherboard o un host de desarrollo.)
