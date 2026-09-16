# Porte de motherBoard y Display_HMI a ESP-IDF nativo

Rama: `refactor/idf-native-port` · Worktree: `Firmware/.worktrees/idf-native-port`
ESP-IDF de referencia: **v6.0.1** (`IDF_PATH=C:\esp\v6.0.1\esp-idf`), la misma
que ya usa la SensorBoard.

Objetivo: quitar el framework Arduino de las dos placas y construir con
`idf.py`/CMake, dejando las tres placas del equipo sobre la misma toolchain.

> **Recordatorio de contexto.** Esto es firmware de un equipo medico con
> unidades en campo (Togo) y una tirada de 200 en curso. El criterio de todo el
> porte es **preservar comportamiento**: donde algo se cambia a mejor, va en un
> commit aparte y etiquetado. Lo que no se puede verificar sin banco, se marca
> como pendiente de banco y no se da por hecho.

---

## 1. Estado actual (medido, no estimado)

**Las dos placas compilan y enlazan enteras sobre ESP-IDF v6.0.1, sin una
linea de Arduino.** Nada probado en hardware todavia.

| Binario | Tamano | Slot | Libre |
|---|---|---|---|
| `Display_HMI/build/display_hmi.bin` | 0x26df60 (2,5 MB) | 5 MB | 51 % |
| `motherBoard/build/motherboard.bin` | 0x1deb50 (1,96 MB) | 2,625 MB | 29 % |

| Pieza | Estado |
|---|---|
| `components/incunest_platform` (tiempo, GPIO, PWM, I2C, SPI, NVS, FS, UART, WiFi, TCP/TLS, HTTP, OTA, mDNS, String) | ✅ |
| `shared/` como componente | ✅ |
| `components/thingsboard` (SDK con tres parches, `PATCHES.md`) | ✅ |
| `components/incunest_sensors` (9 libs vendorizadas), `incunest_afe4490`, `arduino_pid`, `incunest_gt911` | ✅ |
| GPRS sobre `esp_modem` (PPP/CMUX), `motherBoard/src/tasks/gprs_modem.*` | ✅ compila · ⚠ banco |
| Tests de host: `tools/host_tests` (CMake + Unity) | ✅ **25/25 en verde** |
| `flasher_tool` (layout de IDF, con PlatformIO como respaldo) | ✅ |
| `.github/workflows/release.yml` (esp-idf-ci-action v6.0.1) | ✅ sin ejecutar aun |

Comandos:

```
idf.py -C Firmware/Display_HMI build
idf.py -C Firmware/motherBoard build            # HW_NUM y variante de taller: idf.py menuconfig
idf.py -C Firmware/tools/platform_smoke build   # gate barato de la capa de plataforma
pwsh Firmware/tools/host_tests/run_host_tests.ps1   # 25 suites Unity en el PC
```

## 2. Decisiones tomadas y por que

### 2.1 `millis()` y `micros()` conservan el nombre

390 llamadas entre las dos placas, casi todas dentro del patron de resta sin
signo `(uint32_t)(millis() - t0) >= PERIODO`, y muchas en caminos de alarma y
control. La implementacion de `components/incunest_platform/plat_time.h` ya es
ESP-IDF puro (esp_timer): no queda una linea de Arduino. Renombrarlas solo
habria anadido 390 ediciones a mano sin ganar nada tecnico.

Se preserva a proposito el desbordamiento: `millis()` da la vuelta a los 49,7
dias y `micros()` a los 71,6 minutos, igual que antes. Para plazos largos hay
`millis64()`.

### 2.2 El PWM conserva el mapeo canal→timer de Arduino

`timer = (canal / 2) % 4`, la regla interna de `esp32-hal-ledc.c`. No es un
detalle: `board.h:167` documenta que el ventilador se movio al canal 7
**justo para que su frecuencia no pisara los 400 Hz del calefactor**, y esa
colocacion solo tiene sentido con ese mapeo. Cambiarlo por un reparto "mejor"
de timers alteraria en silencio la frecuencia real de un actuador termico.

### 2.3 `NvsPrefs` reproduce byte a byte el formato de `Preferences`

Es el punto de mayor riesgo de datos de todo el porte. Las unidades en campo
tienen la NVS escrita por la `Preferences` de Arduino, y ahi viven los
perfiles de bebe, la configuracion de control y las claves. El mapeo se
verifico leyendo el `Preferences.cpp` real de arduino-esp32.

