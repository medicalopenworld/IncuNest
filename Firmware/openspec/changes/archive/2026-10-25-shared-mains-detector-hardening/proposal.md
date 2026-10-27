## Why

Placas afectadas: **motherBoard** (detector de red, driver del cargador y `sensors_Task`); Display_HMI y shared/ no cambian. Segunda revisión de seguridad de `shared-mains-detector`: la instancia de producción del detector estaba a `= {}` (el plazo valía 0 por estar la estructura a cero, no por la constante), un chip mudo en el arranque dejaba la red sin vigilar para siempre, una configuración fallida del cargador no se reintentaba y la retirada de la alarma por 2 lecturas permitía oscilar con una fuente intermitente. Toca alarmas (known issue nº 1).

## What Changes

- `MAINS_DETECTOR_INITIALIZER` con la constante, usado por `main.cpp`, más `mains_detector_init()` antes de crear `sensors_Task` y un test que fija que coinciden.
- `CHARGER_EXPECTED` (HW 17 en adelante) decide si el cargador se espera; un chip esperado que no contesta es ceguera, no «sin cargador».
- Reintento de `init_BQ25730()` con espera progresiva 5 s a 5 min cuando el cargador se espera y no está configurado (`retry_backoff`, puro, con test), y limitador de log (uno por episodio y luego cada 5 min).
- Histéresis asimétrica: 2 lecturas para declarar, 6 seguidas con red para retirar.
- Docs: `alarms.md`, checklist de banco (registro 0x04), known issue 23 (alarma técnica pausable pendiente; `charger_comm_fault` por WiFi tras `feat/mb-diag-wifi`).

## Capabilities

### Modified Capabilities

- `mb-mains-interruption-detector`: retirada con 6 lecturas, cargador esperado por hardware, reintento de configuración, instancia de producción con la constante.

## Impact

- **motherBoard**: `modules/control/mains_detector.{h,cpp}`, `modules/util/retry_backoff.{h,cpp}`, `main.cpp`, `system/security.cpp`, `config/board.h`, `platformio.ini`, `test/test_mains_detector`, `test/test_retry_backoff`.
- Sin cambios de protocolo ni de HMI. Cableado sin entorno de test: verificación en banco.
- **Non-goals**: el `AlarmId` técnico (otra rama añade el id 20) y publicar `charger_comm_fault` por WiFi (`feat/mb-diag-wifi`).
