# Tasks — sb-usb-autoswap-host-evidence

## 1. Red

- [x] 1.1 Nueva firma `sb_usb_orient_init(st, timeout)` / `sb_usb_orient_tick(st, host_active, bus_reset_seen, now)` en el header; `TEST_CASE`s `[orient]` reescritos (uno por Scenario) — build de `comm_test` en rojo

## 2. Green

- [x] 2.1 `sensorBoard_usb_orient.c`: estado `armed`, arma con el primer reset, no extiende, un intercambio por evidencia
- [x] 2.2 `sensorBoard_comm.c`: `tud_event_hook_cb` (contador de bus resets) y consumo en `orient_service()`

## 3. Verify

- [x] 3.1 `idf.py build` (app) e `idf.py -C test_apps/comm_test build` en verde; Scenarios cubiertos 1:1

## 4. Review

- [x] 4.1 Revisión (code + security) de la política v2

## 5. Docs / cierre

- [ ] 5.1 README, ADR-0003 (enmienda), architecture.md, CHANGELOG; archivar; retro; ESTADO.md
- [ ] 5.2 Motherboard: dejar el log de dispositivo enumerado como permanente (INFO) y retirar la instrumentación temporal

> Verificación on-target (checklist en `design.md`): banco con motherboard V18 y PC, ambas orientaciones, arranque conjunto y re-enchufe en caliente.