**La trampa:** `putFloat`/`putDouble` guardaban un **BLOB**, no un tipo
numerico de NVS. Son 52 escrituras y 18 lecturas de float en el firmware. Si
alguien las "mejora" a `nvs_set_u32` o similar, un OTA deja a las unidades con
los valores por defecto y sin perfiles de bebe. Tambien se conserva el
`nvs_commit()` tras **cada** escritura, porque cambia el desgaste de flash y el
comportamiento ante un corte de corriente.

Los nombres de metodo se conservan (`putFloat`, `getUChar`...), asi que
convertir un fichero es cambiar el tipo `Preferences` → `NvsPrefs` y poco mas.

### 2.4 Estandar de C++ fijado al que ya se usaba

ESP-IDF 6 compila por defecto en **gnu++26**, y el codigo venia de gnu++2b
(HMI) y gnu++17 (motherBoard). C++26 convierte en **error** las operaciones de
bits entre enums de distinto tipo, patron que LVGL usa por todas partes.
Modernizar el estandar sobre 62k lineas es un proyecto propio, no algo que
deba entrar de gorra en un cambio de toolchain.

### 2.5 LVGL usa el `lv_conf.h` del proyecto

El componente de LVGL para ESP-IDF, si se le deja, se configura por menuconfig
y **se salta** `include/config/lv_conf.h`. Se apaga ese camino con
`LV_KCONFIG_IGNORE` y se apunta con `LV_CONF_PATH`, para que el fichero de
siempre siga siendo la unica fuente de verdad.

### 2.6 El SDK de ThingsBoard: copia local con dos parches

**v0.13.0 no compila contra ESP-IDF 6** (mbedtls 4) y **no hay ninguna version
publicada que lo haga**: v0.15.0 esta rota de otra forma y el proyecto no
publica desde diciembre de 2024. Detalle completo y delta exacto en
`components/thingsboard/PATCHES.md`. La alternativa era quedarse en ESP-IDF
5.5 (mbedtls 3), descartada porque fragmentaria de nuevo las toolchains.

---

### 2.7 GPRS: `esp_modem` con PPP en modo CMUX

Decision del 2026-09-11. `motherBoard/src/tasks/gprs_modem.{h,cpp}` envuelve la
API C de `esp_modem` con la forma de los metodos de TinyGSM, asi que la maquina
de estados de `GPRS.cpp` y los ganchos del test de fabrica no cambian. Dos
cosas que parecen erratas y se conservan a proposito: el cruce de nombres de
pines en `Serial2.begin()` (los nombres de `board.h` son desde el modem) y la
doble inversion lat/lon en `getGsmLocation()` que se anula. **Riesgo**: `CLBS`
y `CNTP` usan el portador interno `SAPBR` del SIM800, que puede no convivir con
PPP; se abre best-effort y el reloj queda cubierto por SNTP sobre PPP.

### 2.8 Los manejadores del servidor web siguen corriendo en la tarea de OTA

`esp_http_server` ejecuta los manejadores en su propia tarea; la `WebServer`
de Arduino los ejecutaba dentro de `handleClient()`, en la tarea de OTA. El
manejador de `/update` escribe la flash y comparte estado con la OTA de
ThingsBoard: moverlo de tarea seria meter una carrera entre dos escrituras de
flash. `plat_webserver` hace un rendezvous entre la tarea de httpd y
`handleClient()` para que la concurrencia sea exactamente la de antes.

### 2.9 Consola del HMI compartida con el protocolo en UART0

Decision del 2026-09-11: se mantiene como estaba. `HardwareSerial::begin()`
sobre UART0 hace que la consola escriba a traves del mismo driver
(`uart_vfs_dev_use_driver`) para que cada escritura salga entera.

## 3. Trabajo pendiente, en orden

El orden importa: cada bloque desbloquea al siguiente y es verificable por su
cuenta con `idf.py build`.

> **Estado**: fases A, B y C HECHAS (compilan, enlazan, tests de host en
> verde). Lo que queda es banco (seccion 4), lo que solo se puede hacer tras el
> merge (seccion 7) y las limpiezas de codigo muerto (seccion 6).

### Fase A — Display_HMI  ·  HECHA

