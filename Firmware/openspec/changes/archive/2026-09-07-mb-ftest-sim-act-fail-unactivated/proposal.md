## Why

Afecta a **motherBoard**. El change anterior (`mb-ftest-sim-act-warning`)
convirtió todo fallo de `sim_act` en aviso. Es demasiado laxo: una SIM
Onomondo que la API declara **no activada** y que no se ha podido activar no
puede salir de fábrica en amarillo. El aviso solo tiene sentido cuando no se
ha podido consultar la API (sin WiFi, sin respuesta), porque entonces no se
sabe el estado de la SIM.

## What Changes

- `FtestSimState` distingue `FTEST_SIM_ERROR` (la API contestó y la SIM queda
  sin activar: 4xx en el GET, cuerpo sin `activated`, PATCH rechazado) de
  `FTEST_SIM_UNREACHABLE` (sin TLS/HTTP, 429/5xx persistentes, build sin
  clave, sin tarea).
- `sim_act`: ya activada o activada ahora → PASS; `ERROR` → **FAIL**;
  `UNREACHABLE` o plazo agotado (sin WiFi / sin respuesta) → WARN con el
  motivo. Otra SIM → SKIP, como antes.

## Capabilities

### Modified Capabilities
- `mb-factory-test`: criterio de `sim_act`.

## Impact

- `motherBoard/src/modules/factory_test/ftest_sim_activation.{h,cpp}`,
  `factory_test_hw.cpp`, `factory_test_api.h`; `PROTOCOL.md`.
