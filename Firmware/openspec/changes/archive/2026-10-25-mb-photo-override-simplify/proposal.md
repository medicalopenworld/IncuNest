## Why

Placas afectadas: **motherBoard** (display_HMI no cambia; `shared/` solo gana un `_Static_assert`). La segunda revisión de seguridad marcó la liberación confirmada de `photo_override` como regresión alta (known issues nº 7 y nº 11, tarea Comm del display atascada): con la fototerapia real ON, un temporizador vencido durante la liberación hacía que el ON se auto-confirmara y se guardara como sesión continua; un OFF del operador se perdía; y una trama rancia del anillo de RX del display se colaba. Además había una carrera entre el lazo de intensidad y el apagado desde la tarea Comm.

## What Changes

- **Raíz eliminada**: durante la prueba el campo 8 de `CTRL,STATE` emite la **orden del display**, no el ON forzado. El display nunca adopta el ON de depuración.
- **Soltar es inmediato**: se aplica la última orden del display (registrada durante la prueba, con la caducidad del temporizador como orden de apagado), en el tick periódico, sin trama y sin persistir. Se elimina la ventana de gracia, la confirmación, el mínimo de 2 s y el respaldo de 60 s.
- **PWM serializado** (`photoPwmRefresh()` bajo spinlock) entre el lazo de intensidad y `applyPhotoState()`.
- `_Static_assert` en C junto al `static_assert` de `ALARM_COUNT`.

## Capabilities

### Modified Capabilities

- `phototherapy-lamp-monitor`: el campo 8 (con su excepción) y la liberación de la prueba de depuración.

## Impact

- **motherBoard**: `modules/control/photo_override.{h,cpp}`, `modules/debug/debug_mode.{h,cpp}`, `main.cpp`, `tasks/CommTask.cpp`, `modules/sensors/sensors_module.cpp`, `include/main.h`.
- **shared/**: `include/alarm_ids.h`.
- **Docs**: `PROTOCOL.md`, `docs/known_issues.md` §20.
