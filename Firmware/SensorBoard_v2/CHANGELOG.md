# Changelog

Todos los cambios notables de este proyecto se documentan en este archivo.

El formato sigue [Keep a Changelog](https://keepachangelog.com/es-ES/1.1.0/) y este proyecto usa [Semantic Versioning](https://semver.org/lang/es/). La versión de referencia es `SB_PROTO_FW_VERSION` (definida en `sensorBoard_comm_protocol.h`), no un `package.json`.

## [Unreleased]

### Added

- **Tolerancia a la orientación del conector USB — HW_NUM 4 (`usb_comm`)**: si no hay host activo (ni SETUP ni `SET_CONFIGURATION`) en `CONFIG_SB_USB_AUTOSWAP_TIMEOUT_MS` (2 s, mín. 1,5 s), se intercambian D+/D- en el PHY del ESP32-S3 (`exchg_pins`) con detach/attach de 250 ms y se sigue alternando hasta enumerar; política pura `sb_usb_orient_*` con 6 `TEST_CASE` en `comm_test`, Kconfig `SB_USB_AUTOSWAP`, log retenido por intercambio y estado `sensors.usb_swap` en `status` (ADR-0003). La V5 corrige el conector en hardware; el mecanismo queda activo e inocuo. El bootloader ROM no aplica el intercambio: flashear con el cable invertido sigue requiriendo girarlo.

### Fixed

- **`usb_comm`: DTR pegajoso.** Las escrituras al CDC exigen ahora DTR **y** `tud_cdc_n_connected()`: si el host desaparece sin cerrar el puerto (sin VBUS sensing), los frames vuelven a la retención de arranque en vez de perderse en un FIFO muerto.
- **Fase 5 — Cámara (`camera_sensor`)**: OV2640 (esp32-camera, QVGA JPEG) con SCCB compartiendo el bus I2C principal, comando `capture` bajo demanda (registro de comandos por componente en `usb_comm`), `sensorBoard_comm_send_binary()` real con frames `TYPE=0x01` en PSRAM y ownership en la cola TX, y `sensors.cam` en `status`.
- **Fase 3 — Micrófono (`mic_sensor`)**: ICS-41350 por I2S en modo PDM RX (16 kHz/16 bit), RMS→dB con offset de calibración Kconfig y gate de plausibilidad [0,140] dB, evento `sound_level` periódico y `sensors.mic` en `status`. El valor `dba` aún no lleva ponderación A (diferido hasta calibrar con sonómetro).
- **Fase 4 — Sensor de puerta (`door_sensor`)**: hall DRV5032FB en IO47 con ISR de solo hand-off, debounce configurable (≥ muestreo interno de 5 Hz del sensor), eventos `door_open`/`door_closed` solo en cambio estable, estado inicial publicado al arrancar y `sensors.door` en `status`.
- **Fase 2 — Sensores ambientales (`env_sensors`)**: 3× SHT40 en dos buses I2C con CRC-8 y conversión datasheet, ALS-PT19 por ADC (conversión sin calibrar, `CONFIG_SB_ALS_UV_PER_LUX`), `sensor_task` con polling configurable, evento `sensor_data` con redundancia posicional y `null` por sensor caído (ADR-0002), y campo `sensors{}` en la resp de `status` vía registro agnóstico en `usb_comm`.
- **Fase 1 — Transporte USB CDC (`usb_comm`)**: proyecto ESP-IDF v6 (ESP32-S3 N16R8, flash 16MB OTA, PSRAM OPI), framing binario `Magic+Type+Length+Payload+CRC16-CCITT`, tareas FreeRTOS RX/TX con `tx_queue`, logs `ESP_LOG` como frames JSON, comando `status`, heartbeat 30 s y test app Unity `comm_test` (ADR-0001).
- Documento de hardware `docs/hardware.md` con el pinout completo confirmado (SHT40 ×3, ALS-PT19 analógico, mic PDM ICS-41350, hall DRV5032, OV2640).
- Framework de agentes Genesis adaptado a ESP-IDF v6/C/FreeRTOS (agentes, rules, hooks, skills y documentación mínima: `ESTADO.md`, `docs/adr/`, `docs/retro/`, `docs/epics/`, `docs/architecture.md`, `docs/blueprint/`).

<!--
Plantilla de entrada al cortar una release (mover [Unreleased] a una sección versionada):

## [X.Y.Z] - AAAA-MM-DD

### Added
### Changed
### Fixed
### Removed
-->
