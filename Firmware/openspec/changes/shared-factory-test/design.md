## Context

Estado de partida, verificado en el árbol (rama `feat/factory-test`, base
`feat/sb-usb-pin-autoswap` + `dev`):

- **motherBoard ya tiene un autotest de arranque** en
  `src/system/initHardware.cpp`: `testStandByCurrent()` (:498, hasta 2 s),
  `testBuzzer()` (:523, en HW ≥ 17 solo pita 300 ms y no puede fallar),
  `testSensors()` (:450), `actuatorsTest()` (:623, 11–16 s: calefactor al 100 %,
  fototerapia al 10 %, ventilador, RPM, humidificador con `USB_FAULT`). Se llaman
  desde `initHardware()` **antes** de crear ninguna tarea, así que asumen que
  nadie más toca los canales PWM. Reportan en el bitmask global `HW_error`
  (`initHardware.cpp:192`, códigos `HW_ERROR_ID` en `include/main.h:172-195`),
  que se copia a `in3.HW_test_error_code` y solo sale por ThingsBoard.
- **En runtime hay tres escritores concurrentes de actuadores**: `PIDHandler()`
  cada 1 ms desde `Communication_Receiver` (`main.cpp:361`), `turnFans()` en cada
  trama HMI (`CommTask.cpp:287`, reescribe `ACTUATORS_EN`), y `securityCheck()`
  desde `security_Task`, que levantaría `ALARM_HEATER_FAULT` /
  `ALARM_FAN_FAILURE` y apagaría el humidificador por `checkUsbFault()`. La única
  inhibición existente es `in3.alarmsEnabled` (`main.cpp:347`). No hay ninguna
  función de "estado seguro".
- **La salida hacia el HMI no está serializada.** `CommunicationHost_Send()`
  (`CommTask.cpp:985`) es `hmiSerial.print()` sin mutex, `hmiSerial` es
  file-static, y `Communication_Task` emite `CTRL,PPG` a 25 Hz. Una segunda tarea
  escribiendo líneas se entrelazaría con esa y el HMI descartaría ambas.
- **El único watchdog** es `esp_task_wdt` sobre `loopTask` (75 s). Una tarea
  nueva no está vigilada, así que una batería de varios minutos es segura.
- **El módem no tiene PWRKEY cableado** (`board.h:40,82`): un reset por
  software lo deja muerto hasta ciclo de alimentación. El puerto `Serial2` lo
  usa solo `GPRS_Task`, sin mutex. El estado ya recogido por esa tarea
  (`GPRS.powerUp`, `GPRS.CCID`, `GPRS.CSQ`, `GPRS.post`,
  `GPRSIsConnectedToServer()`) es legible desde cualquier tarea.
- **WiFi**: `WIFIIsConnected()` / `WIFIIsConnectedToServer()` son públicas. Las
  credenciales por defecto (`in3wifi`) vienen de `Credentials.h`, no versionado.
  El provisionamiento en ThingsBoard exige `in3.serialNumber != 0`.
- **SensorBoard**: `sensorboard_comm` expone `sensorboard_get_snapshot()` con
  temperatura ×3, humedad ×3, lux, dBA y puerta con marcas de frescura, y la
  captura asíncrona `sensorboard_capture_request()` / `_take()` / `_free()`. **No
  existe petición `status`**, y el snapshot no lleva ni versión de firmware ni
  disponibilidad por recurso. La tarea del módulo es la única que transmite por
  el handle USB.
- **La fuente de sensores** se decide una vez por arranque en
  `sensor_source.h`: si el bus I2C2 responde → equipo antiguo, el USB no se
  levanta jamás.
- **Display_HMI**: el splash `ui_ScreenIntro` no admite ningún toque; lo cierra
  `intro_timer_cb()` (`UITask.cpp:284-297`, mínimo 5 s, máximo 15 s) y a los
  20 s `inactivity_timer_cb()` fuerza `ui_ScreenLock`. Los módulos de UI hechos
  a mano (`AlarmCenter`, `BabyWizard`…) son overlays en `lv_layer_top()` con
  API `_Init/_Open/_Close/_IsOpen/_Poll` y máquina de estados por polling. LVGL
  corre solo en `UI_Task`; I2C y NVS se hacen fuera de `LVGL_Lock()`. El
  display no tiene entorno de test.
