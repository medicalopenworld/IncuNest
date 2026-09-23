## 1. shared/ — tabla y codec

Commit: `feat(shared): test sensorboard unico, estado WARN y cuarto campo de FTEST_DONE`.

- [x] 1.1 `factory_test.h`: `FTEST_MB_SENSORBOARD` en lugar de `SENSOR_SRC` +
      `SB_LINK` (`FTEST_MB_COUNT` = 29), `FTEST_WARN = 6`, `ftest_format_done`
      / `ftest_parse_done` con `warn`.
- [x] 1.2 **RED** — tests: WARN codifica/parsea, status 7 rechazado, DONE con
      3 y 4 campos, DONE con 5 → false, clave `sensorboard`.
- [x] 1.3 **GREEN** — `factory_test.cpp`; `ftest_summary` con `warn` y
      `warn_mask` (+ tests). `pio test -e native` en verde.

## 2. motherBoard

Commit: `fix(motherboard): test de fabrica tras el banco (sensorboard unico, WARN, flags GSM)`.

- [x] 2.1 `GPRS.h/.cpp`: flags `modemResponded` y `simReady` en
      `GPRSPowerUp()`.
- [x] 2.2 `factory_test_hw.cpp`: `ftest_sensorboard` (D1) y cascada `sb_usb`;
      `gsm_at`/`gsm_sim` con los flags nuevos; `gsm_net`/`wifi`/`tb_provision`
      /`time` a 30 s y WARN.
- [x] 2.3 `factory_test_task.cpp`: `FTEST_DONE` con `warn`; NVS `mb_ftest/warn`.
- [x] 2.4 `pio run -e IncuNest_V18` en verde.

## 3. Display_HMI

Commit: `fix(hmi): boton HW test centrado y armado, cuadricula de tests, sin panel ni toque`.

- [x] 3.1 Botón "HW test" en `LV_ALIGN_BOTTOM_MID`, sin ext click area,
      armado a 1,5 s y comprobación del punto del indev (D5).
- [x] 3.2 Quitar `HMI_PANEL` y `HMI_TOUCH` de la secuencia y su código.
- [x] 3.3 Cuadrícula de 3 columnas ordenada por severidad, SKIP ocultos,
      panel de detalle con descripción/valor/Reintentar (D4).
- [x] 3.4 `CommTask.cpp`: cuarto campo de `FTEST_DONE`; WARN como final;
      resumen "errores · avisos · OK"; NVS `hmi_ftest/mb_warn`.
- [x] 3.5 `pio run -e main` en verde.

## 4. Documentación

Commit: `docs: protocolo FTEST v2.3.1 y pantalla HW test`.

- [x] 4.1 `PROTOCOL.md` v2.3.1 (tabla renumerada, WARN, `FTEST_DONE` de 4
      campos, criterios GSM, plazos de 30 s).
- [x] 4.2 `docs/hmi.md` (botón HW test, cuadrícula, sin panel/toque) y
      `docs/hardware.md` (fila `sensorboard`).

## Evidencia del stage Verify (2026-09-05, sin hardware conectado)

HEAD `111dd7c`, worktree `Firmware/.worktrees/factory-test`, PowerShell:

| Comando | Resultado |
|---|---|
| `motherBoard: pio run -e IncuNest_V18` | SUCCESS, RAM 26.3 %, Flash 54.4 % (1497937 B), 0 warnings |
| `motherBoard: pio test -e native` | 20 suites PASSED (297 casos, +9 de WARN/DONE/sensorboard/warn_mask); `test_sensorboard_frame` ERRORED por la DLL preexistente de esta máquina |
| `Display_HMI: pio run -e main` | SUCCESS, RAM 38.6 %, Flash 78.7 % (2474640 B), 0 warnings |

Las tareas 1.x–4.x están hechas; la sección 5 es banco.

## 5. Verificación manual (banco)

- [ ] 5.1 **Manual** — con SensorBoard: `sensorboard` PASA "usb" aunque el
      sondeo I2C2 haya clasificado la unidad como antigua.
- [ ] 5.2 **Manual** — módem ya arrancado antes de pulsar: `gsm_at` y
      `gsm_sim` PASAN sin esperar 45 s.
- [ ] 5.3 **Manual** — sin AP ni cobertura: `wifi`, `gsm_net`, `tb_provision`
      y `time` en ámbar a los 30 s; la cuadrícula los muestra tras los fallos.
- [ ] 5.4 **Manual** — el splash no abre el test sin pulsar; el botón
      responde a partir de 1,5 s.
