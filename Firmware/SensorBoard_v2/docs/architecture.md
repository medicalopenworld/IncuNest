# Arquitectura — SensorBoard

Firmware ESP-IDF v6 (CMake, C, FreeRTOS) para el ESP32-S3-WROOM-1-N16R8 del periférico de sensores de la incubadora IncuNest. Se comunica con la placa "motherboard" por USB-CDC nativo (TinyUSB), con un protocolo propio de framing binario transportando JSON.

Este documento da la vista de conjunto. El detalle histórico completo está en `Firmware/docs/superpowers/plans/2026-06-08-sensorboard-phase1.md` (plan de implementación de la Fase 1) y `Firmware/docs/superpowers/specs/2026-07-03-sensorboard-roadmap.md` (roadmap de las 5 fases) — no se duplica aquí.

## Capa de transporte: `usb_comm`

Componente ESP-IDF que resuelve, de una vez, todo lo que no depende del contenido:

- **Framing:** `Magic(2B: 0xAB 0xCD) + Type(1B) + Length(4B LE) + Payload(NB) + CRC16(2B)`. `TYPE=0x00` es JSON (fases 1-4); `TYPE=0x01` está reservado para JPEG binario (Fase 5) desde el diseño original, sin que el framing tenga que cambiar cuando llegue.
- **CRC:** CRC16-CCITT (poly `0x1021`, init `0xFFFF`) sobre `Type+Length+Payload`. Un frame con CRC inválido se descarta en silencio, sin respuesta.
- **Dos tareas FreeRTOS**, prioridad 5: `usb_rx_task` (decodifica bytes entrantes con una máquina de estados y despacha frames completos) y `usb_tx_task` (consume una `tx_queue` de 8 items y escribe por CDC). Toda escritura saliente pasa por la cola — es el único punto de entrada para transmitir.
- **Logs redirigidos:** `esp_log_set_vprintf()` intercepta todo `ESP_LOG*` y lo emite como frame JSON `{"type":"log",...}`, sin usar la UART.
- **Orientación del conector (HW_NUM 4, ADR-0003):** política pura `sb_usb_orient_*` (`sensorBoard_usb_orient.c`, testeable sin host) evaluada por `usb_tx_task` en cada iteración: cuando ve un bus reset (`tud_event_hook_cb` cuenta `DCD_EVENT_BUS_RESET`) y el host no habla en `CONFIG_SB_USB_AUTOSWAP_TIMEOUT_MS`, hace `tud_disconnect()` → `usb_wrap_ll_phy_enable_pin_exchg()` → `tud_connect()` **una vez** y espera quieta al siguiente reset; sin host no alterna. "Host activo" = `tud_mounted() || tud_connected()`; `connected` se pone con el primer SETUP recibido (imposible con D+/D- cruzados), así que un host lento nunca provoca intercambios. La alternancia periódica se descartó en banco: engancha en fase con la recuperación del puerto de la pila host de la motherboard (ADR-0003, enmienda). Estado consultable en `status` como `sensors.usb_swap`. Todas las escrituras al CDC pasan por `cdc_writable()` (DTR visto y `tud_cdc_n_connected()`), porque DTR es pegajoso si el host desaparece sin cerrar el puerto.
- **API pública mínima:** `sensorBoard_comm_init()`, `sensorBoard_comm_send_json(const char *)`, `sensorBoard_comm_send_binary(uint8_t type, uint8_t *buf, size_t len)` (stub hasta Fase 5).

## Principio transversal: `usb_comm` es agnóstico al contenido

Ninguna fase posterior a la Fase 1 debería reabrir el framing, el CRC o las tareas RX/TX. Cada fase nueva añade **su propio componente ESP-IDF** (`components/<nombre>/`) con su(s) propia(s) tarea(s) FreeRTOS, y solo llama a `sensorBoard_comm_send_json()`/`send_binary()` con su payload. Si el diseño de una fase necesita tocar `usb_comm.c`/`sensorBoard_frame.c`/`sensorBoard_crc16.c`, es una señal de que se está saliendo de su capa (detalle y reglas concretas en el skill `arch-embedded-layering` y en `.claude/rules/embedded.md`).

El campo `sensors.<nombre>` en la respuesta de `status` (booleano de disponibilidad) es el patrón que cada fase extiende — no se inventa un formato nuevo por sensor.

## Fases del roadmap

Orden de implementación acordado: **1 → 2 → 4 → 3 → 5** (de menor a mayor complejidad). Hardware confirmado en [`hardware.md`](hardware.md).

| Fase | Qué añade | Hardware/componente |
|---|---|---|
| 1 — Comunicación USB CDC | Framing, CRC16, `usb_comm`, comando `status`, heartbeat | USB nativo (IO19/IO20) |
| 2 — Sensores ambientales | `sensor_task`, evento `sensor_data` (temp/hum/lux) | SHT40 ×3 en dos buses I2C + ALS-PT19 analógico (ADC, IO1) |
| 4 — Sensor de puerta | ISR con hand-off a tarea, eventos `door_open`/`door_closed` | Hall DRV5032 (IO47, salida digital) |
| 3 — Micrófono / dBA | `audio_task`, evento `sound_level` | ICS-41350 **PDM** (I2S en modo PDM RX; IO40 clk, IO39 data) |
| 5 — Cámara | Activa `TYPE=0x01`, implementa `send_binary()` real, comando `capture` | OV2640 DVP + SCCB en I2C principal (usa la PSRAM 8MB OPI) |

Solo la Fase 5 toca `usb_comm` de forma acotada (necesita payloads grandes en TX); el resto son estrictamente aditivas.
