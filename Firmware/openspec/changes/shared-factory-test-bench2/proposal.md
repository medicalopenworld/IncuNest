## Why

Afecta a **shared/**, **motherBoard** y **Display_HMI**. Tercera vuelta del
test de fábrica tras la segunda prueba en banco (2026-09-06): motherBoard V18
alimentada solo con batería, SensorBoard conectada, sin SHT4x exterior.

Lo que se vio:

- `charger` se quedaba en "en curso". La tarea FTEST llamaba a
  `charge_status()` (varias transacciones I2C encadenadas) mientras
  `PowerManagement_Task` y `sensors_Task` usan el mismo `Wire` sin ningún
  mutex de bus: las transacciones se entrelazan. El mismo riesgo afectaba a
  `skin_adc` y a la comparación con el exterior de `sb_env`.
- `ext_sht4x` daba FAIL porque la unidad no lleva SHT4x: el hardware monta
  SensorBoard **o** sensor ambiental, nunca ambos.
- `HMI_I2C` daba FAIL en un display que funciona: un `endTransmission()`
  vacío no es una prueba fiable de presencia del STC8H1K28 ni del GT911; la
  evidencia real es que sus inits del arranque funcionaron.
- Sin cobertura, `gsm_signal` salía en rojo; conectarse es opcional.
- El humidificador y el zumbador/altavoz del display no se pueden verificar en
  el jig actual: se omiten por ahora.
- La cuadrícula con scroll no se maneja bien con guantes; el operario quiere
  páginas, una barra de progreso y un veredicto final claro.

## What Changes

- **`env_sensor` (id 6)** sustituye a `sensorboard` (7) y `ext_sht4x` (6):
  PASA si cualquiera de los tres caminos (SensorBoard por USB, STS35/SHTC3 por
  I2C2, SHT4x exterior) entrega una lectura fresca y en rango; el camino va
  en el detalle. `FTEST_MB_COUNT` pasa de 29 a 28. **BREAKING** dentro de
  `FTEST` (renumeración), sin placas en campo con la versión anterior.
- **Regla nueva: los tests de la MB no hacen I2C directo** salvo dentro de
  `actuatorsTest()` / `testStandByCurrent()`. `charger`, `skin_adc`,
  `ina3221`, `env_sensor` y `sb_env` leen el estado que ya mantienen
  `PowerManagement_Task` y `sensors_Task` (`g_bq_status_valid`,
  `lastSuccesfullSensorUpdate[]`, `in3.temperature[]`, flags de presencia).
  `charger` PASA si el BQ25730 responde, con o sin alimentación externa.
- `humid_usb` devuelve SKIP `omitido` sin tocar `USB_EN`. `gsm_signal` termina
  en WARN `sin señal` en vez de FAIL.
- **Display**: `HMI_BUZZER` y `HMI_SPEAKER` salen de la secuencia local;
  `HMI_I2C` decide con el resultado del init del GT911 y del backlight del
  arranque (el sondeo de direcciones queda informativo); la cuadrícula se
  **pagina** (3 columnas × N filas, botones < >, indicador i/n) en vez de
  hacer scroll; abajo una **barra de progreso** y el **veredicto** en grande:
  `HW OK` en verde si no hay ningún FALLA, `HW ERROR` en rojo si hay alguno
  (los avisos no cambian el veredicto). El veredicto se persiste.

## Capabilities

### New Capabilities
<!-- Ninguna. -->

### Modified Capabilities
- `factory-test-protocol`: tabla (ids 6/7 fusionados en `env_sensor`,
  `FTEST_MB_COUNT` = 28).
- `mb-factory-test`: `env_sensor` unificado, regla de no-I2C directo,
  criterio de `charger`, `humid_usb` omitido, `gsm_signal` WARN, `sb_env`
  sin exterior obligatorio.
- `hmi-factory-test`: secuencia local sin zumbador ni altavoz, criterio de
  `HMI_I2C`, cuadrícula paginada, barra de progreso y veredicto.

## Impact

- `shared/include/factory_test.h`, `shared/src/factory_test.cpp`,
  `motherBoard/test/test_factory_test/`.
- motherBoard: `factory_test_hw.cpp` (tests 1, 3, 5, 6, 9, 15, 22), cabecera
  del fichero con la regla de I2C.
- Display_HMI: `src/ui/FactoryTest.cpp` (páginas, barra, veredicto, I2C,
  secuencia), `src/tasks/UITask.cpp` (getters del init de touch y backlight).
- `Firmware/PROTOCOL.md` (v2.3.2), `docs/hmi.md`, `docs/hardware.md`.
