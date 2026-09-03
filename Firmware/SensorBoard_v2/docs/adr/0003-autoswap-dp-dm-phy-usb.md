# 0003. Autoswap de D+/D- en el PHY USB ante conector invertido

**Estado:** Aceptada
**Fecha:** 2026-09-03

## Contexto

En el **SensorBoard HW_NUM 4** el conector USB SensorBoard↔motherboard admite dos orientaciones y en una de ellas D+/D- (IO20/IO19) llegan cruzados; el host ve un dispositivo low-speed que no responde y la enumeración falla. El origen es de hardware (cableado del conector). La **V5**, ya enviada a fabricar (2026-09), lleva el fix en hardware; pero las unidades HW4 existentes y sus reconexiones en campo (dispositivo médico, usuario no técnico) exigen tolerancia en firmware. Se planteó como alternativa un fallback a UART o I2C por los mismos pines.

Restricciones: `usb_comm` es agnóstico al payload y no debe reabrirse desde otras capas; la cámara (Fase 5) necesita el canal binario USB; la motherboard no debe cambiar su transporte.

## Decisión

El ESP32-S3 permite intercambiar D+/D- por software en el PHY (`USB_WRAP.otg_conf.exchg_pins` + `exchg_pins_override`, expuesto por ESP-IDF v6 como `usb_wrap_ll_phy_enable_pin_exchg()`). `usb_comm` incorpora una **política de orientación pura** (`sb_usb_orient_*`) que, si no hay **host activo** (`tud_mounted() || tud_connected()`) en 2 s, fuerza detach → intercambio → attach y sigue alternando hasta enumerar. El fallback UART/I2C se descarta. El autoswap se mantiene activo por defecto también en la V5: con host activo nunca actúa, así que en hardware corregido es inocuo y cubre un cable o adaptador cruzado.

## Consecuencias

- Un solo protocolo y un solo transporte; cero cambios en la motherboard; cambio contenido en `usb_comm` (init + `usb_tx_task`).
- En la orientación invertida (solo HW4) el arranque enumera ~2 s más tarde. Cada intercambio deja un `ESP_LOGW` retenido y el estado queda consultable en `status` como `sensors.usb_swap`: el conector invertido es diagnosticable de forma determinista (criterio de producción: `usb_swap:true` en una unidad recién ensamblada = defecto de cableado).
- El bootloader ROM no aplica el intercambio: flashear por USB con el cable al revés sigue fallando (el flasher debe consultar `usb_swap` o indicar "gira el cable" antes de forzar el modo download). Existe el eFuse `USB_EXCHG_PINS` para un cruce **permanente** (lo aplica también el ROM), pero no sirve para un cruce que depende de la orientación.
- La política se prueba con Unity sin host; la secuencia con el PHY se verifica on-target (manual).
- Convención derivada: **"host activo" = `tud_mounted() || tud_connected()`**. `connected` lo pone TinyUSB al recibir el primer paquete SETUP (`usbd.c`) y lo borra el bus reset/unplug; un SETUP válido es imposible con D+/D- cruzados, así que es prueba de orientación correcta y un host lento en configurar nunca dispara intercambios (sin necesidad de backoff/jitter). `mounted` se mantiene en suspend, así que un host dormido tampoco. *Corrección registrada:* el diseño inicial descartaba `tud_connected()` asumiendo que se ponía con el bus reset (SE0, simétrico ante el cruce); la revisión de seguridad lo contrastó con el fuente de TinyUSB y era falso.
- Convención derivada: DTR es pegajoso (TinyUSB no invoca `line_state_cb` en reset/unplug y el S3 no tiene VBUS sensing): la condición para escribir al CDC es `DTR && tud_cdc_n_connected()`, nunca DTR solo.

## Enmienda 2026-09-03 (banco HW4 + motherboard V18): solo con evidencia de host

La versión inicial alternaba la orientación cada 2 s mientras no hubiera host activo, también sin host. En banco funcionó con un PC pero no con la motherboard: su pila host (ESP-IDF 4.4.6 en Arduino) solo recupera el puerto raíz tras una desconexión y tarda más que la ventana buena (1,75 s) pero menos que el ciclo (4 s), así que cada reintento caía en la orientación mala (`HUB: Bad transfer status 1: CHECK_SHORT_DEV_DESC` cada 4,01 s, `devices=0` durante minutos). Si al encender el puerto encontraba la ventana buena, enumeraba en <1 s: una moneda al aire por arranque.

**Decisión enmendada:** la política intercambia únicamente tras ver un **bus reset** (`DCD_EVENT_BUS_RESET`, evidencia de host, simétrico ante el cruce) sin que el host hable en `T`, **una sola vez por evidencia**, y permanece en la nueva orientación hasta el siguiente reset. Sin host no se mueve. Resets adicionales no extienden el plazo (Windows hace 3 seguidos). Así el host, tarde lo que tarde en recuperarse, encuentra siempre la orientación correcta, y con el cable correcto nunca ve una ventana low-speed. La motherboard no necesita recuperación adicional; se le añade un log INFO de cada dispositivo enumerado (VID/PID/velocidad) para diagnóstico. Change `2026-09-03-sb-usb-autoswap-host-evidence`.

Aprendizaje: "el host se recupera de una enumeración fallida" era una suposición sobre una pila concreta que solo el banco podía refutar; la política pura permitió cambiar la regla sin tocar la ejecución sobre el PHY.

**Causa raíz final (banco con UART0 del SensorBoard, 2026-09-03):** con la política v2 el SensorBoard intercambiaba correctamente, pero la motherboard **no detectaba la conexión en modo intercambiado** (ningún reset nuevo en el SensorBoard, ningún intento en el host), mientras que un PC sí. La motherboard hace `gpio_reset_pin(19/20)` para soltar el bus I2C2 antes de arrancar el host y esa llamada deja el **pull-up interno (~45 kΩ) habilitado** sobre las líneas USB: con la pull-down de 15 kΩ del host la línea "baja" queda a ~0,8 V, en el umbral del receptor single-ended del PHY, y la detección de conexión fallaba en una de las orientaciones. Fix en la motherboard: `gpio_pullup_dis`/`gpio_pulldown_dis` en 19/20 antes de `usb_host_install`. Con ese fix, cable invertido → un intercambio → enlace en ~1 s tras arrancar el host. El detach del intercambio pasa a 1 s (`CONFIG_SB_USB_SWAP_DETACH_MS`) para dar margen a la recuperación del puerto del host, y el enganche de fase de la v1 queda explicado: las ventanas "intercambiadas" eran invisibles para la motherboard.

## Alternativas consideradas

- **Fallback a UART/I2C por IO19/IO20** — duplica la pila de transporte en ambas placas (y el parser de la motherboard) en un dispositivo médico; no es simétrico ante el cruce (TX/RX y SDA/SCL también se invierten, exigiría autodetección igual); pierde ancho de banda para la cámara y la enumeración/desconexión que da USB.
- **Solo corrección en fabricación (V5, ya enviada a fabricar)** — necesaria y hecha, pero no protege a las unidades HW4 ya fabricadas ni a la reconexión en campo.
- **Backoff/jitter en la alternancia** — innecesario una vez la guarda incluye el primer SETUP: no existe secuencia en la que la orientación correcta se abandone mientras el host intenta enumerar.
- **eFuse `USB_EXCHG_PINS`** — cruce permanente e irreversible; solo válido si la placa siempre estuviera cruzada, no para un cruce dependiente de la orientación.
