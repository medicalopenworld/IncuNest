## Why

Afecta a **motherBoard** y, en un detalle de presentación, a **Display_HMI**.
Banco 2026-09-07 con la batería del test de hardware:

- `sim_act` (activación de la SIM en la API de Onomondo) daba **FAIL** sin
  WiFi en la nave. Activar la SIM necesita red externa y una API viva; que
  falten no es un fallo de la placa, igual que `wifi` o `tb_provision`. Y el
  test corría con cualquier SIM, aunque la API solo acepta ICCIDs de
  Onomondo.
- El contador de la barra decía `34 tests` mientras la cabecera sumaba 1
  error + 5 avisos + 20 OK = 26. Los 8 que faltan son los omitidos (SKIP),
  que no se muestran ni se cuentan en la cabecera pero seguían en el total.

## What Changes

- `sim_act` solo corre si el ICCID empieza por `894573` (prefijo Onomondo,
  el mismo que valida el endpoint `/sims/{id}`); con otra SIM es SKIP `sim
  no onomondo`. Si no puede activar (sin WiFi, sin respuesta, error de la
  API) termina en **WARN** con el motivo, no en FAIL.
- La barra y su contador excluyen los omitidos: hechos = terminales que no
  son SKIP, total = esperados menos omitidos. Cuadra con la cabecera.

## Capabilities

### New Capabilities
<!-- Ninguna. -->

### Modified Capabilities
- `mb-factory-test`: criterio y condición de `sim_act`.
- `hmi-factory-test`: contador de progreso sin omitidos.

## Impact

- `motherBoard/src/modules/factory_test/factory_test_hw.cpp`,
  `factory_test_api.h` (prefijo y comentario del plazo).
- `Display_HMI/src/ui/FactoryTest.cpp` (`renderVerdictAndProgress`).
- `PROTOCOL.md` (nota v2.3.3 y fila 28), `docs/hardware.md`.
