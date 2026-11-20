## 1. Protocolo compartido
- [x] 1.1 `shared/include/service_hours_proto.h` + `shared/src/service_hours_proto.cpp`: ids de actuador, build/parse de `CTRL,SERVICE`, build/parse de `HMI,SERVICE_DONE`, reconocimiento de `HMI,SERVICE_REQ`.

## 2. Tests en rojo
- [x] 2.1 `motherBoard/test/test_service_proto` y `test_service_calc` (horas desde servicio, vencimiento, informe).
- [x] 2.2 `Display_HMI/test/test_service_state` (estado, vencimiento, snooze, parseo de línea mala) y `test_service_proto`.

## 3. motherBoard
- [x] 3.1 `config/service_thresholds.h` (umbrales provisionales) y claves NVS `svc_b_*` / `svc_e_*`.
- [x] 3.2 `modules/service/service_calc.{h,cpp}` (puro) y `service_hours.{h,cpp}` (NVS + `in3`).
- [x] 3.3 `CommTask.cpp`: `HMI,SERVICE_REQ`, `HMI,SERVICE_DONE`, envío al `UI_READY` y cada hora.
- [x] 3.4 Telemetría `svc_*_h` en DIAG (GPRS y WiFi); `check_transport_matrix.py` sin superar 112.

## 4. Display_HMI
- [x] 4.1 `modules/service/service_state.{h,cpp}` (puro).
- [x] 4.2 `CommTask.cpp`: parseo de `CTRL,SERVICE`, `HMI,SERVICE_REQ` y `HMI,SERVICE_DONE`.
- [x] 4.3 `MaintenanceDialog.cpp`: vista de servicio, aviso suave al desbloquear, confirmación en dos pasos.
- [x] 4.4 Textos en ES/EN/FR/PT.

## 5. Verificación
- [x] 5.1 `pio test -e native` en motherBoard y Display_HMI.
- [x] 5.2 `pio run -e IncuNest_V18` (motherBoard) y `pio run -e main` (Display_HMI).
- [ ] 5.3 Banco (PENDIENTE, sin hardware en esta rama): checklist en `docs/hmi.md` seccion 9.

## 6. Documentación
- [x] 6.1 `PROTOCOL.md`, `docs/hmi.md`, `docs/transport_matrix.md`, `docs/thingsboard_dashboards.md`.