| # | Trabajo | Ficheros | Notas |
|---|---|---|---|
| A1 | `Preferences` → `NvsPrefs` | `EEPROM.cpp`, `maintenance.cpp`, `training_progress.cpp`, `main.cpp` | Mecanico. Solo cambia el tipo. |
| A2 | `Wire` → `I2cBus` y driver propio de **PCA9557** | `UITask.cpp`, `FactoryTest.cpp` | El PCA9557 es un expansor trivial (un registro de salida). |
| A3 | Driver propio de **GT911** (tactil) | `lib/TAMC_GT911_Fixed` | 208 lineas, I2C puro. Reescribir sobre `I2cBus`. |
| A4 | **Enlace con la placa**: `Stream*`/`COMM_SERIAL` → driver UART de IDF | `display_comms.h`, `CommTask.cpp`, `main.cpp` | **El mas delicado: por aqui viajan las alarmas.** Ver la nota en `display_comms.h`. Conservar `COMM_RX_RING_BYTES = 1024`. En IDF el tamano del anillo es argumento de `uart_driver_install()`, lo que ademas resuelve de raiz el fallo de las lineas perdidas. |
| A5 | Red y nube: `WiFi`/`WebServer`/`Update`/`ESPmDNS` → `esp_wifi` + `esp_http_server` + `esp_https_ota` + `espressif/mdns`; `Arduino_MQTT_Client` → `Espressif_MQTT_Client` | `Wifi_OTA.cpp` (623 l.), `support_report.cpp` | Bloque grande pero acotado a dos ficheros. |
| A6 | Restos de `Arduino.h` | `hmi_audio_module.cpp`, `training_mode.cpp`, `AlarmCenter.cpp`, `BabyHistory.cpp`, `ElementsCreation.cpp` | Solo necesitan `millis()`, que ya existe. |

`AudioManager.cpp` sigue **fuera del build**, igual que en `platformio.ini`. No
se reactiva dentro del porte.

### Fase B — motherBoard  ·  HECHA (B9 pendiente de banco)

| # | Trabajo | Notas |
|---|---|---|
| B1 | `Preferences` → `NvsPrefs` (`EEPROM.cpp`, `baby_profile_store.cpp`, `main.cpp`) | 117 referencias, mecanico. |
| B2 | `String` → `std::string` | **290 usos.** El bloque mas tedioso, pero de riesgo bajo y revisable. |
| B3 | Borrar `TFT_eSPI` | Codigo muerto: `main.cpp:88` declara `TFT_eSPI tft`, tres ficheros la declaran `extern` y **nadie la usa**. La placa ya no tiene pantalla propia. |
| B4 | Drivers de sensor sobre `I2cBus`: SHTC3, STS3x, SHT4x, INA3221, TCA9555 | Los envoltorios `drv_*.cpp` ya existen y son finos; el trabajo es sustituir las libs de Arduino que hay detras. Sensores simples de comando + lectura de palabra. |
| B5 | `BQ25730` y `AFE4490` (SPO2) | `BQ25730` ya es nuestro (17 KB): solo cambia el transporte I2C. El AFE4490 va por SPI (`incunest_afe4490`, libreria externa pineada) y hay que reescribirlo sobre `spi_master`. |
| B6 | `PID_v1` y `RotaryEncoder` | Portables casi tal cual. El encoder puede ir a PCNT. |
| B7 | `LittleFS` → `joltwallet/littlefs` | **Montar la particion existente etiquetada `spiffs`, sin reformatear.** Ahi estan los perfiles de bebe y el archivo de pesos. |
| B8 | Red y nube (`Wifi_OTA.cpp`, 1679 l.) | Igual que A5. |
| B9 | **`TinyGSM` → `esp_modem`** (`GPRS.cpp`, 1339 l.) | **El bloque de mas riesgo de todo el porte.** No es una sustitucion: TinyGSM es AT sobre `Stream` y `esp_modem` es PPP/lwIP, otra arquitectura. De aqui cuelgan las SIM Onomondo y la tirada de 200. **Debe ir el ultimo y con banco dedicado.** |

### Fase C — alrededores  ·  HECHA salvo C4 (post-merge) y C5

