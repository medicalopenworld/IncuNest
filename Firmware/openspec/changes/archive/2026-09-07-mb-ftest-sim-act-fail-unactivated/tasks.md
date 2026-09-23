## 1. motherBoard

Commit: `fix(motherboard): sim_act falla si la API responde y la SIM queda sin activar`.

- [x] 1.1 `FTEST_SIM_UNREACHABLE` en `FtestSimState`; mapeo en
      `simActivationTask()` y en los fallos de arranque (sin clave, sin tarea).
- [x] 1.2 `ftest_sim_act`: ERROR → FAIL, UNREACHABLE/plazo → WARN.
- [x] 1.3 `pio run -e IncuNest_V18` en verde.

## 2. Documentación

- [x] 2.1 `PROTOCOL.md` nota v2.3.3 y fila 28.

## 3. Verificación manual (banco)

- [ ] 3.1 **Manual** — sin WiFi: `sim_act` amarillo `sin wifi`. Con WiFi y SIM
      Onomondo activada: verde `ya activada`. Con una SIM Onomondo no dada de
      alta en la cuenta (GET 404): rojo `get 404`.
