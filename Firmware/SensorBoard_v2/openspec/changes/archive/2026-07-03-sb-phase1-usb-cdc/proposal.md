# Proposal — sb-phase1-usb-cdc

## Why

El SensorBoard no tiene firmware: la Fase 1 que el roadmap (`2026-07-03-sensorboard-roadmap.md`) describía como "completada" nunca se implementó (confirmado por Pablo, 2026-07-03). Todo el roadmap de sensores (Fases 2-5) depende de esta capa de transporte: sin `usb_comm` no hay forma de publicar lecturas ni recibir comandos desde la motherboard.

## What Changes

- Proyecto ESP-IDF v6 nuevo en `Firmware/SensorBoard_v2/` (CMake raíz, `sdkconfig.defaults` para ESP32-S3-WROOM-1-N16R8: flash 16MB, PSRAM OPI 8MB, `partitions.csv` con OTA).
- Componente `usb_comm`: transporte USB-CDC (TinyUSB) agnóstico al contenido:
  - Framing binario `Magic(0xAB,0xCD) + Type(1B) + Length(4B LE) + Payload + CRC16-CCITT(2B BE)`; CRC sobre Type+Length+Payload. `TYPE=0x00` JSON, `TYPE=0x01` reservado (JPEG, Fase 5).
  - Encoder y decoder por máquina de estados (resync ante basura, descarte silencioso de CRC inválido, rechazo de longitudes mayores que el buffer).
  - Tareas FreeRTOS `usb_rx_task`/`usb_tx_task` (prioridad 5) + `tx_queue` (8 items) como único punto de emisión.
  - `esp_log_set_vprintf()` → todo `ESP_LOG` sale como frames JSON `{"type":"log",...}`.
  - API pública: `sensorBoard_comm_init()`, `sensorBoard_comm_send_json()`, `sensorBoard_comm_send_binary()` (stub `ESP_ERR_NOT_SUPPORTED` hasta Fase 5).
- Handler de comandos: `status` → resp con `device`/`fw`/`uptime`; comando desconocido → `status:"error"`.
- Heartbeat `{"type":"event","cmd":"heartbeat"}` cada 30 s desde `app_main`.
- Test app Unity `test_apps/comm_test/` con los escenarios del spec (CRC16, encoder, decoder, dispatcher).

## Impact

- Affected specs: `usb-transport` (nueva capability), `command-dispatch` (nueva capability).
- Affected code: todo nuevo — `CMakeLists.txt`, `sdkconfig.defaults`, `partitions.csv`, `main/`, `components/usb_comm/`, `test_apps/comm_test/`.
- Base para Fases 2-5: ninguna fase posterior reabre `usb_comm` (salvo Fase 5, acotada a `send_binary`/TX).
- Verificación automatizable: `idf.py build` (app + test app). Ejecución Unity y criterios de integración en hardware real quedan como checklist manual documentada.