| # | Trabajo | Notas |
|---|---|---|
| C1 | **Tests de host** (26 suites Unity) | Hoy corren con `pio test -e native`. Hay que rehospedarlos (CMake+Unity, o el target `linux` de IDF). **No dar el porte por terminado sin esto**: es la unica cobertura automatizada que existe. |
| C2 | `flasher_tool` | Lee `.pio/build/<env>/`; pasa a leer `build/`. Los nombres de los binarios cambian (`firmware.bin` → `<proyecto>.bin`). |
| C3 | `.github/workflows/release.yml` | `pio run` → `idf.py build`, y las rutas de los artefactos. |
| C4 | `.claude/` (11 ficheros mencionan `pio`) | Reglas, hooks y skills. El hook de Stop debe pasar a `idf.py build`. |
| C5 | Documentacion | `README`, `docs/partitions.md`, `CLAUDE.md`. |

---

## 4. Riesgos abiertos

1. **`esp_modem` (B9).** El unico bloque que cambia arquitectura y no solo API.
   Con 200 unidades en curso, merece rama y banco propios.
2. **Presupuesto de SRAM del HMI.** Los dos bounce buffers de 38,4 KB
   contiguos tenian ~87 KB de margen medido. Otra toolchain mueve ese numero.
   **Hay que volver a medir el mayor bloque contiguo (no el libre total)**
   antes de dar el HMI por bueno.
3. **Tamano de flash de la motherBoard sin confirmar.** `platformio.ini` no
   declaraba `flash_size` y heredaba 8 MB del manifest. Si el modulo real es
   N16R8, sobra media flash. Comprobar con `esptool.py flash_id`.
4. **Compatibilidad de NVS.** Verificar en una unidad real que, tras el OTA,
   los perfiles de bebe y la configuracion siguen ahi. Es lo primero que hay
   que probar en banco, antes que ninguna funcionalidad.
5. **Copia local del SDK de ThingsBoard.** Deuda asumida a conciencia sobre
   una dependencia sin mantenimiento. Revisar si upstream revive.

## 5. Lo que no se ha tocado a proposito

- `AudioManager.cpp`, fuera del build desde la migracion a Arduino 3.x.
- `src/hal/hal_hmi.*`: **no lo llama nadie** (comprobado con grep). Se porto en
  vez de borrarlo, porque quitar codigo es una decision de producto. Candidato
  a borrar en un commit aparte, junto con `TFT_eSPI` (B3).
- Los `usb_host_*` de la motherBoard, que ya eran ESP-IDF nativo. Candidatos a
  sustituirse por los componentes gestionados `espressif/usb_host_cdc_acm` y
  `espressif/usb_host_ch34x_vcp`, pero eso es un cambio aparte.

## 6. Hallazgos del porte que NO son del porte

Cosas que ya estaban en el codigo y que han salido a la luz al compilarlo
contra un compilador estricto. Se dejan anotadas, no arregladas: cada una
merece su propio commit.

- **`TFT_eSPI` en la motherBoard era codigo muerto** — objeto declarado, tres
  `extern`, cero llamadas. Ya retirada (bloqueaba 7 ficheros por arrastrar
  `Print.h`).
- **`PCA9557` en el HMI**: el expansor no esta poblado en esta revision
  (`UITask.cpp:4020`, `FactoryTest.cpp:1264`). Solo quedaba el `#include`.
- **`BluetoothSerial`**: cero referencias en todo `src/`.
- **`pinMode` y `digitalWrite` redeclarados en `main.h`** de la motherBoard,
  con la firma de Arduino y sin definirse en ningun sitio.
- **Toda la capa `drv_*`** (`drv_shtc3`, `drv_sts3x`, `drv_ina3221`) **es
  codigo muerto**: son envoltorios limpios que no llama nadie. Los sensores se
  usan directamente desde `sensors_module.cpp` e `initHardware.cpp`.
- **`src/hal/hal_hmi.*`** del HMI: tampoco lo llama nadie.
- **El canal 5 del PWM (humidificador) nunca se configura**: nadie le llama a
  `ledcSetup` ni le enruta un pin, pero `IncuNest_humidifier.cpp:83` le
  escribe duty. La rama esta dormida (solo se alcanza con
  `activationMode == HUMIDIFIER_PWM`), pero esta muerta. El porte la preserva
  tal cual y ahora avisa por log.

## 7. Solo tras el merge (fuera del alcance de la rama)

- **`Firmware/.claude/`** esta en `.gitignore`: sus reglas, hooks y skills que
  citan `pio run` / `pio test -e native` (11 ficheros) hay que actualizarlos a
  mano en el arbol principal. El hook de Stop pasa a `idf.py build` y a
  `run_host_tests.ps1`.
