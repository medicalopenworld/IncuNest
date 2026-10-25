## Why

Placas afectadas: **motherBoard** (cableado del detector de red y driver del cargador) y **Display_HMI** (indicador del heading de Main). **shared/** no cambia. Corrige `shared-mains-detector` tras la revisión de seguridad, que lo declaró NO APTO por cómo estaba enganchado, no por el detector. Toca alarmas (known issue nº 1).

La reconfiguración del cargador al volver la red (`init_BQ25730()`) bajaba `chargerPresent` y, si fallaba una de sus ~11 operaciones I2C, lo dejaba a false para siempre: la placa dejaba de leer el cargador y de vigilar la red. Si salía bien, `securityCheck()` veía igualmente el false transitorio y retiraba/levantaba la alarma. Además, un cargador que dejaba de responder congelaba el detector sin avisar. El plazo de 10 min contradecía 6.10 y mostraba «BATERÍA» en falso, y la palabra del indicador pisaba la fecha del reloj en Main.

## What Changes

- **A1.** `chargerDetected` (se fija en el arranque, no baja nunca) separado de `chargerPresent`; `reconfigure_BQ25730()` no baja `chargerPresent`; el detector y el bucle del cargador usan `chargerDetected`; la reconfiguración fallida se registra (`logE`) y se reintenta. El log de «no se vigila la red» sale solo si el cargador nunca se detectó, como error.
- **A2.** `mains_detector_blind()` (módulo puro, con tests): cargador detectado sin lectura válida > 30 s = ceguera. Se hace visible por log y por la clave `charger_comm_fault` (grupo DIAG, GPRS). Con la alarma ya declarada, se mantiene. No se añade `AlarmId` (otra rama añade el id 20): queda anotado en known issues.
- **A3.** `MAINS_ALARM_MIN_HOLD_MS = 0`: la alarma se retira al confirmarse el retorno de la red. Se conservan constante y mecanismo.
- **M1.** Main lleva solo el icono (en la fila de la fecha); la palabra queda en Lock.
- **M2.** Se elimina el filtro `vbat_mv == 0` (`vbat_mv = adc*64 + 2880` nunca es 0).
- **L2.** Comentario de hilos corregido.

## Capabilities

### Modified Capabilities

- `mb-mains-interruption-detector`: plazo mínimo 0 por defecto; lectura no válida sin el criterio `vbat_mv`; «sin cargador» pasa a «no detectado en el arranque»; reconfiguración que no apaga la vigilancia; ceguera visible.
- `hmi-on-battery-indicator`: Main solo icono, Lock icono y palabra; se retira al confirmarse el retorno de la red.

## Impact

- **motherBoard**: `src/drivers/BQ25730.{h,cpp}`, `src/main.cpp`, `src/system/security.cpp`, `src/modules/control/mains_detector.{h,cpp}`, `include/config/telemetry_keys.h`, `src/tasks/GPRS.cpp`, `test/test_mains_detector/`.
- **Display_HMI**: `src/ui/ElementsCreation.{cpp}`, `include/ui/ElementsCreation.h`, `src/tasks/UITask.cpp`.
- **Docs**: `docs/alarms.md` §1 y §9, `docs/alarms_bench_verification.md` (casos 19 y 20), `docs/known_issues.md`, `docs/hmi.md`.
- **Telemetría**: +1 clave en el grupo DIAG de GPRS; peor caso GPRS 93 de 112 campos (`tools/check_transport_matrix.py`).
- **Sin cambios de protocolo.**
- **Testing**: lo puro, en `[env:native]`. El cableado y el HMI siguen sin entorno de test: verificación en banco, marcada como manual.
- **Non-goals**: un `AlarmId` de fallo técnico del cargador (espera a integrar las ramas); el pin de la V19.
