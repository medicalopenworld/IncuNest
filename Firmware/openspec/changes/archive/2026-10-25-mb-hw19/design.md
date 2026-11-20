## Context

La V19 no existe físicamente. El pinout sale del PcbDoc y se trata como dato. El firmware debe compilar para ella y no alterar la V18. Las decisiones de abajo están tomadas leyendo el código y el framework (`espressif32@6.6.0`, Arduino-ESP32 2.0.14), no a ojo.

## Tabla de pines V19

| GPIO | Define | Función | Dirección / nivel |
|---|---|---|---|
| IO7 | `AFE44XX_PWDN_PIN` | PDN del AFE4490 (U4-20) | salida, LOW = apagado. En V17/V18 este pin era `FAKE_PIN` (46) |
| IO39 | `GSM_PWR_EN` | EN del buck TPS563231 de VBAT del SIM800C | salida, pull-down 73k2: módem APAGADO hasta que el firmware lo levanta |
| IO40 | `PWR_5V_EN` | EN del buck TPS563231 de 5 V | salida, pull-down: sin firmware no hay humidificador, LEDs del AFE, SensorBoard ni display |
| IO41 | `EXT_WDT_WDI` | WDI del TPS3431 (montaje por confirmar) | salida, solo si `EXT_WDT_ENABLED` |
| IO42 | `MAINS_OK_PIN` | CHRG_OK del BQ25730 (U11-4) | entrada, open-drain con pull-up 10k a 3V3; HIGH = fuente de entrada válida |
| IO43 / IO44 | `DEBUG_UART_TX_PIN` / `DEBUG_UART_RX_PIN` | UART de depuración, conector J1 | UART0 |
| IO46 | `FAKE_PIN` | libre | sin cambio |
| IO5 / IO6 | `USB_EN` / `USB_FAULT` | TPS2553 del USB de la SensorBoard | sin cambio |
| IO18 | `BABY_TEMP_EN` | referencia LM4040 | sin cambio |
| IO35/36/37 | `AFE_MOSI/SCK/MISO` | SPI del AFE | sin cambio (ver PSRAM) |

`GPRS_PWRKEY` pasa a **0** en V19. Hoy es `FAKE_PIN` (46), y `#if (GPRS_PWRKEY)` en `GPRS.cpp` y `initHardware.cpp` es VERDADERO con 46, así que la V18 pulsa IO46 en el vacío y gasta ~2 s en `GPRSPowerUp()` (casos 0 y 1). En la V19 PWRKEY va a GND y ese bloque debe quedar fuera. **La V18 conserva ese comportamiento a propósito**: cambiarlo no es objeto de este cambio y alteraría el tiempo de arranque de la unidad de banco.

## Decisiones

### D1. Entorno y PSRAM

`[env:IncuNest_V19]` extiende `common` como V18, con `-DHW_NUM=19` y `-DBOARD_HAS_PSRAM`. La V18 y la V17 no definen `BOARD_HAS_PSRAM`, así que no inicializan PSRAM aunque el SDK de la variante la soporte.

**Módulo N8R2 y no N8R8/N16R8**: la PSRAM octal de los módulos R8 ocupa GPIO33-37 y el SPI del AFE4490 va en IO35/36/37 (`AFE_MOSI/SCK/MISO`). Un R2 usa PSRAM **quad**, que no consume esos pines.

**`memory_type`**: el enunciado pedía `qio_qspi`. Se usa **`dio_qspi`**, por esto: `board_build.flash_mode = dio` y la variante de librerías precompiladas (con su sdkconfig) se elige por `<flash_mode>_<psram_type>` (`builder/main.py`, `_get_board_memory_type`); hoy la V18 ya compila contra `dio_qspi` (el defecto del builder con `flash_mode=dio` y `psram_type` sin definir). Pasar a `qio_qspi` cambiaría las librerías precompiladas (sdkconfig del driver de flash) de una placa cuya flash "queda igual que hoy", mientras la cabecera de la imagen seguiría escribiéndose en `dio`, o sea un estado mixto sin necesidad. `dio_qspi` ya contiene el soporte de PSRAM quad (se activa con `BOARD_HAS_PSRAM`). Si en banco se quisiera flash QIO, es un cambio de una línea (`board_build.arduino.memory_type = qio_qspi` + `board_build.flash_mode = qio`) con su propia verificación de arranque.

