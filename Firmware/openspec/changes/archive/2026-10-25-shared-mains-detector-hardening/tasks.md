## 1. motherBoard — lógica pura (TDD en `[env:native]`)

- [x] 1.1 Tests en rojo: inicializador de producción, histéresis asimétrica, espera progresiva y limitador de log
- [x] 1.2 `mains_detector`: `MAINS_DETECTOR_INITIALIZER`, `CONFIRM_ABSENT/PRESENT_SAMPLES`
- [x] 1.3 `modules/util/retry_backoff.{h,cpp}`
- [x] 1.4 `pio test -e native` en verde

## 2. motherBoard — cableado (sin entorno de test)

- [x] 2.1 `CHARGER_EXPECTED` en `board.h`; `chargerExpectedNow()` en `main.cpp` y `security.cpp`
- [x] 2.2 Reintento de `init_BQ25730()` con espera progresiva y log limitado; log limitado de la reconfiguración
- [x] 2.3 `g_mains_detector` con el inicializador y `mains_detector_init()` antes de crear `sensors_Task`
- [x] 2.4 `pio run -e IncuNest_V18` en verde

## 3. Verificación manual en banco

- [ ] 3.1 **Manual**: fuente intermitente: la alarma se mantiene continua
- [ ] 3.2 **Manual**: chip mudo al arrancar (HW 17+): `logE` y `charger_comm_fault` a los 30 s y configuración en el reintento al volver el chip
- [ ] 3.3 **Manual**: leer el registro 0x04 (MaxChargeVoltage) tras un ciclo de red con la reconfiguración fallando

## 4. Documentación

- [x] 4.1 `docs/alarms.md`, `docs/alarms_bench_verification.md`, `docs/known_issues.md` (23)
