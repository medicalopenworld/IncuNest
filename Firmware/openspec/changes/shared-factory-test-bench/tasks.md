## 1. shared/ — tabla y codec

Commit: `feat(shared): test sensorboard unico, estado WARN y cuarto campo de FTEST_DONE`.

- [ ] 1.1 `factory_test.h`: `FTEST_MB_SENSORBOARD` en lugar de `SENSOR_SRC` +
      `SB_LINK` (`FTEST_MB_COUNT` = 29), `FTEST_WARN = 6`, `ftest_format_done`
      / `ftest_parse_done` con `warn`.
- [ ] 1.2 **RED** — tests: WARN codifica/parsea, status 7 rechazado, DONE con
      3 y 4 campos, DONE con 5 → false, clave `sensorboard`.
- [ ] 1.3 **GREEN** — `factory_test.cpp`; `ftest_summary` con `warn` y
      `warn_mask` (+ tests). `pio test -e native` en verde.

## 2. motherBoard

Commit: `fix(motherboard): test de fabrica tras el banco (sensorboard unico, WARN, flags GSM)`.

- [ ] 2.1 `GPRS.h/.cpp`: flags `modemResponded` y `simReady` en
      `GPRSPowerUp()`.
- [ ] 2.2 `factory_test_hw.cpp`: `ftest_sensorboard` (D1) y cascada `sb_usb`;
      `gsm_at`/`gsm_sim` con los flags nuevos; `gsm_net`/`wifi`/`tb_provision`
      /`time` a 30 s y WARN.
- [ ] 2.3 `factory_test_task.cpp`: `FTEST_DONE` con `warn`; NVS `mb_ftest/warn`.
- [ ] 2.4 `pio run -e IncuNest_V18` en verde.

## 3. Display_HMI

Commit: `fix(hmi): boton HW test centrado y armado, cuadricula de tests, sin panel ni toque`.

- [ ] 3.1 Botón "HW test" en `LV_ALIGN_BOTTOM_MID`, sin ext click area,
      armado a 1,5 s y comprobación del punto del indev (D5).
- [ ] 3.2 Quitar `HMI_PANEL` y `HMI_TOUCH` de la secuencia y su código.
- [ ] 3.3 Cuadrícula de 3 columnas ordenada por severidad, SKIP ocultos,
      panel de detalle con descripción/valor/Reintentar (D4).
- [ ] 3.4 `CommTask.cpp`: cuarto campo de `FTEST_DONE`; WARN como final;
      resumen "errores · avisos · OK"; NVS `hmi_ftest/mb_warn`.
- [ ] 3.5 `pio run -e main` en verde.

## 4. Documentación

Commit: `docs: protocolo FTEST v2.3.1 y pantalla HW test`.

- [ ] 4.1 `PROTOCOL.md` v2.3.1 (tabla renumerada, WARN, `FTEST_DONE` de 4
      campos, criterios GSM, plazos de 30 s).
- [ ] 4.2 `docs/hmi.md` (botón HW test, cuadrícula, sin panel/toque) y
      `docs/hardware.md` (fila `sensorboard`).

## 5. Verificación manual (banco)

- [ ] 5.1 **Manual** — con SensorBoard: `sensorboard` PASA "usb" aunque el
      sondeo I2C2 haya clasificado la unidad como antigua.
- [ ] 5.2 **Manual** — módem ya arrancado antes de pulsar: `gsm_at` y
      `gsm_sim` PASAN sin esperar 45 s.
- [ ] 5.3 **Manual** — sin AP ni cobertura: `wifi`, `gsm_net`, `tb_provision`
      y `time` en ámbar a los 30 s; la cuadrícula los muestra tras los fallos.
- [ ] 5.4 **Manual** — el splash no abre el test sin pulsar; el botón
      responde a partir de 1,5 s.
