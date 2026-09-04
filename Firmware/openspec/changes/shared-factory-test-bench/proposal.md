## Why

Afecta a **shared/**, **motherBoard** y **Display_HMI**. Es la segunda vuelta
del test de fábrica (`shared-factory-test`, archivado el 2026-09-03) tras la
primera prueba en banco del 2026-09-04/05 con motherBoard V18, SensorBoard y
HMI reales.

Lo que se vio en banco:

- El test `sensor_src` dio FAIL con detalle `i2c2 responde` en una unidad con
  SensorBoard conectada. La clasificación de generación sondea el bus I2C2
  sobre las mismas líneas que son D+/D− del USB, y un `endTransmission()` con
  SDA retenida a nivel bajo devuelve ACK para cualquier dirección. Separar
  "origen" de "enlace" convertía una peculiaridad del sondeo en un fallo de
  fábrica; lo que importa es que la cabina tenga sensor por el camino que sea.
- Los tests de panel (colores) y de toque alargan la batería sin aportar en
  la línea de montaje actual: se retiran.
- `gsm_sim` exigía el CCID; en banco basta con que el módem responda
  `+CPIN: READY`. Además, el test `gsm_at` esperaba `GPRS.powerUp`, que es
  verdadero solo **durante** la secuencia de arranque del módem, así que
  fallaba si el módem ya había arrancado antes de pulsar el botón.
- Los tests de conectividad (red GSM, WiFi, ThingsBoard, hora) acababan en
  SKIP o FAIL según el caso; en fábrica sin cobertura ni AP eso debe verse
  como **aviso**, en ámbar, con un plazo corto.
- La lista de 40 filas con SKIP intercalados no se lee en un panel de 7″: el
  operario quiere ver primero lo que falla.

## What Changes

- **Un solo test de sensor de cabina** (`sensorboard`, id 7) sustituye a
  `sensor_src` (7) y `sb_link` (8): PASA si llegan lecturas válidas de cabina
  por USB (SensorBoard enlazada) **o** por I2C2 (equipo antiguo), con el
  camino en el detalle (`usb` / `i2c`). Los tests `sb_status`, `sb_env`,
  `sb_door`, `sb_light`, `sb_camera` siguen exigiendo el camino USB y salen
  SKIP en el camino I2C. `FTEST_MB_COUNT` pasa de 30 a 29. **BREAKING** para
  el protocolo `FTEST` (renumeración de ids), aceptable: ninguna placa en
  campo lleva la versión anterior.
- **Estado nuevo `WARN` (6)**: final, cuenta aparte en `CTRL,FTEST_DONE`
  (cuarto campo `warn`, opcional al parsear). Lo usan `gsm_net`, `wifi`,
  `tb_provision` y `time` cuando agotan su plazo, ahora de 30 s cada uno.
- `gsm_at` PASA si el módem ha respondido a algún AT (`GPRS.modemResponded`,
  flag nuevo) o ya está adjunto; `gsm_sim` PASA con `+CPIN: READY`
  (`GPRS.simReady`, flag nuevo), sin exigir CCID.
- **Display**: se retiran `HMI_PANEL` y `HMI_TOUCH` de la secuencia local
  (los ids quedan en la tabla). La lista se sustituye por una **cuadrícula de
  3 columnas de botones**, ordenada FALLA → AVISO → en curso → PASA, con las
  filas SKIP ocultas; cada botón lleva el título del test y al tocarlo abre un
  panel con la descripción, el estado, el valor medido y Reintentar si aplica.
  El resumen muestra fallos, avisos y correctos; los omitidos no se muestran.

## Capabilities

### New Capabilities
<!-- Ninguna nueva: se modifican las tres existentes. -->

### Modified Capabilities
- `factory-test-protocol`: tabla (ids 7/8 fusionados, `FTEST_MB_COUNT` = 29),
  estado `WARN`, cuarto campo de `FTEST_DONE`.
- `mb-factory-test`: test `sensorboard` unificado, criterios de `gsm_at` y
  `gsm_sim`, plazos de 30 s y resultado `WARN` en los tests de conectividad.
- `hmi-factory-test`: secuencia local sin panel ni toque, cuadrícula de
  botones con detalle al tocar, SKIP ocultos, WARN en ámbar.

## Impact

- `shared/include/factory_test.h`, `shared/src/factory_test.cpp`,
  `motherBoard/test/test_factory_test/`.
- motherBoard: `factory_test_hw.cpp` (tests 7, 21–24, 25–27),
  `factory_test_task.cpp` / `ftest_summary` (contador `warn`, NVS `warn`),
  `include/tasks/GPRS.h` + `src/tasks/GPRS.cpp` (flags `modemResponded`,
  `simReady`).
- Display_HMI: `src/ui/FactoryTest.cpp` (cuadrícula, panel de detalle,
  filtro de SKIP), `src/tasks/CommTask.cpp` (cuarto campo de `FTEST_DONE`).
- `Firmware/PROTOCOL.md` (v2.3.1), `docs/hmi.md`, `docs/hardware.md`.