- **Borrar lo que ya no se usa**, cada cosa en su commit: `platformio.ini` y
  `pre_native.py` de las dos placas, `shared/library.json`,
  `Display_HMI/lib/TAMC_GT911_Fixed` (ahora componente), los `.pio/` locales,
  y el codigo muerto de la seccion 6.
- **Primera ejecucion real de `release.yml`** con un tag de prueba.
- **`Firmware/README.md`** y el `CLAUDE.md` de las placas: comandos de build.

---

## Banco 2026-09-11 (segunda tanda): cuatro fallos encontrados y corregidos

Tras el aviso de banco —sonda de aire desconectada, ventilador oscilando,
alarma de ventilador que ya no se retiraba y caida de la MB— se encontraron
estos cuatro, tres de ellos de una clase que no da la cara compilando.

### 1. `plat_i2c` se quedo sin el mutex de `TwoWire` (REGRESION DEL PORTE)

La `TwoWire` de arduino-esp32 tomaba un mutex en `beginTransmission()` y no lo
soltaba hasta `endTransmission()`/`requestFrom()`. El porte no lo reprodujo, y
**cada bus lo comparten varias tareas**:

    Wire  -> BQ25730 (PWR_MGMT), humidificador (bucle principal),
             INA3221/ambiente (SENSORS), bateria de fabrica (FTEST)
    Wire1 -> SensorBoard (SB_COMM) y sensores de aire STS35/SHTC3 (SENSORS)

La superficie de compatibilidad es una maquina de estados por bus (`tx_buf_`,
`tx_addr_`, `pending_restart_`, `rx_buf_`...), asi que dos tareas intercaladas
se pisan el bufer —una escribe su registro en la direccion de la otra— y
ademas `deviceFor()` muta `devices_[]`/`device_count_` sin proteccion: dos altas
simultaneas con `device_count_ == kMaxDevices-1` escriben en el mismo hueco y
dejan el contador en `kMaxDevices+1`, que ya es corrupcion de memoria.

Con la sonda de aire desconectada, `updateRoomSensor()` falla y llama a
`initRoomSensor()` **en cada ciclo de reconexion** (500 ms), multiplicando las
transacciones de la tarea SENSORS sobre `Wire1` justo mientras SB_COMM la usa.
Es el mejor candidato a la caida que se vio en banco.

Arreglado con un mutex **recursivo** por bus que cubre la transaccion entera
(incluido el START repetido), mas dos cosas que la version de Arduino no tenia:
compensacion de la toma huerfana que deja `Adafruit_I2CDevice::write()` cuando
vuelve con `return false` sin cerrar, y marca de dueño del bufer de RX para que
`read()`/`available()` no sirvan a una tarea los bytes de otra.

De paso, `setTimeOut()` se guardaba pero **no se usaba**: el unico punto que lo
llama (`initRoomSensor`, `wire2->setTimeOut(10)`) lo hace por el coste de
sondear una direccion que no contesta. Ahora se aplica al sondeo; las
transferencias de datos se quedan con el plazo por defecto a proposito.

### 2. `ALARM_FAN_FAILURE` no se podia retirar nunca

Estaba en `ongoingFanCriticalAlarm()`, asi que declararla **cortaba la
alimentacion del ventilador**; sin alimentar, las rpm son 0 por construccion y
`checkFanSpeed()` volvia a declararla. Enclavamiento de hecho, sin reset manual
que lo levante. Es lo que vio el banco: la alarma seguia puesta despues de
corregir el fallo del sensor.

Quitarla no relaja nada: el calefactor lo corta `alarm_cuts_heater()`
(`shared/src/alarm_policy.cpp`), que lista `ALARM_FAN_FAILURE` explicitamente.
Y un ventilador a 2800 rpm sigue moviendo aire; cortarlo a 0 empeoraba la
situacion.

Ademas, las salidas tempranas de `checkFanSpeed()` no **retiraban** la
condicion, y la maquina conserva `present` hasta que alguien declara false:
apagar la actuacion con la alarma puesta la dejaba viva para siempre. Y la
gracia de arranque contaba desde que se ORDENA el ventilador, no desde que se
ALIMENTA, asi que al volver de una subtension el arranque mecanico contaba como
averia.

