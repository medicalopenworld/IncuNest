## Why

Placas afectadas: **motherBoard** (display_HMI y `shared/` no cambian). Pone al día la spec `phototherapy-lamp-monitor` con lo que la tercera revisión de seguridad cambió después de `mb-photo-override-simplify` (known issues nº 7, nº 10 y nº 11): qué se persiste durante y tras la prueba de depuración, el rearme antes del tick, el cerrojo de caducidad del temporizador de fototerapia y la exclusión de la prueba en `timeTrackHandler`.

## What Changes

- **Persistencia**: durante la prueba se persiste la orden del display (nunca está contaminada, porque el campo 8 emite esa misma orden); al soltar se persiste el valor aplicado; el ON forzado nunca se persiste.
- **Rearme antes del tick**: `PHOTO,1`, `PHOTO,0`, `PHOTO,1` no captura el `in3.phototherapy` contaminado como orden del display.
- **Cerrojo de caducidad**: tras vencer el temporizador se ignora `photo=1` sin minutos hasta ver `photo=0`, dentro y fuera de la prueba.
- **`timeTrackHandler`** no suma la prueba a las horas de fototerapia.
- **Prohibición de fade LEDC** en el canal de la lámpara, solo documentada (sin comprobación en tiempo de ejecución).

## Capabilities

### Modified Capabilities

- `phototherapy-lamp-monitor`: requisito de la liberación de la prueba (MODIFIED) y requisito nuevo del cerrojo de caducidad (ADDED).

## Impact

- **motherBoard**: `modules/control/{photo_override,photo_expiry_latch}`, `main.cpp`, `tasks/CommTask.cpp`, `system/updateData.cpp`, `modules/sensors/sensors_module.cpp`, `include/config/board.h`.
- **Docs**: `PROTOCOL.md`, `docs/known_issues.md` §20 (ya actualizados).
