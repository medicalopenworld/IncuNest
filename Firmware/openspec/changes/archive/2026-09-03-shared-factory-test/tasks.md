## 1. shared/ — tabla y codec del protocolo FTEST

Commit: `feat(shared): tabla y codec del test de fábrica (FTEST)`.

- [x] 1.1 Crear `shared/include/factory_test.h` (`extern "C"`): `FtestId`
      (30 IDs de motherBoard en el orden de design.md D10, `FTEST_MB_COUNT`,
      `FTEST_HMI_BASE = 64` con los 9 IDs del display), `FtestStatus`
      (RUNNING..CONFIRM), `FtestReject`, `FtestHmiCmd`, constantes
      `FTEST_DETAIL_MAX 40`, `FTEST_TX_LINE_MAX 64`, `FTEST_TX_QUEUE_LEN 16`,
      `FTEST_MB_RESPONSE_TIMEOUT_MS 10000`, `FTEST_STIMULUS_TIMEOUT_MS 30000`,
      `FTEST_CONFIRM_TIMEOUT_MS 60000`.
- [x] 1.2 Declarar `ftest_id_is_mb()`, `ftest_id_is_optional()`,
      `ftest_id_key()`, `ftest_format_result()`, `ftest_parse_result()`,
      `ftest_format_done()`, `ftest_parse_done()`, `ftest_format_reject()`,
      `ftest_parse_hmi_cmd()`.
- [x] 1.3 **RED** — `motherBoard/test/test_factory_test/test_factory_test.cpp`
      con los escenarios de `factory-test-protocol`: tabla (flags, claves, id
      fuera de tabla), codificar resultado, saneado y truncado de detail,
      parseo válido, cuatro descartes malformados, cierre codificar/parsear y
      malformado, cuatro comandos HMI válidos y cinco inválidos.
- [x] 1.4 **GREEN** — `shared/src/factory_test.cpp` sin Arduino ni `String`.
      `pio test -e native -f test_factory_test` en verde.

## 2. motherBoard — acumulador de resultado (lógica pura)

Commit: `feat(motherboard): acumulador puro de resultados del test de fábrica`.

- [x] 2.1 `src/modules/factory_test/ftest_summary.h/.cpp`: `ftest_summary_init()`,
      `ftest_summary_note(id, status)` (ignora RUNNING/WAIT/CONFIRM), máscaras
      `pass/fail/run` embebidas en `FtestSummary` (contadores `pass/fail/skip`
      leídos directamente de la struct), y `ftest_summary_merge_single(...)`
      para el reintento. Nota: no se añadió `ftest_summary_counts()` — el
      contrato exacto de la tarea de implementación (shared-factory-test,
      literal) solo pide `init/note/merge_single`; los contadores ya son
      campos públicos de `FtestSummary`, así que la función sería redundante.
- [x] 2.2 Añadir `+<modules/factory_test/ftest_summary.cpp>` al
      `build_src_filter` de `[env:native]`.
- [x] 2.3 **RED** — tests en `test_factory_test` para los escenarios
      "Acumulación de resultados", "RUNNING, WAIT y CONFIRM no cuentan" y
      "Reintento de un test fallido" (máscaras).
- [x] 2.4 **GREEN** — `pio test -e native` en verde.

## 3. motherBoard — `status` de la SensorBoard

Commit: `feat(motherboard): peticion status al SensorBoard y disponibilidad por recurso`.

- [x] 3.1 **RED** — en `test/test_sensorboard_json/` los escenarios
      "Decodificar la respuesta status" y "Respuesta status incompleta".
- [x] 3.2 **GREEN** — `sb_json_decode_status_resp()` en `sb_json_codec.h/.cpp`
      y `sb_json_encode_status_cmd()`.
- [x] 3.3 En `sensorboard_comm.h/.cpp`: campos `status_seen`, `sb_fw[16]`,
      `avail_sht[3]`, `avail_als`, `avail_door`, `avail_cam`, `usb_swap` en
      `SbSnapshot`; `sensorboard_status_request()` con el mismo patrón de flag
      que `capture`; envío en el tick de la tarea; decodificación de la `resp`
      con `cmd == "status"` bajo el mutex del snapshot.

## 4. motherBoard — cola TX y exposición del autotest

Commit: `feat(motherboard): cola de lineas hacia el HMI y header del autotest`.

- [x] 4.1 `CommunicationHost_Enqueue(const char *line)` en `CommTask.h/.cpp`:
      `xQueueCreate(FTEST_TX_QUEUE_LEN, FTEST_TX_LINE_MAX)`, `xQueueSend` con
      timeout 0 y `logE` si falla; drenaje con `xQueueReceive(…, 0)` al inicio
      de cada vuelta de `Communication_Task`, escribiendo en `hmiSerial`.
- [x] 4.2 `include/system/hw_selftest.h`: `extern long HW_error;`,
      `bool actuatorsTest();`, `void testStandByCurrent();` y sustituir la
      declaración implícita de `HW_error` en `CommTask.cpp` por el include.