La decision se extrajo a `modules/control/fan_guard.c` (sin hardware) y tiene
suite propia: `test_fan_guard`, con un caso por cada uno de los fallos.

### 3. Un aviso de alarma que SONABA SIN VERSE

`alarm_banner_update()` (UITask.cpp) colgaba el `lv_obj_clear_flag(HIDDEN)` del
mismo `if` que comprueba si CAMBIO el texto. La misma condicion que se retira y
vuelve deja `wantText` identico al de la vez anterior, asi que el banner no se
des-ocultaba: el zumbador sonaba y en pantalla no habia nada. Media senal de
alarma, que es la mitad que 60601-1-8 no deja omitir. Reportado en banco como
"suena el LINK LOST pero no sale el banner".

Ahora el des-ocultado mira el estado real del objeto (`lv_obj_has_flag`, que es
leer un bit y no invalida nada) en vez del texto, y la salida temprana que se
dejaba `s_bannerText` sin borrar ya lo borra.

### 4. La MB no habia producido NUNCA un coredump utilizable

`CONFIG_ESP_COREDUMP_STACK_SIZE=1024` no llegaba. Un `abort()` limpio terminaba
asi:

    I esp_core_dump_flash: Save core dump to flash...
    I esp_core_dump_common: Backing up stack @ ... use core dump stack @ ...
    Guru Meditation Error: Core 1 panic'ed (Double exception).

y lo que quedaba en flash eran **96 bytes** (cabecera + un program header) que
el arranque siguiente rechazaba con `Core dump data check failed`. Con 4096 el
mismo fallo da `Core dump used 1456 bytes on stack. 2736 bytes left free.` y
`Found core dump 47200 bytes in flash`, y `esp_coredump` lo descodifica entero
(tarea, razon del panic, registros y pilas). El HMI estaba en 1792, que habria
entrado por 336 B: subido tambien.

Reproducible sin red con `INCUNEST_CRASH_TEST=1` (ver `motherBoard/main/CMakeLists.txt`);
el gancho sustituye al `-DCRASH_TEST_MB=1` de `platformio.ini`, que el porte
habia dejado sin efecto sin que nadie lo notara.

## Modo depuracion y bateria de banco

- `motherBoard/src/modules/debug/` y `Display_HMI/src/modules/debug/`: endpoints
  `/debug/*` sobre el servidor web que ya existia, con la autenticacion de
  `/config`. Arrancan APAGADOS, no se persisten, y apagarlos retira de golpe
  toda medida simulada y toda alarma forzada. `/debug/state` es de solo lectura
  y siempre esta.
- La MB simula medidas (se pisan en `sensors_Task`, despues de los sensores
  reales y antes de copiar a `ctrl_tel_msg`) y fuerza CONDICIONES de alarma
  (despues de todos los detectores y antes del tick, asi que el retardo de
  anuncio, la prioridad y el corte de calefactor siguen siendo los de produccion).
- El display INYECTA lineas del protocolo. Van por cola y las drena `Comm_Task`
  en el mismo punto que una linea real, no el manejador HTTP: si no, seria una
  carrera con la tarea Comm en `ctrl_state_msg` y `alarmList`.
- `tools/bench_tests/bench_tests.py`: 14 pruebas contra el hardware real. Deja
  el equipo sin simular pase lo que pase.

## Encoder rotativo retirado

Hardware de una revision anterior. `encoderISR()` no estaba enganchada a
ninguna interrupcion (`initInterrupts()` solo registra `fanEncoderISR`, que es
el TACOMETRO DEL VENTILADOR y se queda). Retirado el objeto, las variables, los
pines (`ENC_A/ENC_B/ENC_SWITCH`, que eran `FAKE_PIN` = GPIO46 y se configuraban
de verdad), las macros y la libreria vendorizada.

---

## Banco 2026-09-11 (tercera tanda): la red no funcionaba de verdad

Con el hotspot ya levantado salieron cuatro cosas mas. Dos las provoque yo con
el propio modo depuracion; las otras dos llevaban ahi desde el porte.

### 5. `available()` de los sockets devolvia SIEMPRE 0 (REGRESION DEL PORTE)

El sintoma que lo destapo fue ridiculo comparado con la causa: el reloj de la
pantalla iba **dos horas atrasado**. El epoch era correcto al segundo —NTP
funciona— pero `tz_known=0`: nadie habia resuelto la zona horaria.