La tabla de particiones **no cambia** (8 MB, `ESP32S3_8MB.csv`).

El sysinfo de arranque imprime `psramFound()` y `ESP.getPsramSize()` para saber en el primer banco si el módulo lleva la PSRAM que se espera y la inicializa. Una V19 prototipo con un módulo sin PSRAM arrancaría con `psramFound()=0`: no es un fallo, es un dato.

### D2. HAL: `hal_hw19.cpp` propio

`g_hal_pins` no se referencia hoy fuera de `src/hal/`, pero la tabla existe para reflejar el pinout de cada revisión. `hal_hw17.cpp` fija `afeLedAlm = 7`, y en la V19 IO7 es **AFE_PDN**, así que reutilizarlo dejaría una tabla que afirma lo contrario del PCB. Por eso se crea `hal_hw19.cpp` (copia de `hal_hw17.cpp` con `afeLedAlm = 0xFF` y los campos nuevos) en lugar de extender `hal_hw17.cpp`. `hal.h` añade `afePwdn, gsmPwrEn, pwr5vEn, extWdtWdi, mainsOk, debugTx, debugRx` (0xFF = sin pin en esa revisión) y los tres ficheros HAL se mantienen en paralelo; los `build_src_filter` excluyen los que no corresponden a cada entorno.

### D3. Secuencia de arranque HW19

En `setup()`:

1. Latch de `PWR_EN` (ya existía, sin cambio).
2. **`pwr5vSet(true)`**: lo primero que se hace tras el latch, antes de abrir `debugSerial`, antes de `initHardware()` (cuyo test de actuadores usa el humidificador por `USB_EN`, que cuelga del raíl de 5 V), antes de `CommunicationHost_Init()` (enlace con el display), antes de `sensorboard_comm_init()` (host USB) y antes de `initSPO2()` (LEDs del AFE).
3. `debugSerial.begin(...)`, logs de arranque, `initGPIO()`.
4. **`gsmPowerSet(true)`** justo tras `initGPIO()`, bastante antes de crear la tarea GPRS, para que el SIM800C haya arrancado (~3 s tras aplicar VBAT) cuando `GPRSPowerUp()` empiece a sondear AT. Los casos 2 de `GPRSPowerUp()` ya reintentan cada segundo dentro de `GPRS_TIMEOUT`.

Los helpers viven en `src/system/power_rails.{h,cpp}`. En HW<19 son no-op (`pwr5vSet`, `gsmPowerSet`), `mainsPresentPin()` devuelve `true`.

### D4. Power-cycle del módem (feature 12)

Máquina de tres fases, **pura y sin tiempo propio** (recibe `now_ms`), en `modules/control/modem_power_cycle.{h,cpp}`:

`IDLE --begin(grace)--> SHUTDOWN_WAIT --(now-t >= grace)--> OFF --(now-t >= off_ms)--> IDLE`

