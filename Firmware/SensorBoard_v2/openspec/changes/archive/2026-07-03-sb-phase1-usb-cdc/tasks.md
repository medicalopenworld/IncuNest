# Tasks — sb-phase1-usb-cdc

## 1. Red (tests primero)

- [x] 1.1 Scaffold ESP-IDF compilable: CMake raíz, `sdkconfig.defaults`, `partitions.csv`, `main/` stub, `components/usb_comm/` con stubs y headers (`sensorBoard_comm.h`, `sensorBoard_comm_protocol.h`, `sensorBoard_crc16.h`, `sensorBoard_frame.h`)
- [x] 1.2 Test app `test_apps/comm_test/` con `TEST_CASE`s Unity que cubren cada Scenario de `usb-transport` (CRC16, encoder, decoder) — fallan sobre los stubs

## 2. Green

- [x] 2.1 Implementar `sensorBoard_crc16.c` (CCITT FALSE, incremental + bloque)
- [x] 2.2 Implementar `sb_frame_encode`
- [x] 2.3 Implementar decoder (máquina de estados con resync y rechazo de longitud excesiva)
- [x] 2.4 Implementar `sensorBoard_comm.c`: init TinyUSB CDC, tareas RX/TX, cola, `send_json`, `send_binary` stub, interceptor de logs
- [x] 2.5 Implementar `sensorBoard_cmd_handler.c`: dispatcher cJSON, `status`, desconocido, malformado
- [x] 2.6 `main.c` final con heartbeat de 30 s

## 3. Verify

- [x] 3.1 `idf.py build` de la app principal sin errores
- [x] 3.2 `idf.py build` del test app sin errores
- [x] 3.3 Cada Scenario del spec tiene su `TEST_CASE` (o cobertura manual documentada para los que exigen hardware: status/heartbeat on-device)

> Cobertura manual pendiente de hardware (checklist del plan de junio, paso "Task 11/Step 4"): respuesta `status` ok, resp de error para cmd desconocido, logs como frames, CRC corrupto sin respuesta y heartbeat cada 30 s se verifican con `idf.py -p COMx flash` + script Python decodificador. Los Scenarios de `send_json` oversize y "log antes de init" quedan cubiertos por guardas testeadas indirectamente (`send_json` sin init → `INVALID_STATE`) y por diseño (interceptor descarta con cola NULL).

## 4. Review

- [x] 4.1 code-reviewer y security-reviewer en paralelo; hallazgos resueltos

## 5. Docs / cierre

- [x] 5.1 README del proyecto + CHANGELOG + `docs/architecture.md` actualizados
- [x] 5.2 Archivar change, retro y checkbox de EPIC-001