`ensureWifiTimeZoneSynced()` conectaba con ip-api.com, mandaba la peticion y
leia **0 bytes**. La causa: lwIP implementa `ioctl(FIONREAD)` dentro de
`#if LWIP_SO_RCVBUF` (sockets.c), arduino-esp32 traia
`CONFIG_LWIP_SO_RCVBUF=y`, y el porte lo perdio en las DOS placas. Sin el, el
ioctl falla, `WiFiClient::available()` devuelve 0 para siempre y **todo bucle
escrito como `while (client.available())` lee cero bytes sin dar un error**.

Afectaba en silencio a los tres usuarios de sockets planos: la zona horaria, la
geolocalizacion por IP (ambas comprobadas rotas y ahora arregladas: `Timezone
from IP lookup: 8 quarter-hours`, `Position from IP lookup: 38.96, -0.18`) y,
por el mismo camino de `readStringUntil()`, la subida a Drive y la activacion
de SIM.

Arreglado en `sdkconfig.defaults` de las dos placas. Ademas `rawAvailable()` ya
no responde "no hay nada" cuando no puede contar: sondea sin bloqueo y contesta
si/no, que es lo unico que miran los que llaman. Y los tres `return` mudos de
`ensureWifiTimeZoneSynced()` ahora logean por que se van.

### 6. El reset manual de alarmas enclavadas no existia

`alarm_is_latching()` marca los dos cortes termicos —correcto: 201.15.4.2.1
aa)/bb) exige que un corte AUTO-REARMABLE siga avisando "hasta reset manual", y
aqui el corte de calefactor se rearma solo en cuanto baja la temperatura, asi
que la senal es lo unico que deja constancia del episodio.

Pero `alarm_machine_reset()` **no lo llamaba nadie**: la unica forma de quitar
un corte termico ya enfriado era reiniciar la placa. Se encontro en banco
simulando el corte con el modo depuracion.

Cableado: `HMI,ALM_RESET[,<id>]` en la placa, campo 24 `latchedBitmask` en
`CTRL,STATE` (el parser del display ya era tolerante por campo, asi que no
rompe compatibilidad), y en el centro de alarmas la fila de una condicion
enclavada cambia su boton de SILENCIAR a RECONOCER. La politica no se duplica:
`alarm_machine_reset()` rechaza el reset si la condicion sigue presente.

### 7. `/debug/state` se comia la DRAM interna del display (fallo mio)

Pedia 4 KB con `malloc()` —que sirve de la interna primero— y los copiaba a un
`String`: 8 KB de DRAM interna POR PETICION. En la motherBoard sobra; en el
display la interna es EL recurso escaso. Con la bateria consultando en bucle,
la interna paso de 23,8 KB a 5,7 KB y aparecieron `wifi:mem fail`, glitches en
el panel y un HMI LINK LOST fantasma. Ahora va a PSRAM y se envia por la
sobrecarga `const char *`, que no copia: 41 peticiones cuestan 52 B.

Y el JSON informa de la interna y la PSRAM POR SEPARADO, nunca del total:
`esp_get_free_heap_size()` suma las dos y con 7 MB de PSRAM detras tapaba
completamente el problema. La prueba `memoria` de la bateria miraba ese total,
o sea que **no vio la fuga que ella misma provocaba**.

### 8. Subir la pila de coredump del display le dejo sin WiFi (fallo mio)

De 1792 a 4096 son 2304 B de DRAM interna estatica, y en esta placa fueron la
gota: WiFi encontraba el AP pero no se asociaba, con `wifi:m f assoc req` en el
log ("m f" = malloc failed). Devuelto a 1792, que cubre los 1456 B medidos. Si
alguna vez se comprueba que no basta, la DRAM hay que sacarla de otro sitio.

### Estado de la bateria de banco

15/15 en verde contra el hardware real, incluidas las dos que reproducen los
fallos de banco: `ventilador-se-retira` y `corte-termico` (que ademas verifica
que el aviso SIGUE tras enfriarse y que `HMI,ALM_RESET` lo retira).

La motherBoard tambien acepta ahora tramas inyectadas (`/debug/inject` con
lineas `HMI,*`), espejo de lo que ya hacia el display. Sin eso no habia forma
de encender la actuacion ni de reconocer una alarma desde la bateria, y las dos
pruebas de arriba se quedaban sin ejecutar.

