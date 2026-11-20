## Why

Placas afectadas: **motherBoard** y **shared/** (prioridad del id 20); Display_HMI solo recompila. Corrige los hallazgos de la revisión de código y seguridad de `mb-photo-current-check` (known issues nº 1 y nº 7/11 aplican: alarmas sin parpadeo y tarea Comm del display que se atasca).

## What Changes

- **Sensor MAIN caído**: el lazo de intensidad no regula con un dato congelado ni rancio (exige muestra nueva); `photo_fault` distingue «no aplica» (retira) de «sin medida» (mantiene y, tras 5 ciclos, declara la misma condición).
- **ON de depuración fijo**: la liberación de `photo_override` es por confirmación del display, no por ventana fija; con plazo largo de aviso que nunca adopta un 1 sin confirmar.
- **Exposición por bebé** ya no cuenta la prueba de depuración ni una lámpara en fallo.
- **Prioridad MEDIA** (provisional) para `ALARM_PHOTO_LAMP_FAULT`, citando la Tabla 1 de IEC 60601-1-8.
- Limpieza: `static_assert(ALARM_COUNT <= 32)`, `photo` del keepalive validado a {0,1}, `photoMin` inicializado.

## Capabilities

### Modified Capabilities

- `phototherapy-lamp-monitor`: la condición de fallo de lámpara (prioridad, ceguera del sensor) y la liberación del encendido de depuración.

## Impact

- **motherBoard**: `modules/control/{photo_fault,photo_override}`, `modules/sensors/sensors_module.cpp`, `modules/debug/debug_mode.cpp`, `tasks/CommTask.cpp`, `main.cpp`.
- **shared/**: `alarm_policy.cpp`, `alarm_text.cpp`, `alarm_ids.h`.
- **Despliegue**: flashear el HMI antes que la motherBoard (un display anterior descarta el id 20).
- **Out of scope**: reaplicar la liberación sin keepalive (la lógica solo corre al recibir una orden del display).