- **Lógica pura testeable** en motherBoard: `[env:native]` compila una lista
  blanca de `src/modules/**` más todo `shared/src` (`pre_native.py`). El patrón
  canónico es `modules/control/alarm_test.h/.cpp`: `extern "C"`, tiempo
  inyectado como `uint32_t now_ms`, cero I/O.

## Goals / Non-Goals

**Goals**
- Que un operario de montaje, con solo la pantalla táctil, pueda lanzar una
  batería que cubra todo el hardware de la unidad (display, motherBoard y
  SensorBoard) y ver PASA / FALLA por test.
- Que los tests que encienden actuadores **nunca** puedan ejecutarse con un
  control térmico activo ni pisar una alarma real.
- Que la parte del contrato entre placas (identificadores, estados, formato de
  línea) sea **una sola tabla** compilada en ambas y verificada por tests
  nativos.
- Que el resultado quede persistido en cada placa con la identidad de la
  unidad y las versiones de firmware.
- Que un test fallido se pueda reintentar solo.

**Non-Goals**
- No se reescribe `actuatorsTest()`: se reutiliza tal cual, envuelto en la
  inhibición nueva. Rehacerlo es riesgo sin beneficio.
- No se añade un modo de test a la SensorBoard: `status` + los eventos que ya
  publica bastan. Cambiar su firmware queda fuera de este change (otro root
  OpenSpec).
- No se ejecutan comandos AT desde el test: el test GSM es **pasivo**, lee el
  estado que `GPRS_Task` ya recoge. Evita colisionar con el único dueño del
  puerto y el riesgo de dejar el módem apagado sin PWRKEY.
- No se confirma la telemetría a ThingsBoard con un round-trip de atributos:
  se toma como evidencia la sesión MQTT aceptada con el token provisionado
  (CONNACK). El PUBACK no existe con QoS 0.
- No se testea el TCA9535, el encoder, el consumo del zumbador (HW ≤ 16), el
  HW_NUM ni el interruptor ON_OFF. No se cicla el backlight del display.
- No se añade CRC ni framing nuevo al protocolo (regla de `security.md`).

## Decisions

### D1. Una tabla compartida en `shared/include/factory_test.h`

Los identificadores (`FtestId`), los estados (`FtestStatus`), los motivos de
rechazo y el codificador / parser de las líneas `FTEST` viven en `shared/` y
se compilan en ambas placas. La alternativa (cada placa con su enum y su
`sscanf`) es la misma fuente de desincronización que ya costó `humidityMin` y
`cause` en `PROFILE_*`.

- IDs de motherBoard en `0..FTEST_MB_COUNT-1` (caben en un `uint32_t` de
  máscara para NVS); IDs del display a partir de `FTEST_HMI_BASE = 64`, nunca
  viajan por el cable.
- Cada ID tiene un flag `optional` (SKIP en vez de FAIL cuando falta el
  entorno: AP WiFi, SIM, sonda SpO2) y una clave ASCII corta para logs.
- Codec puro: `ftest_format_result()`, `ftest_parse_result()`,
  `ftest_format_done()`, `ftest_parse_done()`, `ftest_parse_hmi_cmd()`. El
  campo `detail` es el **último** de la línea y se sanea sustituyendo comas y
  saltos de línea; longitud máxima `FTEST_DETAIL_MAX = 40`. Todo en
  `shared/src/factory_test.cpp`, con Unity en
  `motherBoard/test/test_factory_test/` (`pre_native.py` ya compila `shared/src`).

### D2. Mensajes propios `CTRL,FTEST*`, no campos en `CTRL,STATE`