- [x] 4.3 `volatile bool g_factoryTestActive` (definido en
      `factory_test_task.cpp`, declarado en `hw_selftest.h`); retorno temprano
      en `PIDHandler()` (`PID.cpp`) y en `turnFans()` (`Actuators.cpp`) cuando
      está a true.
- [x] 4.4 `preferences_keys.h`: `NS_FTEST = "mb_ftest"`, claves `epoch`,
      `pass`, `fail`, `run`, `fw`, `sb_fw`, `probe` (≤ 15 caracteres).

## 5. motherBoard — módulo factory_test (tarea y tests de hardware)

Commit: `feat(motherboard): test de fabrica por comando del HMI`.

- [x] 5.1 `src/modules/factory_test/factory_test.h`: `bool factoryTestStart(void)`,
      `bool factoryTestRunSingle(FtestId)`, `void factoryTestAbort(void)`,
      `void factoryTestConfirm(FtestId, bool)`, `bool factoryTestRunning(void)`,
      `FtestReject factoryTestPrecheck(FtestId)`; constantes de umbral
      (`FTEST_SB_SPREAD_MAX_C 1.0f`, `FTEST_SB_VS_EXT_MAX_C 3.0f`,
      `FTEST_BUZZER_DBA_DELTA 6.0f`, `FTEST_HUMID_MIN_MA 20.0f`,
      `FTEST_HEAP_MIN_BYTES 40*1024`).
- [x] 5.2 `factory_test_task.cpp`: creación de la tarea `FTEST` (8192 B, prio
      3, core 1), `enter_safe_state()` / `restore()` según design D4 (con
      guardado de `in3.alarmsEnabled`), bucle sobre la tabla o test único,
      emisión RUNNING/resultado por `CommunicationHost_Enqueue` con
      `ftest_format_result`, `FTEST_DONE`, semáforo de CONFIRM (rechaza id
      distinto con log), flag de ABORT, persistencia NVS (`mb_ftest`) con
      `ftest_summary`, y `vTaskDelete(NULL)`.
- [x] 5.3 `factory_test_hw.cpp`: cuerpos de los 30 tests (una función por ID,
      tabla `{id, fn}`) con los criterios exactos de design D10 y de la spec
      `mb-factory-test`. SKIP en cascada: `SB_*` si `SENSOR_SRC` falló;
      `GSM_SIGNAL`/`GSM_NET` si `GSM_SIM` falló (`detail = sin sim`);
      `FAN_RPM` si `ACTUATORS` falló.
- [x] 5.4 AFE_SPI: `afe.getTimingConfig()` (lee los registros de timing por
      SPI bajo `_spi_mutex`; devuelve ceros si no inicializada): PASS si
      `t1 < t2 && t2 != 0 && t2 != 0xFFFF`; detail con `t2` y el DIAG de
      `afe.runAfeDiagnostics()` en hexadecimal (informativo, ~10 ms de SPI).
      No hay `readRegister` público en `incunest_afe4490` v0.81.0.
- [x] 5.5 SYSINFO: `ESP.getFlashChipSize()`, `esp_get_free_heap_size()`,
      `esp_reset_reason()`, `boots` de NVS `diag`. MAC con `esp_read_mac()`
      en el detail.
- [x] 5.6 `CommTask.cpp::parse_line()`: rama `HMI,FTEST,` antes del catch-all
      `HMI,`: `ftest_parse_hmi_cmd()`; START/RUN → `factoryTestPrecheck()` y
      `CTRL,FTEST_REJECT` o arranque; ABORT → `factoryTestAbort()`; CONFIRM →
      `factoryTestConfirm()`; malformado → descarte con `logE`.
- [x] 5.7 `pio run -e IncuNest_V18` en verde (PowerShell, log en scratchpad).

## 6. Display_HMI — módulo FactoryTest

Commit: `feat(hmi): pantalla de test de fabrica con tests locales y remotos`.

- [x] 6.1 `include/ui/FactoryTest.h` + `src/ui/FactoryTest.cpp` (molde
      `AlarmCenter`): overlay en `lv_layer_top()`, lista de filas (nombre,
      estado, detail), zona de instrucción con Sí / No, botones Reintentar y
      Salir, `TXT(es,en,fr)` local y `FactoryTest_ApplyLanguage()`.
- [x] 6.2 Máquina de estados por polling (`enum class Step`, `s_deadlineMs`):
      tests locales en el orden de la spec `hmi-factory-test`, luego
      `Communication_SendFtestStart()`, consumo del anillo de resultados,
      plazo de 10 s "MB sin soporte", resumen, reintento (local o `RUN,id`),
      Salir (`ABORT` si hay batería en curso, `lv_scr_load(ui_ScreenMain)`).
- [x] 6.3 Tests locales: SYSINFO, I2C (`Wire.beginTransmission/endTransmission`
      fuera del lock), PANEL (5 rectángulos hijos del overlay, 800 ms cada
      uno, luego pregunta), TOUCH (5 objetivos ≤ 40 px, 20 s cada uno),
      BUZZER y SPEAKER (300 ms no bloqueantes con arbitraje frente a
      `click_beep` y `link_audio_service`, luego pregunta), WIFI (MAC;
      conectado → RSSI; si no `scanNetworks(true)` y `scanComplete()` por
      polling), NVS (`hmi_ftest/probe`), LINK.
