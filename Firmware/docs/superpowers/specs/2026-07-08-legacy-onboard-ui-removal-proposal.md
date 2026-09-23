# Eliminación de la UI on-board legacy — Propuesta

**Fecha:** 2026-07-08
**Status:** Ejecutada (fases 0-3) y validada en hardware real (`IncuNest_V16`/`V17`): actuación por HMI, botón físico del encoder (mute/reconocimiento de alarma), arranque con `restoreState`, y sensores/logging tras la migración a `modules/sensors/`.

**Post-validación**: `resetCalibration()`/`saveCalibrationToEEPROM()` (código muerto en `EEPROM.cpp`, sin llamador tras el borrado del proceso de calibración) se han eliminado. `alarmTimerStart()` ahora acepta `assumeStabilized` (default `false`); el arranque con `restoreState` (`initHardware.cpp`) lo llama con `true` para no exigirle a temperatura/humedad esperar de nuevo la ventana de estabilización tras resumir una sesión que ya estaba en marcha.

## Contexto

`motherBoard/src/legacy/` (7 ficheros, ~3244 líneas) contiene lo que quedó de la UI on-board original del motherBoard (pantalla + encoder rotatorio), de cuando el ESP32 gestionaba la interfaz directamente. Hoy toda la UI la gestiona `Display_HMI` vía protocolo serie (`Firmware/PROTOCOL.md`). El objetivo es retirar el código on-board que ya no cumple ninguna función, sin romper lo que sí sigue vivo.

**Hallazgo clave**: `legacy/` no es homogénea. Mezcla UI verdaderamente muerta con lógica funcional que solo está mal ubicada en esa carpeta (p.ej. `sensors.cpp` contiene `currentMonitor()`/`measureMeanConsumption()`, en uso activo — lo tocamos en esta misma sesión para el fix de seguridad del ramp del heater).

## Mapa por fichero

| Fichero | Naturaleza | Veredicto |
|---|---|---|
| `drawGraphicInterphace.cpp` (1036 líneas) | Renderizado TFT_eSPI puro | UI muerta — borrable (fase 4) |
| `sensors.cpp` (626 líneas) | `currentMonitor()`/`measureMeanConsumption()`/filtros Butterworth — **lógica viva** | No borrar — migrar (fase 1) |
| `UI_actuatorsProgress.cpp` (318 líneas) | Pantalla de progreso + efectos de negocio entrelazados (`in3.actuation`, persistencia, `alarmTimerStart()`, `turnFans()`, y **llama a `PIDHandler()` en su propio bucle** — la causa del hazard de concurrencia que reportó el security-reviewer) | Extraer efectos (fase 2), borrar UI (fase 4) |
| `UI_calibration.cpp` (179) + `system/calibrateSensors.cpp` | Flujo de calibración 100% on-board (encoder + pantalla) | Proceso de calibración ya no existe — UI muerta, borrable (fase 4) |
| `UI_mainMenu.cpp` (211) | Navegación de menú on-board | UI muerta — borrable (fase 4) |
| `UI_settings.cpp` (194) | Renderizado de ajustes, sin efectos propios | UI muerta — borrable (fase 4) |
| `userInterface.cpp` (680) | Handler de encoder: controlMode, setpoints, fototerapia, idioma, número de serie, fine-tune de calibración | **Confirmado redundante en su totalidad** — `CommTask.cpp` ya gestiona controlMode/setpoints/fototerapia/idioma vía `HMI,...`, y el fine-tune de calibración cae junto con el resto del proceso — borrable (fase 4) |

**Fuera de `legacy/` pero acoplado**:
- `system/updateData.cpp`: `updateDisplaySensors()` (dibujo on-board) comparte fichero con `logI/logE/logAlarm/logCharger/...`, usados por todo el sistema activo — no se puede borrar el fichero, solo extirpar el dibujo.
- `system/initHardware.cpp`: llama `loadlogo()` en boot — trivial de quitar.
- `main.cpp`: `UI_Task` (dibuja y llama `PIDHandler()`), `Backlight_Task`, y `in3.restoreState` que arranca directamente en `UI_actuatorsProgress()` tras un reinicio.

## Decisiones ya tomadas

1. **Calibración**: el proceso de calibración ya no existe en el producto — no hace falta construir ningún equivalente en Display_HMI. `UI_calibration.cpp`, `system/calibrateSensors.cpp`, y el fine-tune de calibración dentro de `userInterface.cpp` se borran directamente en la fase 4, igual que el resto de la UI muerta. (Al ejecutar la fase 4: comprobar si algo fuera de `legacy/` sigue leyendo valores de calibración ya persistidos en EEPROM/Preferences — eso es aplicar una corrección ya guardada, no el proceso de calibración en sí, y puede seguir vivo aunque se borre la UI que los generaba.)
2. **Fallback sin HMI**: se acepta perder la UI local si Display_HMI se desconecta/falla. El control de actuadores y alarmas del motherBoard no depende de la UI, así que la seguridad no se ve afectada — solo se pierde visibilidad/control local sin el HMI conectado. No añade alcance a la fase 4.

## Fases de ejecución

| Fase | Qué | Riesgo | Bloqueada por |
|---|---|---|---|
| 0 ✅ | Borrar `loadlogo()` (+ llamada en `initHardware.cpp`) y `Backlight_Task`/`backlightHandler()` | Ninguno | — |
| 1 | Migrar de `legacy/sensors.cpp` la lógica viva a `modules/sensors/` o `system/`; separar el logging de `updateData.cpp` en fichero propio | Bajo (mover código, no cambiar lógica) | — |
| 2 | Sacar de `UI_actuatorsProgress()` los efectos no visuales (`in3.actuation`, persistencia `KEY_ACTUATION`, `alarmTimerStart()`, `turnFans()`, llamada a `PIDHandler()`) hacia `Communication_Receiver`/`main.cpp` | Resuelve por construcción el hazard de concurrencia de `PIDHandler()` con dos llamadores | Fase 1 (para que `PIDHandler()` no dependa de código que ya se movió) |
| 3 | Borrado real: `drawGraphicInterphace.cpp`, `UI_mainMenu.cpp`, `UI_settings.cpp`, `userInterface.cpp`, `UI_actuatorsProgress.cpp`, `UI_calibration.cpp`, `system/calibrateSensors.cpp`, `UI_Task`, prototipos en `main.h`. Antes de borrar, comprobar si algo fuera de `legacy/` sigue leyendo offsets de calibración ya persistidos en EEPROM/Preferences (aplicar un valor guardado es independiente de la UI que lo generó) | Libera ~3400 líneas y una tarea FreeRTOS (`UI_Task`, 4096 bytes de stack) | Fases 1 y 2 |

## Siguiente paso

Sin bloqueos pendientes. Ejecutar Fase 0 y 1 primero (sin riesgo), luego Fase 2, y cerrar con la Fase 3 (borrado real).
