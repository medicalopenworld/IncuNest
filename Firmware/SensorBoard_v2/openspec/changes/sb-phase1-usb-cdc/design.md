# Design — sb-phase1-usb-cdc

> Diseño detallado preexistente: `2026-06-08-sensorboard-phase1-design.md` y plan de implementación `2026-06-08-sensorboard-phase1.md` (raíz del proyecto). Este documento fija las decisiones vinculantes y las desviaciones respecto a ese plan.

## Decisiones

1. **TinyUSB CDC-ACM (`esp_tinyusb`), no USB-Serial-JTAG.** El enlace con la motherboard es el USB nativo del S3 (IO19/IO20, ver `docs/hardware.md`); TinyUSB da control del endpoint CDC y callbacks de RX/line-state. Consola UART/USB desactivada en la app principal (`CONFIG_ESP_CONSOLE_NONE`): el único canal es el protocolo enmarcado. ADR-0001.
2. **CRC16-CCITT FALSE** (poly 0x1021, init 0xFFFF) sobre Type+Length+Payload, transmitido big-endian. Vector de verificación: `"123456789"` → `0x29B1`.
3. **Decoder como máquina de estados byte a byte** con buffer fijo de 256 B (`SB_PROTO_MAX_JSON_PAYLOAD`): sin heap en el camino RX, resync automático buscando magic, longitudes > buffer descartan el frame.
4. **TX por cola de items por valor** (frame ya codificado, ≤265 B): sin ownership compartido ni heap. `send_json` codifica y encola; solo `usb_tx_task` escribe en el endpoint.
5. **Estructura**: raíz del proyecto IDF = `Firmware/SensorBoard_v2/` (donde vive este framework), no un subdirectorio `SensorBoard/` como decía el plan de junio — así `idf.py build` corre desde la raíz que documenta `CLAUDE.md`.
6. **Dependencias**: `esp_tinyusb` como managed component (idf_component.yml); cJSON solo en el handler de comandos (parseo de entrada); la emisión JSON usa `snprintf` (payloads pequeños y planos, sin heap).

## Riesgos

- `CONFIG_ESP_CONSOLE_NONE` + interceptor de logs: si `usb_comm` falla en el arranque no hay canal de diagnóstico — mitigado dejando el interceptor activo solo tras crear la cola TX.
- Versiones IDF v6: nombres de Kconfig/API de `esp_tinyusb` pueden diferir del plan de junio; se ajustan en green con el build como árbitro.