- [x] 6.4 Persistencia en `hmi_ftest` al mostrar el resumen (fuera de
      `LVGL_Lock()`), claves en `EEPROM_defines.h`.
- [x] 6.5 `CommTask.cpp`: `Communication_SendFtestStart/Run/Abort/Confirm()`;
      en `parse_message()` ramas `CTRL,FTEST,` (→ `ftest_parse_result()` a un
      anillo de 8 con `portMUX_TYPE` + `g_pendingFtest`), `CTRL,FTEST_DONE,` y
      `CTRL,FTEST_REJECT,`; API `FactoryTest_TakeEvent()` para drenarlo desde
      `_Poll`. Declaraciones en `CommTask.h`.
- [x] 6.6 `ElementsCreation.cpp::ui_ScreenIntro_screen_init()`: botón "TEST
      FÁBRICA" abajo a la izquierda (`montserrat_16`, `TOUCH_EXT_MEDIUM`) con
      callback que pone `g_factoryTestRequested` y llama a `FactoryTest_Open()`.
- [x] 6.7 `UITask.cpp`: `FactoryTest_Init()` junto a `AlarmCenter_Init()`,
      `FactoryTest_Poll()` junto a `TelemetryHistory_Poll()`,
      `intro_timer_cb()` no navega si `g_factoryTestRequested`,
      `inactivity_timer_cb()` no bloquea si `FactoryTest_IsOpen()`, enganche
      en `UI_ApplyLanguage()`.
- [x] 6.8 `pio run -e main` en verde (PowerShell, log en scratchpad).

## 7. Documentación

Commit: `docs: protocolo FTEST y test de fabrica`.

- [x] 7.1 `Firmware/PROTOCOL.md` → v2.3.0: sección `FTEST` con los siete
      mensajes, la tabla de IDs y estados, la regla de descarte y la nota de
      compatibilidad.
- [x] 7.2 `Firmware/docs/hmi.md`: el botón del splash, el flujo de la pantalla
      y las instrucciones al operario. `Firmware/docs/hardware.md`: qué
      verifica cada test de la motherBoard y qué queda fuera y por qué.

## 8. Verificación manual (sin entorno de test en hardware)

- [ ] 8.1 **Manual** — banco con motherBoard V18 + SensorBoard + HMI: batería
      completa, comprobar orden, plazos, WAIT de puerta y luz, CONFIRM de
      zumbador sin micrófono (SensorBoard desconectada), resumen y NVS.
- [ ] 8.2 **Manual** — REJECT con control activo; REJECT por batería en curso;
      ABORT a mitad de `ACTUATORS` y comprobar PWM a 0 y alarmas restauradas.
- [ ] 8.3 **Manual** — unidad forzada a modo I2C2: `SENSOR_SRC` FAIL y `SB_*`
      SKIP.
- [ ] 8.4 **Manual** — HMI con motherBoard antigua: "MB sin soporte" a los 10 s.
- [ ] 8.5 **Manual** — resultados durante `CTRL,PPG` a 25 Hz sin líneas
      corruptas.

## 9. Evidencia del stage Verify (2026-09-03, sin hardware conectado)

Ejecutado por el orquestador sobre HEAD `6ed12f2` + docs, en el worktree
`Firmware/.worktrees/factory-test`, con PowerShell:

| Comando | Resultado |
|---|---|
| `motherBoard: pio run -e IncuNest_V18` | SUCCESS, RAM 26.3 % (86040 B), Flash 54.4 % (1496253 B), 0 warnings |
| `motherBoard: pio test -e native` | 20 suites PASSED (287 casos), incl. `test_factory_test` (18) y los 3 escenarios nuevos de `test_sensorboard_json` |
| `Display_HMI: pio run -e main` | SUCCESS, RAM 38.6 % (126420 B), Flash 78.5 % (2470152 B), 0 warnings |

`test_sensorboard_frame` sale **ERRORED** (código 0xC0000139, el proceso no
arranca por una DLL) tanto aquí como en `dev` sin este cambio: es un problema
del entorno de esta máquina, no de la feature. Queda fuera de este change.

**Tras el review** (HEAD `25c4ceb`, misma máquina): `IncuNest_V18` SUCCESS,
RAM 26.3 %, Flash 54.4 % (1497601 B), 0 warnings; `main` SUCCESS, RAM 38.6 %,
Flash 78.6 % (2471324 B), 0 warnings; `native` 20 suites PASSED (288 casos, uno
más por la cota de dígitos del codec), `test_sensorboard_frame` ERRORED por el
mismo problema de DLL preexistente.

Cobertura de escenarios: todos los marcados `[env:native]` en las tres specs
tienen `TEST_CASE` (protocolo: tabla, codec, descartes; mb: acumulación,
no-finales, reintento, `status` de la SensorBoard). Los marcados como manuales
son la sección 8, pendiente de banco.