### Guarda de placa: verificada (5/5)

`tools/bench_tests/board_guard_test.py`, contra el hardware. Ejercita las dos
barreras POR SEPARADO, porque protegen de cosas distintas:

| caso | resultado |
|---|---|
| binario de la MB al display, cabecera `motherboard` | 400 `esto es un display_hmi` |
| binario del display a la MB, cabecera `display_hmi` | 400 `esto es una motherboard` |
| binario de la MB al display, **sin cabecera** | 400 `el binario es de otra placa` |
| binario del display a la MB, **sin cabecera** | 400 `el binario es de otra placa` |
| binario del display al display, cabecera correcta | **200 OK**, reinicia y vuelve |

Los dos casos sin cabecera son los que importan: reproducen una herramienta que
no declara nada, que es como se estropeo el display del banco el 2026-09-08. Y
el ultimo caso importa igual que los otros cuatro — una guarda que rechaza todo
no es una guarda.

El rechazo por marca llega en `UPLOAD_FILE_END` y hace `Update.abort()`, asi
que `otadata` no se toca y la placa sigue con su firmware. El binario ajeno
queda escrito en el slot inactivo pero inerte: no se puede saber la marca hasta
haber visto el flujo entero, y esa es la contrapartida aceptada.

### Estado final de banco (2026-09-11)

- Tests de host: **26/26**.
- Bateria de banco: **15/15**.
- Guarda de placa: **5/5**.
- Coredump: volcado completo y descodificado (tarea, razon del panic, registros).
- Las dos placas en reposo: sin modo depuracion, sin simulaciones, sin alarmas,
  actuacion apagada, zona horaria resuelta (+2 h) y DRAM interna estable
  (MB 34 KB, HMI 9,5 KB libres / 7,6 KB el mayor bloque).

### 9. El panel RGB se quedaba DESPLAZADO para siempre

Reportado en banco: pantalla corrida por completo al encender el control de
temperatura, sin recuperarse, mas flicker intermitente mientras se simulaban
alarmas.

Lo dificil fue que **el diagnostico decia que todo iba bien**: `LCD_DIAG` daba
48,8 fps y `worst_frame` 20,5 ms perfectamente estables con la pantalla corrida.
LVGL dibujaba bien y el DMA seguia corriendo — solo que desfasado. Los fps no
detectan esto.

`CONFIG_LCD_RGB_RESTART_IN_VSYNC` estaba apagado, y su ayuda en el Kconfig de
`esp_lcd` describe el sintoma palabra por palabra: *"Reset the GDMA channel
every VBlank to stop permanent desyncs from happening"*. Activado.

`CONFIG_LCD_RGB_ISR_IRAM_SAFE` no vale aqui aunque lo parezca: mantiene la ISR
viva con la cache apagada, pero el bufer de rebote se rellena leyendo de PSRAM,
que con la cache apagada tampoco esta. Y esta placa no tiene IRAM que gastar.

Lo que lo disparaba era la propia bateria consultando `/debug/state` en bucle:
el volcado se construye con decenas de snprintf sobre un bufer en PSRAM y
`uxTaskGetSystemState()` recorre todas las TCB, compitiendo por el ancho de
banda de PSRAM con el panel. La tabla de tareas paso a ser **bajo peticion**
(`/debug/state?tasks=1`).

### Enclavamiento de alarmas: politica nueva (2026-09-11)

Por decision del responsable del producto, **los cortes termicos ya NO se
enclavan**: al enfriarse, el equipo vuelve solo y el aviso se retira con la
condicion. El episodio no se pierde — queda en el registro persistido de
alarmas (6.12.2). **La unica alarma latching es ALARM_HEATER_FAULT**, que
declara el autotest de arranque cuando la corriente del calefactor se sale de
rango: revisar ese cableado exige el equipo apagado, asi que la instruccion al
operador es apagar, revisar y volver a encender.

Al hacerlo aparecio que `alarm_is_latching()` estaba haciendo DOS trabajos: la
maquina lo usaba tambien para decidir si una alarma podia esperar su retardo de
anuncio. Colaba mientras las unicas latching eran los cortes termicos, que son
justo las que no pueden esperar. Quitarles el enclavamiento habria RETRASADO su
aviso — una regresion de seguridad. Se separo en `alarm_announces_immediately()`,
con un test que fija que las dos politicas son independientes.