- `poll()` devuelve la acción a ejecutar: `NONE`, `CUT` (poner `GSM_PWR_EN` LOW) o `RESTORE` (HIGH). Cada acción sale exactamente una vez.
- `GPRSForceReset()` (tarea GPRS): tras `AT+CPOWD=1` llama `begin(grace = 1500 ms)`: el SIM800C tarda ~1 s en completar «NORMAL POWER DOWN» y cortar VBAT en seco durante una escritura de red es lo que el CPOWD intenta evitar. `off_ms = 2000` (el mínimo pedido).
- `GPRSPowerUp()` caso 0 consulta `gsmPowerCycleBusy()` y **no avanza** mientras dure, sin bloquear la tarea (vuelve a la vuelta siguiente de `GPRS_Handler()`). Mientras `GPRS.process == 0` el temporizador `GPRS_TIMEOUT` de `GPRSStatusHandler()` no corre, así que la espera no la mata el timeout. Al ejecutar `RESTORE` se reinicia `processTime`/`packetSentenceTime`.
- `mon_kill` (`GPRSMonitorTask`, `main.cpp`): la tarea GPRS está colgada y se borra; no hay CPOWD que esperar, así que `begin(grace = 0)` y `CUT` inmediato antes de recrear la tarea; la tarea nueva restaura al entrar en `GPRSPowerUp()` caso 0 pasados los 2 s.
- Contador `GSM_power_cycles` (RAM, desde el arranque; no se persiste en NVS) publicado en DIAG junto a los kills.
- Solo `HW_NUM >= 19`. En V18 `GPRSForceReset()` queda exactamente igual (el `#if` deja solo la llamada no-op).

Condición de carrera: `gsmPowerCycleBegin()` desde el monitor solo se llama con la tarea GPRS ya borrada y antes de recrearla; desde `GPRSForceReset()` corre en la propia tarea. El estado no lo tocan dos tareas a la vez.

### D5. AFE4490 (feature 10)

(a) `initSPO2()` ya pulsa `AFE44XX_PWDN_PIN` (LOW 100 ms, HIGH 100 ms): en V19 es un reset real. Se extrae a `afePdnPulse()` para reutilizarlo.

(b) `diag_code` (`AFE4490Data::diag_code`, bits 0-12 = REG_DIAG 0x30, el resto banderas RSQM) sale en `/debug/state` (`"afe":{"diag":N,...}`), en DIAG (`AFE_diag`, entero) y en la línea periódica de SPO2 si `LOG_PULSIOXIMETRY` está activo. Se lee de `g_spo2_data`, que `SPO2_Task` ya escribe, así que no hay llamada nueva a la librería ni SPI nuevo.

(c) Watchdog de muestras. Lectura de la librería (v0.94, `incunest_afe4490.cpp`):

- `begin()` crea 4 objetos FreeRTOS, 4 tareas (`incunest_afe4490`, `hr2`, `hr3`, `diag`) y engancha la ISR de DRDY (`attachInterrupt` RISING).
- **No existe `afe.end()`**: el equivalente es `stop()`: desengancha la ISR, **toma `_spi_mutex` con `portMAX_DELAY`**, borra las 4 tareas, borra colas/semáforos/mutex y deja `_initialized=false`. `begin()` puede llamarse otra vez tras `stop()`.
- Riesgo 1: si la tarea muerta murió con `_spi_mutex` cogido, `stop()` se queda esperando para siempre. Acotado: solo bloquea `SPO2_Task` (el `loopTask` sigue alimentando el Task WDT) y es indistinguible, para el firmware, de «el AFE sigue colgado». Queda como riesgo anotado, no se arregla tocando la librería (regla: no subir librerías).
- Riesgo 2: otros llamantes de `afe.*` (`factory_test_hw.cpp`: `getTimingConfig()`/`runAfeDiagnostics()`) durante la ventana `stop()`..`begin()` ven `_initialized` en falso y devuelven ceros (la librería lo contempla), pero una carrera de ms con el borrado del mutex es posible. El factory test corre con el equipo parado y en el arranque; el watchdog solo actúa tras 10 s sin muestras, no durante un test de fábrica corto. Se documenta.
- Secuencia: `afe.stop()` → pulso de PDN (LOW 100 ms, HIGH 100 ms) → `afe.setHgacEnable(true)` → `afe.begin(AFE44XX_CS, AFE_ADC_READY)`. `SPI.begin()` no se repite. `SPO2_Task` y su cola de aplicación se conservan: solo se reinicia la librería.
- Decisión: **solo en `HW_NUM >= 19`** (`AFE_WATCHDOG_ENABLED`). En HW<19 el pulso de PDN cae en `FAKE_PIN` (inocuo pero inútil) y reiniciar la librería sin reset físico cambiaría el comportamiento de la V18. Contador `AFE_resets` solo en V19.
- Lógica en `modules/control/afe_watchdog.{h,cpp}`: «sin muestra > 10 s con tarea viva» y «como mucho una vez por minuto». «Tarea viva» = la propia `SPO2_Task` está ejecutando el chequeo, es decir, el watchdog corre dentro de su bucle, así que si la tarea de la aplicación muriera no habría chequeo (cubierto por el Task WDT del sistema, no por este mecanismo).

