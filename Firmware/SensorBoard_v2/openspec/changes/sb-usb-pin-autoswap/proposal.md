# Proposal — sb-usb-pin-autoswap

## Why

El conector USB SensorBoard↔motherboard admite dos orientaciones y, en una de ellas, D+ y D- (IO20/IO19) llegan cruzados: el host ve un dispositivo low-speed que no responde y la enumeración falla. El SensorBoard queda mudo (sin heartbeat) y la motherboard lo declara "no disponible" aunque el hardware esté sano. El origen es de hardware (cableado del conector) y se corregirá en la siguiente revisión de PCB, pero el equipo puede montarse o reconectarse en campo con el cable al revés: el firmware debe ser tolerante a la orientación.

Descartado el fallback UART/I2C por los mismos pines: duplica la pila de transporte en ambas placas, no es simétrico ante el cruce (TX/RX y SDA/SCL también se invierten) y pierde el canal binario de la cámara. Ver ADR-0003.

## What Changes

- **`usb_comm` (acotado a init + `usb_tx_task`, sin tocar framing/CRC/RX):**
  - Nuevo módulo puro `sensorBoard_usb_orient` (política de orientación): máquina de estados sin dependencia de hardware que, alimentada periódicamente con "¿enlace montado?" y el instante actual, decide cuándo intercambiar D+/D-. Testeable con Unity sin host.
  - Integración: si el dispositivo no llega a `tud_mounted()` en `CONFIG_SB_USB_AUTOSWAP_TIMEOUT_MS` (2000 ms por defecto), `usb_tx_task` hace `tud_disconnect()` → `usb_wrap_ll_phy_enable_pin_exchg(&USB_WRAP, swapped)` → pausa → `tud_connect()`, y sigue alternando mientras no haya enumeración. Con el enlace montado nunca se intercambia; el plazo se rearma en cada tick con enlace.
  - Cada intercambio emite `ESP_LOGW` (queda en la retención de arranque y se vuelca al conectar): la motherboard/monitor ve que el conector está invertido.
  - Kconfig: `SB_USB_AUTOSWAP` (bool, default y) y `SB_USB_AUTOSWAP_TIMEOUT_MS` (500–10000).
- **Sin cambios de protocolo** ni en la motherboard.

## Impact

- Affected specs: `usb-transport` (nuevo requirement "Tolerancia a la inversión de D+/D-").
- Affected code: `components/usb_comm/` (módulo nuevo + init/tx_task + Kconfig + CMake), `test_apps/comm_test`.
- Riesgo: en la orientación invertida el arranque tarda ~2 s más en enumerar; el bootloader ROM (modo download del flasher) no aplica el intercambio, así que flashear por USB con el cable al revés seguirá fallando (el flasher debe indicar "gira el cable"). Sin VBUS sensing, `tud_mounted()` se mantiene en true tras un desenchufe físico hasta el siguiente bus reset del host — el intercambio se dispara igualmente al reconectar porque el host emite reset (SE0, simétrico ante el cruce) antes de intentar enumerar.

## Non-goals

- Fallback a UART/I2C.
- Persistir la orientación en NVS (optimización de arranque diferida; no hay NVS inicializado en el firmware).
- Corregir el pinout del conector (hardware, siguiente revisión de PCB).
