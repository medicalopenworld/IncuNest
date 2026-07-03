# Tasks — sb-phase5-camera

## 1. Red

- [x] 1.1 Stubs: `sensorBoard_cmd_register` + `send_binary` real (firma), builder `sb_cam_build_capture_resp` + tests en `comm_test` (registro, guardas de send_binary, round-trip binario 4KB) y `camera_test` (builders)

## 2. Green

- [x] 2.1 Registro de comandos en el dispatcher (tabla estática de 4)
- [x] 2.2 `send_binary`: frame en PSRAM con ownership a la cola (item con puntero), TX por chunks + free garantizado
- [x] 2.3 `camera_sensor`: init esp32-camera (pinout hardware.md, `sccb_i2c_port=1` tras verificar el bus), `camera_task` prio 3, handler `capture` (notify + busy), resp + JPEG `TYPE=0x01`, `sensors.cam`
- [x] 2.4 `main.c`: init no fatal tras `env_sensors`

## 3. Verify

- [x] 3.1 Builds en verde (app + comm_test + camera_test); Scenarios cubiertos

## 4. Review

- [x] 4.1 code-reviewer + security-reviewer; hallazgos resueltos

## 5. Docs / cierre

- [x] 5.1 README/CHANGELOG/architecture; archivar; retro; checkbox EPIC-001 (última fase)

> Verificación manual on-target pendiente: captura real con OV2640 (resp + JPEG decodificable), flood de `capture` sin pérdida de heartbeat/telemetría, throughput real del enlace con JPEG QVGA, y convivencia SCCB↔SHT40 en el bus compartido.
