# Diseño

**H6 — asimetría.** Declarar rápido (2 lecturas, ≈ 10 s) y retirar despacio (6 seguidas con red, ≈ 30 s; cualquier ausencia reinicia la racha). Con una fuente intermitente, retirar con pocas lecturas hace oscilar la alarma: cada vuelta es una entrada de historial, una escritura en flash y un rearme de audio. Con 6, la fuente tiene que estar estable ≈ 30 s para retirar.

**H1 — la constante manda.** `MAINS_DETECTOR_INITIALIZER` usa `MAINS_ALARM_MIN_HOLD_MS`; `g_mains_detector` lo usa y `setup()` llama a `mains_detector_init()` antes de crear la tarea. Un test compara el inicializador con `mains_detector_init()` campo a campo y con la constante.

**H2 — esperado por hardware.** `CHARGER_EXPECTED` = 1 con `HW_NUM >= 17` (el esquema V16_A no trae BQ25730; V17_A sí). `chargerExpectedNow() = CHARGER_EXPECTED || chargerDetected` decide el bucle del cargador, la ceguera y el «sin cargador» silencioso (solo HW sin BQ). Un chip mudo en una placa que lo lleva cuenta como ceguera a los 30 s (`logE` y `charger_comm_fault`).

**H4 — reintento.** Con el cargador esperado y `chargerPresent == false`, `sensors_Task` reintenta `init_BQ25730()` con `Backoff` (5, 10, 20 ... 300 s); el éxito lo reinicia. Pura y probada (`retry_backoff`). **H5**: `LogLimiter` (uno por episodio y luego cada 5 min) para el reintento y para la reconfiguración fallida.

**Deuda (known issue 23).** Con la alarma declarada y el cargador ciego sigue ALTA sin fin: lo correcto es una alarma técnica pausable, que necesita un `AlarmId` y espera a la integración de ramas.
