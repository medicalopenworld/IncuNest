# Proposal — sb-phase5-camera

## Why

Inspección visual remota del interior de la incubadora: la motherboard solicita capturas JPEG bajo demanda. Última fase del roadmap; justifica la PSRAM de 8 MB y el `TYPE=0x01` reservado en el framing desde la Fase 1.

## What Changes

- **`usb_comm` (única fase que lo toca, acotado a TX + dispatcher):**
  - Implementar `sensorBoard_comm_send_binary()`: enmarca el payload (mismo `sb_frame_encode`, Length de 4 B ya lo soporta) en un buffer de **PSRAM con ownership transferido a la cola TX** (item de cola con puntero; `usb_tx_task` lo escribe por chunks al CDC y lo libera). El camino JSON por valor no cambia; RX no cambia (los comandos entrantes siguen siendo JSON ≤256 B).
  - **Registro de comandos** `sensorBoard_cmd_register(cmd, handler)` (tabla estática de 4, patrón del registro de status): el dispatcher despacha comandos de fases sin conocerlos. `camera_sensor` registra `capture`.
- **Componente nuevo `camera_sensor`:**
  - `esp32-camera` 2.1.7 (managed). OV2640 por DVP con el pinout de `docs/hardware.md` (XCLK IO15, PCLK IO13, VSYNC IO6, HREF IO7, Y2-Y9, PWDN IO21, RESET no conectado = -1). **SCCB comparte el bus I2C principal** (`pin_sccb_sda=-1`, `sccb_i2c_port=1`): `sccb-ng.c` obtiene el handle del bus que `env_sensors` creó — verificado en el código del componente; requiere init de cámara **después** de `env_sensors`.
  - Captura **bajo demanda** (decisión del roadmap: no continua, para no saturar el enlace): el handler de `capture` (contexto usb_rx) solo notifica a `camera_task` (prio 3, por debajo de los sensores); la tarea captura (`esp_camera_fb_get`), responde `{"type":"resp","cmd":"capture","id":N,"status":"ok","size":…}` y envía el JPEG como frame `TYPE=0x01`; libera el fb. Fallos → resp `error`, `sensors.cam:false`.
  - QVGA JPEG por defecto (Kconfig: framesize, calidad, XCLK 20 MHz), frame buffer en PSRAM.
  - Builder puro de la resp de captura, testable.

## Impact

- Affected specs: `usb-transport` (send_binary/TYPE=0x01), `command-dispatch` (registro de comandos), `camera` (nueva).
- Affected code: `components/usb_comm/` (send_binary + registro), `components/camera_sensor/` (nuevo), `main/main.c`, `test_apps/comm_test` (registro + guardas send_binary + round-trip binario), `test_apps/camera_test` (builder).
- Riesgo principal (documentado para verificación on-device): throughput real USB CDC con JPEG de 10-30 KB y backpressure; presión de PSRAM si `capture` llega en ráfaga (mitigado: una captura en vuelo, comandos re-entrantes → resp `busy`).