`CTRL,STATE` va a 1 Hz con `msg[192]` y ya lleva 22 campos. Meter ahí 30
resultados obligaría a crecer el buffer y a repetir a 1 Hz información que
cambia una vez por test (`known_issues.md` #2). Se emite **una línea por
evento de test**, y una de cierre:

```
HMI,FTEST,START                      → batería completa
HMI,FTEST,RUN,<id>                   → un solo test (reintento)
HMI,FTEST,ABORT
HMI,FTEST,CONFIRM,<id>,<0|1>         → respuesta del operario a un CONFIRM
CTRL,FTEST,<id>,<status>,<detail>    → status: 0 RUNNING 1 PASS 2 FAIL 3 SKIP
                                                4 WAIT (espera estímulo)
                                                5 CONFIRM (pregunta al operario)
CTRL,FTEST_DONE,<pass>,<fail>,<skip>
CTRL,FTEST_REJECT,<reason>           → 0 ya hay batería en curso
                                       1 control o fototerapia activos
                                       2 id desconocido
```

Compatibilidad: ambas placas ya descartan líneas con prefijo desconocido, así
que una pareja mixta no se rompe; el display trata la ausencia de cualquier
`CTRL,FTEST*` en 10 s tras `START` como "motherBoard sin soporte".

### D3. Cola de líneas hacia `Communication_Task` para el TX de la motherBoard

Se añade `CommunicationHost_Enqueue(const char *line)`: una `QueueHandle_t`
de `FTEST_TX_QUEUE_LEN = 16` slots de `FTEST_TX_LINE_MAX = 64` bytes, drenada
por `Communication_Task` en cada vuelta de su bucle, justo antes de la
telemetría periódica. Solo la tarea de comunicación escribe en `hmiSerial`,
como hasta ahora. Se descartó un mutex de TX porque hay ocho rutas distintas
que escriben a pelo por `hmiSerial` y todas habría que tocarlas; la cola no
toca ninguna. Si la cola está llena la línea se pierde con log de error: el
display marca el test como FAIL por tiempo de espera, nunca se bloquea la
tarea de test.

### D4. Tarea propia `FTEST` con estado seguro explícito

`src/modules/factory_test/factory_test_task.cpp` crea, bajo demanda, la tarea
`FTEST` (8192 B, prioridad 3, core 1; se autodestruye al terminar). Antes del
primer test entra en **estado seguro**, y lo deshace en un `restore()` que
corre siempre (también en ABORT y en fallo):

| Qué | Cómo | Por qué |
|---|---|---|
| Alarmas | `in3.alarmsEnabled = false` | `securityCheck()` declararía fallo de calefactor / ventilador y apagaría el humidificador durante el test |
| PIDs | `stopPID(airPID/skinPID/humidityPID)`, `fanControlPID.SetMode(MANUAL)` | ya están en MANUAL con `actuation == OFF`, se fuerza por si acaso |
| `PIDHandler()` y `turnFans()` | flag global `g_factoryTestActive` consultado al inicio de ambas | son los dos escritores de PWM que sobreviven con el control apagado |
| Salida | `ledcWrite` a 0 en calefactor, fototerapia, ventilador y zumbador; `in3_hum.turn(OFF)`; `ACTUATORS_EN` LOW | punto de partida conocido para `testStandByCurrent()` |

Precondición dura, comprobada en `parse_line()` **antes** de crear la tarea:
`in3.actuation == ACTUATION_OFF && !in3.phototherapy && !ftest_running`. Si
falla, `CTRL,FTEST_REJECT`. Alternativa descartada: apagar el control desde el
test. Un test de fábrica no debe tener autoridad para apagar un control que
alguien encendió.

`actuatorsTest()` escribe NVS (`KEY_HEATER_TEST`, `KEY_FAN_RPM_FEEDBACK`) y
`in3.phototherapy_intensity` / `photoFirstRun`: en fábrica eso es exactamente
lo deseado (deja la calibración de baseline hecha), así que se acepta y se
documenta. Se exponen `actuatorsTest()`, `testStandByCurrent()` y
`HW_error` con declaraciones en un header nuevo `include/system/hw_selftest.h`
en vez de la declaración implícita actual.

### D5. Cuerpos de test síncronos; el estímulo y la confirmación viven en el cuerpo

Cada test es `FtestStatus fn(char *detail)` y corre en la tarea `FTEST`, con
permiso para bloquear. La secuencia es un bucle sobre la tabla; la única
lógica pura extraída (y testeada en nativo) es la **acumulación de resultado**
(`ftest_summary`: contadores y máscaras PASA / FALLA / EJECUTADO a partir de
la secuencia de estados) porque es lo que acaba en NVS y en `FTEST_DONE`.

- **Estímulo** (puerta, luz): el cuerpo encola `CTRL,FTEST,id,4,…`, y sondea
  `sensorboard_get_snapshot()` hasta ver la transición o agotar el plazo
  (`FTEST_STIMULUS_TIMEOUT_MS = 30000`). El display pinta la instrucción a
  partir del ID, no del `detail`.
- **Confirmación** (zumbador cuando no hay micrófono): el cuerpo encola
  `CTRL,FTEST,id,5,…` y espera un semáforo binario que `parse_line()` da al
  recibir `HMI,FTEST,CONFIRM,id,ok`, con plazo `FTEST_CONFIRM_TIMEOUT_MS =
  60000`. Un CONFIRM con `id` distinto del esperado se descarta con log.
- **ABORT**: flag atómico consultado entre tests y dentro de los bucles de
  espera; el test en curso termina como SKIP con `detail=abort`.

Se descartó una máquina de estados pura con tick porque las esperas reales
son bloqueos en I2C, INA3221 y semáforos; modelarlas como tick añadía código
sin añadir tests con valor.

### D6. Petición `status` en `sensorboard_comm`

Mismo patrón que `capture`: `sensorboard_status_request()` deja un flag que
la tarea del módulo consume en su siguiente tick y envía
`{"type":"cmd","cmd":"status","id":N}`. La respuesta se decodifica en
`sb_json_codec` (nueva función pura `sb_json_decode_status_resp()` con test en
`test_sensorboard_json`) y rellena en el snapshot: `status_seen`,
`sb_fw[16]`, `avail_sht[3]`, `avail_als`, `avail_door`, `avail_cam`,
`usb_swap`. El test de fábrica lo usa; el resto del firmware no cambia.

### D7. El GSM se testea leyendo el estado de `GPRS_Task`

| Test | Evidencia | Plazo |
|---|---|---|
| AT | `GPRS.powerUp` | hasta 45 s (el módem puede estar arrancando) |
| SIM | `GPRS.CCID` no vacío | 15 s tras AT |
| Señal | `1 <= GPRS.CSQ <= 31` | 15 s |
| Red (opcional) | `GPRS.post` | 60 s; si no, SKIP |

Ventajas: cero comandos AT desde una segunda tarea, cero riesgo de
`AT+CPOWD`. Coste: el test depende de la cadencia de `GPRS_Task`; se acepta.

### D8. WiFi, ThingsBoard y hora, pasivos y opcionales

- WiFi: `WIFIIsConnected()` en ≤ 30 s → PASS con RSSI; si no, **SKIP** ("sin
  AP"). No se tocan credenciales: `applyWifiCredentials()` bloquea 15 s y
  sobrescribe NVS, y en fábrica el AP `in3wifi` ya es el default compilado.
- ThingsBoard: `in3.serialNumber == 0` → SKIP ("sin nº de serie"); con serie,
  `WIFIIsConnectedToServer() || GPRSIsConnectedToServer()` en ≤ 30 s → PASS
  con el transporte; si ningún transporte llegó a conectar → SKIP; si un
  transporte está conectado y el servidor no acepta en el plazo → FAIL.
- Hora: `time(nullptr) >= 1609459200` → PASS con `tz_source_origin()`; si no,
  SKIP. Informativo: sin red no hay hora y no es un fallo de la placa.

### D9. Display: overlay `FactoryTest` en `lv_layer_top()` y botón en el splash

- Nuevo módulo `src/ui/FactoryTest.cpp` + `include/ui/FactoryTest.h`, molde
  `AlarmCenter`: `_Init` (crea el overlay oculto), `_Open`, `_Close`,
  `_IsOpen`, `_Poll`, `_ApplyLanguage`. Lista con una fila por test (nombre,
  estado con el color de prioridad ya usado: `0xDFF3FF` PASA, `0xFFE0E4`
  FALLA, `0xFFF0D6` espera / omitido), zona de instrucción con botones Sí /
  No, botones Reintentar (fila fallida) y Salir.
- Botón "TEST FÁBRICA" en `ui_ScreenIntro_screen_init()` (abajo a la
  izquierda, `montserrat_16`). Su callback pone `g_factoryTestRequested` y
  llama a `FactoryTest_Open()`. `intro_timer_cb()` no navega mientras el
  flag esté puesto; `inactivity_timer_cb()` no bloquea mientras
  `FactoryTest_IsOpen()`, igual que ya exceptúa `ui_ScreenAlarms`. Al salir,
  `lv_scr_load(ui_ScreenMain)`.
- Orden: primero los tests locales (todos en `UI_Task`, por polling, sin
  `delay()`), después `HMI,FTEST,START`. Los `CTRL,FTEST*` los parsea
  `CommTask.cpp` en un anillo de 8 resultados protegido con
  `portMUX_TYPE` y flag `g_pendingFtest`, que `FactoryTest_Poll()` drena
  dentro del bucle de UI (patrón `g_pendingAlarmHistory`).
- Tests locales: SYSINFO (`ESP.getFlashChipSize()`, `ESP.getPsramSize()`,
  heap mínimo), I2C (ACK de 0x14 y 0x30; 0x18 informativo), PANEL (cinco
  pantallas de color sólido de 800 ms cada una sobre el overlay, luego
  pregunta Sí / No), TOUCH (cinco objetivos: esquinas y centro, acierto si el
  toque cae a ≤ 40 px), BUZZER y SPEAKER (300 ms con la API no bloqueante,
  luego Sí / No), WIFI (`WiFi.macAddress()`; conectado → PASS con RSSI; si
  no, `scanNetworks(async)` y PASS si ve alguna red), NVS (escribir y releer
  `hmi_ftest/probe`), LINK (`Display_BoardEverSeen() && !Display_IsBoardLinkLost()`
  con `fwVer` del último `CTRL,STATE`).
- El display persiste en `hmi_ftest`: epoch, máscaras locales PASA / FALLA,
  contadores de la motherBoard y `FWversion`. La motherBoard persiste en
  `mb_ftest`: epoch, máscaras PASA / FALLA / EJECUTADO, `FWversion` y
  `sb_fw`. Escrituras NVS fuera de `LVGL_Lock()`.

### D10. Tests de la motherBoard y sus criterios

| ID | Test | Criterio PASA | Notas |
|---|---|---|---|
| 0 | SYSINFO | flash = 8 MB, heap libre ≥ 40 kB | informativo: reset reason y `boots` en detail; siempre PASA salvo heap |
| 1 | INA3221 | `digitalCurrentSensorPresent[MAIN] && [SECUNDARY]` | |
| 2 | STANDBY | `testStandByCurrent()` sin bit `STANDBY_CONSUMPTION_*` nuevo en `HW_error` | se compara `HW_error` antes / después |
| 3 | CHARGER | `charge_status()` devuelve true | detail: VBAT/VSYS/VBUS mV, ICHG mA |
| 4 | POWER_SRC | siempre PASA | detail: `ac` o `bat` según `g_bq_status.ac_present` |
| 5 | SKIN_ADC | ACK I2C en 0x48 | `skinProbeLastReading() == OPEN` → PASS con detail `sin sonda`; lectura fuera de 1..60 °C con sonda → FAIL |
| 6 | EXT_SHT4X | `ambientSensorPresent` y `updateAmbientSensor()` con T en −10..60, H en 0..100 | |
| 7 | SENSOR_SRC | `sensorSourceGet() == SENSOR_SOURCE_SENSORBOARD` | FAIL con detail `i2c2 responde`: el modo silencioso de fallo de la nueva generación |
| 8 | SB_LINK | `sensorboard_comm_connected()` y `snapshot.link_ok` en ≤ 10 s | SKIP si test 7 falló |
| 9 | SB_STATUS | `status_seen` con las 6 disponibilidades a true en ≤ 5 s | detail: `sb_fw` y `usb_swap` |
| 10 | SB_ENV | `env_seen`, las 3 posiciones válidas, dispersión ≤ 1.0 °C, |media − exterior| ≤ 3.0 °C | umbrales en el header del módulo |
| 11 | SB_DOOR | WAIT: ver `door_open` true y luego false | 30 s |
| 12 | SB_LIGHT | WAIT: lux cae por debajo del 50 % de la base | base < 20 lux → SKIP `poca luz`; 20 s |
| 13 | SB_CAMERA | `capture_request()` y `capture_take()` con `len ≥ 1000` en ≤ 10 s | libera el buffer |
| 14 | ACTUATORS | `actuatorsTest()` sin bits nuevos de calefactor, fototerapia ni ventilador en `HW_error` | detail: corrientes |
| 15 | FAN_RPM | `in3.fanHasSpeedFeedback && in3.fan_rpm >= FAN_MIN_RPM` tras 14 | |
| 16 | HUMID_USB | `USB_EN` HIGH → `GPIORead(USB_FAULT)` true tras 100 ms, corriente en `USB_SHUNT_CHANNEL` > 20 mA | vuelve a LOW siempre |
| 17 | BUZZER | dBA con zumbador − dBA base ≥ 6 dB | sin `sound_seen` → CONFIRM al operario |
| 18 | AFE_SPI | `afe.getTimingConfig()` devuelve los registros que `begin()` escribió: `t1 < t2`, `t2 != 0` y `t2 != 0xFFFF` | la librería no expone lectura cruda de registros; `runAfeDiagnostics()` (DIAG 0x30) va al detail, informativo |
| 19 | AFE_PROBE | `g_spo2_data.probe_state != DISCONNECTED` | opcional: SKIP si desconectada |
| 20 | HMI_LINK | `g_hmiEverSeen` y última línea hace < 5 s | siempre PASA si llegó el START |
| 21 | GSM_AT | D7 | |
| 22 | GSM_SIM | D7 | |
| 23 | GSM_SIGNAL | D7 | detail: CSQ |
| 24 | GSM_NET | D7 | opcional |
| 25 | WIFI | D8 | opcional |
| 26 | TB_PROVISION | D8 | opcional |
| 27 | TIME | D8 | opcional |
| 28 | NVS | `putUInt` + `getUInt` en `mb_ftest/probe` | |
| 29 | LITTLEFS | `LittleFS.begin(true)` y `totalBytes() > 0` | |

## Risks / Trade-offs

- [`actuatorsTest()` calienta al 100 % durante ≤ 8 s con el control apagado]
  → es lo mismo que hace en cada arranque; el estado seguro deja PWM a 0 antes
  y después, y `restore()` corre también en ABORT.
- [El operario lanza el test con un bebé dentro] → precondición
  `actuation == OFF && !phototherapy` y REJECT explícito. No cubre el caso de
  bebé sin control activo, que es indistinguible por software; se documenta en
  `docs/hmi.md` que el botón solo aparece en el splash.
- [La cola TX se llena bajo `CTRL,PPG` a 25 Hz] → 16 slots para ≤ 3 líneas
  por test; se pierde con log y el display marca FAIL por plazo. Nunca bloquea.
- [Los tests GSM dependen del ritmo de `GPRS_Task`] → plazos generosos (D7); en
  fábrica sin cobertura, `GSM_NET` es opcional y sale SKIP.
- [`Credentials.h` no está versionado] → un build limpio compila con los
  dummies de `Credentials_public.h`; el test WiFi saldrá SKIP, no FAIL. El
  build de fábrica lleva el `Credentials.h` real, como hoy.
- [El display podría quedarse esperando a una motherBoard antigua] → plazo de
  10 s sin `CTRL,FTEST*` → fila "MB sin soporte" en FAIL y se sigue.
- [`inactivity_timer_cb` y `intro_timer_cb` son código de `UITask.cpp`,
  monolito] → dos condiciones de una línea cada una; no se amplía nada más ahí.
- [El test PANEL pinta sobre el overlay y podría dejar un color pegado si la UI
  se cae] → los rectángulos de color son hijos del overlay y se destruyen con
  `_Close`.
- [Reentrada de `HMI,FTEST,START`] → REJECT 0 mientras `ftest_running`.

## Migration Plan

1. `shared/` + tests nativos (commit propio).
2. motherBoard: cola TX, `hw_selftest.h`, `status` de SensorBoard, módulo
   `factory_test`, parseo en `CommTask.cpp` (commits por stage).
3. Display_HMI: `FactoryTest` + botón + enganches (commits por stage).
4. `PROTOCOL.md` v2.3.0 y `docs/hmi.md`.
Rollback: quitar el botón del splash deja todo lo demás inerte; ningún
mensaje nuevo se emite sin `HMI,FTEST,START`.

## Open Questions

- Umbrales de coherencia de SB_ENV (1.0 °C / 3.0 °C) son de partida; se
  ajustan en banco y se dejan como constantes con nombre.
