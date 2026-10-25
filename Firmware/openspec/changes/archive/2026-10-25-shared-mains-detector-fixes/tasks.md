## 1. motherBoard — lógica pura (TDD en `[env:native]`)

- [x] 1.1 Tests en rojo: plazo 0 por defecto, mecanismo con plazo de prueba, sin filtro `vbat_mv`, ceguera (commit `test(motherboard): failing specs for mains detector fixes`)
- [x] 1.2 `mains_detector`: plazo por defecto 0, `mains_detector_init_hold()`, `mains_detector_blind()`, firma de `sample()` sin `vbat_mv`, comentario de hilos corregido
- [x] 1.3 `pio test -e native` en verde en motherBoard

## 2. motherBoard — cableado (sin entorno de test)

- [x] 2.1 `chargerDetected` y `reconfigure_BQ25730()` sin tocar `chargerPresent` (`BQ25730.{h,cpp}`)
- [x] 2.2 `main.cpp`: bucle del cargador por `chargerDetected`, reconfiguración con `logE` y reintento, ceguera → `g_charger_comm_fault` y `logE`
- [x] 2.3 `security.cpp`: `checkMainsInterruption()` por `chargerDetected`, log `logE` una vez
- [x] 2.4 Clave `charger_comm_fault` en el grupo DIAG de GPRS; `python tools/check_transport_matrix.py` (presupuesto)
- [x] 2.5 `pio run -e IncuNest_V18` en verde

## 3. Display_HMI (sin entorno de test)

- [x] 3.1 Main solo icono (14 pt, fila de la fecha); Lock icono y palabra
- [x] 3.2 `pio run -e main` en verde

## 4. Verificación manual en banco

- [ ] 4.1 **Manual**: captura del heading de Main con el indicador en ES y FR, con y sin hora, sin solape
- [ ] 4.2 **Manual**: retorno de red: la alarma se retira a los ≈ 10 s sin retirada/levantada espuria en el historial
- [ ] 4.3 **Manual**: desconectar el BQ25730 del bus con la alarma declarada: se mantiene y a los 30 s sale `charger_comm_fault`

## 5. Documentación

- [x] 5.1 `docs/alarms.md` §1 y §9, `docs/alarms_bench_verification.md` (casos 19 y 20), `docs/known_issues.md` (#23), `docs/hmi.md`