### D6. Watchdog externo

`EXT_WDT_WDI` se conmuta (toggle de nivel, no pulso) en `watchdogReload()`, que ya es el sitio del `loop()` (`LOOP_TASK_PERIOD_MS` = 1000 ms) donde se alimenta el Task WDT de 75 s. `#define EXT_WDT_ENABLED 0` por defecto. Con 0 **no se configura el pin** (queda como entrada flotante) para no interferir con un montaje no confirmado ni, en V19, con IO41 como MTDI de JTAG. Se ponga a 1 solo cuando el hardware fije la ventana del TPS3431 (pines de temporización) y se verifique en banco: un watchdog de ventana resetea tanto por pulso tardío como por pulso **temprano**, y 1 Hz puede ser ambas cosas según los condensadores.

### D7. CHRG_OK

`MAINS_OK_PIN` como `INPUT` (el pull-up de 10 k es externo). `mainsPresentPin()`: `digitalRead == HIGH`. Se publica en `/debug/state` (`"mains_ok"`). Sin antirrebote ni alarma aquí: eso es `feat/mains-detector`, que decidirá el filtrado. La polaridad (HIGH = red) viene del enunciado y está por confirmar en banco.

### D8. UART de depuración

Hallazgo que **corrige la premisa del enunciado**: en la motherBoard `debugSerial` es `Serial`, y `Serial` es la **UART0 hardware del S3 con sus pines por defecto, que son IO43 (TX) e IO44 (RX)**. Se comprobó así: `platformio.ini` no define `ARDUINO_USB_CDC_ON_BOOT` (el core lo define a 0, `HardwareSerial.h:326`), `ARDUINO_USB_MODE=1` solo selecciona el modo de USB «hardware CDC/JTAG» que aquí no se usa (IO19/IO20 se usan como I2C2/host USB), y `variants/esp32s3/pins_arduino.h` fija `TX = 43`, `RX = 44`. El display no usa UART0: `CommTask.cpp` define `hmiSerial = Serial1` y la abre en IO15/IO16, y el módem usa `Serial2`. Por tanto **UART1 NO está libre** (es el display) y **UART2 es el módem**: no queda ninguna UART hardware para una segunda salida de log.

Decisión: **no se añade ninguna `HardwareSerial`**. El log de la V19 ya sale por IO43/IO44 = J1 a 115200 (también el log ROM/bootloader). Para dejarlo explícito y que un cambio futuro de core no lo mueva en silencio, en V19 se llama `debugSerial.begin(115200, SERIAL_8N1, DEBUG_UART_RX_PIN, DEBUG_UART_TX_PIN)`, equivalente al valor por defecto. `incunest_log_printf` no cambia: sigue reenviando a `log_printf` → `Serial`. Dos UART con el mismo log obligaría a quitar una de las otras dos funciones o a duplicar cada línea en una ruta con mutex, sin ganancia.

## Riesgos

- PSRAM: si el módulo montado no lleva PSRAM, o es octal, el arranque lo delata en `psramFound()`; no se mueve ningún buffer a PSRAM en este cambio.
- `stop()` del AFE bloqueante (D5).
- Polaridad de CHRG_OK, tiempo de corte del módem (2 s) y consumo del módem al arrancar con el buck de 5 V a la vez: por confirmar en banco.
- `THINGSBOARD_FIELDS_AMOUNT`: tres claves nuevas en DIAG GPRS suben el peor caso de 101 a 104 de 112.
