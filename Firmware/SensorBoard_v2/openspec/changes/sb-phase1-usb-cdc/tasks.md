# Tasks — sb-phase1-usb-cdc

## 1. Red (tests primero)

- [x] 1.1 Scaffold ESP-IDF compilable: CMake raíz, `sdkconfig.defaults`, `partitions.csv`, `main/` stub, `components/usb_comm/` con stubs y headers (`sensorBoard_comm.h`, `sensorBoard_comm_protocol.h`, `sensorBoard_crc16.h`, `sensorBoard_frame.h`)
- [x] 1.2 Test app `test_apps/comm_test/` con `TEST_CASE`s Unity que cubren cada Scenario de `usb-transport` (CRC16, encoder, decoder) — fallan sobre los stubs

## 2. Green

- [ ] 2.1 Implementar `sensorBoard_crc16.c` (CCITT FALSE, incremental + bloque)
- [ ] 2.2 Implementar `sb_frame_encode`
- [ ] 2.3 Implementar decoder (máquina de estados con resync y rechazo de longitud excesiva)
- [ ] 2.4 Implementar `sensorBoard_comm.c`: init TinyUSB CDC, tareas RX/TX, cola, `send_json`, `send_binary` stub, interceptor de logs
- [ ] 2.5 Implementar `sensorBoard_cmd_handler.c`: dispatcher cJSON, `status`, desconocido, malformado
- [ ] 2.6 `main.c` final con heartbeat de 30 s

## 3. Verify

- [ ] 3.1 `idf.py build` de la app principal sin errores
- [ ] 3.2 `idf.py build` del test app sin errores
- [ ] 3.3 Cada Scenario del spec tiene su `TEST_CASE` (o cobertura manual documentada para los que exigen hardware: status/heartbeat on-device)

## 4. Review

- [ ] 4.1 code-reviewer y security-reviewer en paralelo; hallazgos resueltos

## 5. Docs / cierre

- [ ] 5.1 README del proyecto + CHANGELOG + `docs/architecture.md` actualizados
- [ ] 5.2 Archivar change, retro y checkbox de EPIC-001
