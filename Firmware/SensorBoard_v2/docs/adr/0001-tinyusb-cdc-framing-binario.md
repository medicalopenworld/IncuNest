# 0001. Transporte USB: TinyUSB CDC-ACM con framing binario propio

**Estado:** Aceptada
**Fecha:** 2026-07-03

## Contexto

El SensorBoard debe enviar a la motherboard tanto telemetría pequeña y frecuente (JSON: sensores, eventos, logs) como, en la Fase 5, payloads binarios grandes (JPEG de 10-30 KB). El único enlace físico es el USB nativo del ESP32-S3 (IO19/IO20, ver `docs/hardware.md`). En un dispositivo médico, la corrupción silenciosa de datos en el enlace es inaceptable: hace falta detección de errores y resincronización, porque un stream CDC puede perder bytes en (des)conexiones.

## Decisión

Usar **TinyUSB CDC-ACM** (`esp_tinyusb`) como transporte y un **framing binario propio** — `Magic(0xAB,0xCD) + Type(1B) + Length(4B LE) + Payload + CRC16-CCITT FALSE(2B BE)`, CRC sobre Type+Length+Payload — con un byte `Type` que distingue JSON (`0x00`) de binario (`0x01`, reservado). La consola estándar queda desactivada (`CONFIG_ESP_CONSOLE_NONE`) y todo `ESP_LOG` se redirige como frames JSON.

## Consecuencias

- Las Fases 2-4 solo llaman a `sensorBoard_comm_send_json()`; la Fase 5 activa `TYPE=0x01` sin cambiar el protocolo. El framing no se vuelve a tocar.
- Un frame corrupto se descarta en silencio y el decoder se resincroniza buscando magic — sin estado colgado ni buffers desbordados.
- Se sacrifica legibilidad directa del puerto serie: inspeccionar el enlace exige decodificar frames (script Python en el plan de Fase 1).
- Sin consola de fallback: si `usb_comm` no arranca, no hay canal de diagnóstico (mitigación: interceptor de logs activo solo tras crear la cola TX; diagnóstico por JTAG si hiciera falta).

## Alternativas consideradas

- **USB-Serial-JTAG console + texto plano** — sin CRC ni framing: corrupción indetectable y sin canal binario para JPEG. Descartada.
- **CDC con JSON delimitado por líneas** — simple, pero sin integridad (CRC) y sin soporte binario eficiente (base64 inflaría el JPEG un 33%). Descartada.
- **Protocol Buffers / CBOR sobre CDC** — integridad delegada igualmente a un framing externo y añade toolchain de esquemas; el JSON plano de <256 B no lo justifica en Fase 1. Descartada (revisable si el protocolo crece).
