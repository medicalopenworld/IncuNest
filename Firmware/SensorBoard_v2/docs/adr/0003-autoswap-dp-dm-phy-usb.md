# 0003. Autoswap de D+/D- en el PHY USB ante conector invertido

**Estado:** Aceptada
**Fecha:** 2026-09-03

## Contexto

El conector USB SensorBoard↔motherboard admite dos orientaciones y en una de ellas D+/D- (IO20/IO19) llegan cruzados; el host ve un dispositivo low-speed que no responde y la enumeración falla. El origen es de hardware (cableado del conector) y se corregirá en la siguiente revisión de PCB, pero las unidades existentes y las reconexiones en campo (dispositivo médico, usuario no técnico) exigen tolerancia en firmware. Se planteó como alternativa un fallback a UART o I2C por los mismos pines.

Restricciones: `usb_comm` es agnóstico al payload y no debe reabrirse desde otras capas; la cámara (Fase 5) necesita el canal binario USB; la motherboard no debe cambiar su transporte.

## Decisión

El ESP32-S3 permite intercambiar D+/D- por software en el PHY (`USB_WRAP.otg_conf.exchg_pins` + `exchg_pins_override`, expuesto por ESP-IDF v6 como `usb_wrap_ll_phy_enable_pin_exchg()`). `usb_comm` incorpora una **política de orientación pura** (`sb_usb_orient_*`) que, si no se alcanza `tud_mounted()` en 2 s, fuerza detach → intercambio → attach y sigue alternando hasta enumerar. El fallback UART/I2C se descarta.

## Consecuencias

- Un solo protocolo y un solo transporte; cero cambios en la motherboard; cambio contenido en `usb_comm` (init + `usb_tx_task`).
- En la orientación invertida el arranque enumera ~2 s más tarde. Cada intercambio deja un `ESP_LOGW` retenido que la motherboard ve al conectar: el conector invertido es diagnosticable.
- El bootloader ROM no aplica el intercambio: flashear por USB con el cable al revés sigue fallando (el flasher debe indicar "gira el cable"). Existe el eFuse `USB_EXCHG_PINS` para un cruce **permanente** (lo aplica también el ROM), pero no sirve para un cruce que depende de la orientación.
- La política se prueba con Unity sin host; la secuencia con el PHY se verifica on-target (manual).
- Convención derivada: la señal de "enlace sano" es `tud_mounted()`, nunca `tud_connected()` (un bus reset es SE0, simétrico ante el cruce, y pondría `connected` sin enumeración posible).

## Alternativas consideradas

- **Fallback a UART/I2C por IO19/IO20** — duplica la pila de transporte en ambas placas (y el parser de la motherboard) en un dispositivo médico; no es simétrico ante el cruce (TX/RX y SDA/SCL también se invierten, exigiría autodetección igual); pierde ancho de banda para la cámara y la enumeración/desconexión que da USB.
- **Solo corrección en fabricación (conector con llave / pinout USB-C correcto)** — necesaria y planificada para la siguiente revisión de PCB, pero no protege a las unidades ya fabricadas ni a la reconexión en campo.
- **eFuse `USB_EXCHG_PINS`** — cruce permanente e irreversible; solo válido si la placa siempre estuviera cruzada, no para un cruce dependiente de la orientación.
