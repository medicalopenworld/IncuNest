# Tasks — sb-usb-pin-autoswap

## 1. Red

- [ ] 1.1 Header `sensorBoard_usb_orient.h` + `TEST_CASE`s `[orient]` en `comm_test` (uno por Scenario) — build de `comm_test` en rojo (símbolos sin definir)

## 2. Green

- [ ] 2.1 `sensorBoard_usb_orient.c` (política pura)
- [ ] 2.2 Integración en `sensorBoard_comm.c`: init tras TinyUSB, tick en `usb_tx_task`, secuencia disconnect→exchg→connect, `ESP_LOGW`
- [ ] 2.3 Kconfig `SB_USB_AUTOSWAP` / `SB_USB_AUTOSWAP_TIMEOUT_MS`; CMake (`PRIV_REQUIRES esp_hal_usb`)

## 3. Verify

- [ ] 3.1 `idf.py build` (app) y `idf.py -C test_apps/comm_test build` en verde; Scenarios cubiertos 1:1

## 4. Review

- [ ] 4.1 code-reviewer + security-reviewer; hallazgos resueltos

## 5. Docs / cierre

- [ ] 5.1 README (tolerancia a orientación + limitación del bootloader), CHANGELOG, architecture.md, hardware.md; ADR-0003; archivar; retro; ESTADO.md

> Verificación manual on-target pendiente (checklist en `design.md`): orientación correcta, invertida, re-enchufe en caliente y arranque sin host.
