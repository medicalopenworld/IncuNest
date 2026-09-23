## 1. motherBoard

Commit: `perf(motherboard): tests pasivos del test de fabrica en paralelo cooperativo`.

- [x] 1.1 Flag `passive` en la tabla y firma de sondeo `fn(detail, cascade,
      elapsed_ms)` que devuelve RUNNING hasta resolverse.
- [x] 1.2 Runner: RUNNING de todos los pasivos al arrancar, `poll_passives()`
      entre pasos de los activos y al final; cascadas respetadas; ABORT y
      cotas terminan los pasivos pendientes como SKIP.
- [x] 1.3 RUN único de un pasivo por sondeo.
- [x] 1.4 `pio run -e IncuNest_V18` y `pio test -e native` en verde.

## 2. Display_HMI

Commit: `fix(hmi): la pagina sigue al ultimo RUNNING; watchdog por fila a 150 s`.

- [x] 2.1 Fila seguida = último RUNNING recibido; WAIT antes que RUNNING en la
      ordenación.
- [x] 2.2 `FTEST_ROW_TIMEOUT_MS` 150 s.
- [x] 2.3 `pio run -e main` en verde.

## 3. Documentación

- [x] 3.1 `PROTOCOL.md` "Orden de ejecución"; `docs/hardware.md`.

## 4. Verificación manual (banco)

- [ ] 4.1 **Manual** — sin cobertura ni AP la batería termina en ~1 min con los
      mismos resultados que antes.
- [ ] 4.2 **Manual** — la página sigue al test activo aunque haya varios en
      blanco.
