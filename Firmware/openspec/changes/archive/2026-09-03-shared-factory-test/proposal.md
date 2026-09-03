## Why

Afecta a **shared/** (mensajes nuevos del protocolo y tabla de tests), a
**motherBoard** (ejecutor de la batería de tests y sus pruebas de hardware) y a
**Display_HMI** (botón en la pantalla de arranque, pantalla de resultados y
tests locales del propio display).

Hoy la verificación de una unidad recién montada en fábrica es artesanal: la
motherBoard ejecuta un autotest de arranque (`initHardware()`: corriente en
reposo, zumbador, sensores, actuadores) cuyo resultado solo viaja a
ThingsBoard como bitmask `HW_Test`, y el display no verifica nada de sí mismo.
Todo lo que se añadió después (SensorBoard por USB, AFE4490, GSM, WiFi,
provisionamiento, LittleFS) no tiene ninguna comprobación, y el operario de
montaje no tiene forma de saber, delante del equipo, si una placa tiene un
sensor mal soldado, un conector USB girado, una SIM que no responde o un panel
con una zona muerta. Los fallos se descubren en el laboratorio o en el campo.

Con la nueva generación de hardware (SensorBoard_v2 sustituyendo al bus I2C2)
aparece además un fallo silencioso propio: si los pads de IO19/IO20 quedan
en corto, la motherBoard clasifica la unidad como "equipo antiguo" y el enlace
USB con la SensorBoard no se levanta nunca. Ninguna alarma lo distingue de una
placa antigua sana.

## What Changes

- El display gana un **botón "Test de fábrica"** en la pantalla de arranque
  (`ui_ScreenIntro`). Pulsarlo detiene la transición automática a la pantalla
  principal y abre una **pantalla de test** a pantalla completa con la lista de
  tests, su estado (pendiente / en curso / PASA / FALLA / omitido / esperando
  al operario) y un resumen final. Los tests que requieren un estímulo o una
  confirmación humana (colores del panel, toque en 5 puntos, zumbador y altavoz
  del display, abrir la puerta, tapar el sensor de luz) muestran la instrucción
  y un par de botones Sí / No.
- El display ejecuta **sus propios tests** (flash, PSRAM y heap; presencia I2C
  del GT911 y del STC8H1K28; patrones de color del panel; 5 objetivos de
  toque; zumbador y altavoz; MAC y estado WiFi; escritura y relectura de NVS;
  enlace UART con la motherBoard) y **ordena a la motherBoard** ejecutar los
  suyos.
- La motherBoard gana un **ejecutor de test de fábrica**: una tarea propia que
  recorre en orden una batería de tests y reporta cada resultado al display en
  cuanto termina. Reutiliza `actuatorsTest()` y `testStandByCurrent()` del
  autotest de arranque y añade los que faltan: INA3221 ×2, BQ25730
  (informativo), fuente de alimentación (informativo), ADS1110 + NTC de piel,
  SHT4x exterior, clasificación de fuente de sensores, enumeración USB de la
  SensorBoard, `status` de la SensorBoard, coherencia de las tres SHT40 entre
  sí y con el exterior, puerta, luz, cámara, tacómetro del ventilador, USB_EN /
  USB_FAULT del humidificador, zumbador verificado con el micrófono de la
  SensorBoard, registro SPI del AFE4490, sonda SpO2 (opcional), GSM (AT, SIM,
  señal, red opcional), WiFi al AP por defecto (opcional), provisionamiento en
  ThingsBoard (opcional), hora sincronizada (informativo), NVS y LittleFS.
- La motherBoard **rechaza** el test si el control térmico está activo
  (`in3.actuation != 0`) o si ya hay una batería en curso: los tests de
  actuadores encienden el calefactor y la fototerapia en lazo abierto y no
  pueden pisar un control real.
- El protocolo motherBoard ↔ display gana una familia de mensajes `FTEST`:
  `HMI,FTEST,START` / `HMI,FTEST,RUN,id` / `HMI,FTEST,ABORT` /
  `HMI,FTEST,CONFIRM,id,ok` en un sentido, y `CTRL,FTEST,id,status,detail` /
  `CTRL,FTEST_DONE,pass,fail,skip` / `CTRL,FTEST_REJECT,reason` en el otro.
  Los identificadores de test y los códigos de estado viven en
  `shared/include/factory_test.h`, de forma que las dos placas comparten una
  única tabla. **No es breaking**: una placa antigua que reciba estas líneas las
  descarta como desconocidas, y la nueva trata la ausencia de respuesta como
  "motherBoard sin soporte de test de fábrica" tras un tiempo de espera.
- El módulo `sensorboard_comm` de la motherBoard gana la petición `status`
  (hoy solo `capture`) y expone en el snapshot la versión de firmware y la
  disponibilidad por recurso que devuelve la SensorBoard. Sin eso el test no
  puede saber si las tres SHT40, el ALS, el Hall y la cámara están poblados.
- Ambas placas **persisten el último resultado** en NVS (fecha, versiones de
  firmware de MB, SensorBoard y HMI, MAC, bitmask de PASA / FALLA por test),
  para trazabilidad de qué unidades pasaron fábrica y con qué firmware.
- Se pueden **reintentar tests individuales** fallidos sin repetir la batería.
- Quedan fuera y se dice explícitamente: no se testea el TCA9535 ni el encoder
  (código vestigial sin hardware detrás en la motherBoard), no se cicla el
  backlight del display, no hay test de consumo del zumbador de la motherBoard
  (solo existía en HW ≤ 16), y no se comprueba el HW_NUM ni el interruptor
  ON_OFF (si la placa arrancó y ejecuta el test, ambos funcionaron).

## Capabilities

### New Capabilities
- `factory-test-protocol`: los mensajes `FTEST` del enlace serie, la tabla
  compartida de identificadores y estados de test, y las reglas de parseo y
  descarte de líneas malformadas en ambos sentidos.
- `mb-factory-test`: el ejecutor de test de fábrica de la motherBoard —
  precondiciones, orden y contenido de cada test, criterios de PASA / FALLA /
  omitido, tiempos de espera de los tests con estímulo, reintento individual,
  y persistencia del resultado.
- `hmi-factory-test`: el botón de la pantalla de arranque, la pantalla de test
  del display, sus tests locales, la confirmación del operario, la orquestación
  del test remoto de la motherBoard y la persistencia del resultado.

### Modified Capabilities
<!-- Ninguna: `hmi-link-loss-audio` no cambia de comportamiento. -->

## Impact

- `shared/include/factory_test.h` + `shared/src/factory_test.cpp` (nuevos):
  tabla de tests, códigos de estado, codificador / parser de las líneas
  `CTRL,FTEST` y `HMI,FTEST`. Lógica pura, con tests Unity en
  `motherBoard/test/test_factory_test/` (`pio test -e native`).
- `Firmware/PROTOCOL.md`: nueva sección `FTEST` (v2.3.0).
- motherBoard: `src/modules/factory_test/` (nuevo: secuenciador puro con
  tiempo inyectado + tarea + tests de hardware), `src/tasks/CommTask.cpp`
  (parseo de `HMI,FTEST,*` y emisión de `CTRL,FTEST*`),
  `src/modules/sensorboard_comm/` (petición `status` y campos nuevos del
  snapshot), `src/system/initHardware.cpp` (exponer `actuatorsTest()` y
  `testStandByCurrent()` para reutilizarlas), `include/config/preferences_keys.h`
  (namespace `mb_ftest`).
- Display_HMI: `src/ui/FactoryTest.cpp` + `include/ui/FactoryTest.h` (nuevo
  módulo a mano, mismo patrón que `AlarmCenter`), `src/ui/ElementsCreation.cpp`
  (botón en `ui_ScreenIntro_screen_init`), `src/tasks/UITask.cpp` (enganches
  `_Init` / `_Poll`, excepción en `intro_timer_cb` e `inactivity_timer_cb`),
  `src/tasks/CommTask.cpp` (envío `HMI,FTEST,*` y parseo `CTRL,FTEST*`),
  `include/config/EEPROM_defines.h` (namespace `hmi_ftest`).
- Zonas con bug documentado en `docs/known_issues.md` que este cambio toca:
  #2 (inundación de UART) — los resultados viajan una línea por test al
  terminar, nunca de forma periódica; #5 (arranque con CH340) — el test no
  toca el bootload ni el enlace físico, solo añade tráfico de aplicación.
- Sin dependencias externas nuevas. Sin cambio de particiones.
