# Arquitectura — SensorBoard

Firmware ESP-IDF v6 (CMake, C, FreeRTOS) para el ESP32-S3-WROOM-1-N16R8 del periférico de sensores de la incubadora IncuNest. Se comunica con la placa "motherboard" por USB-CDC nativo (TinyUSB), con un protocolo propio de framing binario transportando JSON.

Este documento da la vista de conjunto. El detalle histórico completo está en `Firmware/docs/superpowers/plans/2026-06-08-sensorboard-phase1.md` (plan de implementación de la Fase 1) y `Firmware/docs/superpowers/specs/2026-07-03-sensorboard-roadmap.md` (roadmap de las 5 fases) — no se duplica aquí.

## Capa de transporte: `usb_comm`

Componente ESP-IDF que resuelve, de una vez, todo lo que no depende del contenido:

- **Framing:** `Magic(2B: 0xAB 0xCD) + Type(1B) + Length(4B LE) + Payload(NB) + CRC16(2B)`. `TYPE=0x00` es JSON (fases 1-4); `TYPE=0x01` está reservado para JPEG binario (Fase 5) desde el diseño original, sin que el framing tenga que cambiar cuando llegue.
- **CRC:** CRC16-CCITT (poly `0x1021`, init `0xFFFF`) sobre `Type+Length+Payload`. Un frame con CRC inválido se descarta en silencio, sin respuesta.
- **Dos tareas FreeRTOS**, prioridad 5: `usb_rx_task` (decodifica bytes entrantes con una máquina de estados y despacha frames completos) y `usb_tx_task` (consume una `tx_queue` de 8 items y escribe por CDC). Toda escritura saliente pasa por la cola — es el único punto de entrada para transmitir.
- **Logs redirigidos:** `esp_log_set_vprintf()` intercepta todo `ESP_LOG*` y lo emite como frame JSON `{"type":"log",...}`, sin usar la UART.
- **API pública mínima:** `sensorBoard_comm_init()`, `sensorBoard_comm_send_json(const char *)`, `sensorBoard_comm_send_binary(uint8_t type, uint8_t *buf, size_t len)` (stub hasta Fase 5).

## Principio transversal: `usb_comm` es agnóstico al contenido

Ninguna fase posterior a la Fase 1 debería reabrir el framing, el CRC o las tareas RX/TX. Cada fase nueva añade **su propio componente ESP-IDF** (`components/<nombre>/`) con su(s) propia(s) tarea(s) FreeRTOS, y solo llama a `sensorBoard_comm_send_json()`/`send_binary()` con su payload. Si el diseño de una fase necesita tocar `usb_comm.c`/`sensorBoard_frame.c`/`sensorBoard_crc16.c`, es una señal de que se está saliendo de su capa (detalle y reglas concretas en el skill `arch-embedded-layering` y en `.claude/rules/embedded.md`).

El campo `sensors.<nombre>` en la respuesta de `status` (booleano de disponibilidad) es el patrón que cada fase extiende — no se inventa un formato nuevo por sensor.

## Fases del roadmap

| Fase | Qué añade | Hardware/componente |
|---|---|---|
| 1 — Comunicación USB CDC (✅ completada) | Framing, CRC16, `usb_comm`, comando `status` | — (capa de transporte, sin sensores) |
| 2 — Sensores ambientales | `sensor_task`, evento `sensor_data` (temp/hum/lux) | SHT40 (I2C) + ALS (I2C o ADC) |
| 3 — Micrófono / dBA | `audio_task`, evento `sound_level` | Micrófono MEMS I2S |
| 4 — Sensor de puerta | ISR con hand-off a tarea, eventos `door_open`/`door_closed` | GPIO + interrupción de flanco |
| 5 — Cámara | Activa `TYPE=0x01`, implementa `send_binary()` real, comando `capture` | Módulo cámara JPEG (usa la PSRAM 8MB OPI) |

Solo la Fase 5 toca `usb_comm` de forma acotada (necesita payloads grandes en TX); el resto son estrictamente aditivas.
