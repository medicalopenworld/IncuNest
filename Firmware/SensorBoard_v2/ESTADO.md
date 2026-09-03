# Estado del proyecto

> Testigo de momentum: qué se está haciendo ahora y cuál es el siguiente paso. Una ventana nueva de Claude Code empieza leyendo este archivo. Lo actualiza `retro-improver` al cerrar cada tarea (paso 7).

## Épica activa

Ninguna. **EPIC-001 (Roadmap SensorBoard, Fases 1-5) se completó el 2026-07-03** — ver `docs/epics/sensorboard-roadmap-fases.md`.

## Trabajo puntual (fuera de épica)

- **`feat/sb-usb-pin-autoswap` — cerrado el 2026-09-03, pendiente de merge a `dev`** (gate `guard-merge`, humano). Tolerancia al conector USB invertido: autoswap de D+/D- en el PHY del S3 si no hay enumeración en 2 s (ADR-0003, change archivado `2026-09-03-sb-usb-pin-autoswap`, retro `docs/retro/2026-09-03-usb-pin-autoswap.md`). Worktree: `Firmware/.worktrees/sb-usb-autoswap`. Verificación on-target pendiente (checklist en el `design.md` del change).

**Próximo paso (requiere a Pablo):**

1. **Aprobar e integrar el merge `feat/sb-usb-pin-autoswap` → `dev`** (`merge --no-ff`). Las Fases 1-5 ya están integradas en `dev`.
2. **Verificación on-target pendiente:** autoswap USB (4 casos del checklist) y Fases 3 y 5 (Unity + integración con hardware real); las Fases 1/2/4 ya pasaron sus tests Unity en placa (2026-07-03). Checklists manuales en los `tasks.md`/`design.md` archivados de cada change.
3. **Flasher (`Firmware/flasher_tool`):** indicar "gira el cable USB" si no detecta el SensorBoard — el bootloader ROM no aplica el intercambio de pines.
4. Extender el **parser de la motherboard** para los nuevos `cmd`/`event` (fuera de este repo): `sensor_data`, `door_open/closed` + re-aserción, `sound_level`, `capture`/`TYPE=0x01`, y los contratos de fail-safe del README (heartbeat >90 s, hall averiado, flapping).

## Épicas cerradas

- **EPIC-001 · Roadmap SensorBoard Fases 1-5** — 5 changes OpenSpec archivados (`2026-07-03-sb-phase{1,2,4,3,5}-*`), 5 retros en `docs/retro/`, ~40 commits en ramas encadenadas.
- **EPIC-000 · Adaptación del framework Genesis a ESP-IDF** — archivada como `2026-07-02-adapt-genesis-esp-idf`.

## Últimas decisiones relevantes

- TX con dos colas: JSON siempre prioritario; máx. 1 binario en vuelo (Fase 5).
- Enlace USB = canal de confianza intra-dispositivo (riesgo documentado en README).
- Gates de plausibilidad en todos los sensores (temp, dB, señal viva PDM) — CRC/lectura OK ≠ dato válido.
- La verificación automática (Stop hook) es solo `idf.py build`; flash/Unity on-target siempre manual. En Windows, `idf.py` debe lanzarse desde PowerShell: desde Git Bash (MSYS) sale con código 0 sin compilar.
- Enlace sano = `tud_mounted()`, no `tud_connected()`; autoswap D+/D- en el PHY antes que cualquier fallback UART/I2C (ADR-0003).
- Los `TEST_CASE` Unity en ficheros sin `app_main` exigen `WHOLE_ARCHIVE` en el test app.

## Seguimientos diferidos (candidatos a próximas tareas)

Calibraciones (ALS, dBA + ponderación A), recovery de bus I2C colgado, pool estático cJSON, contratos de fail-safe en la motherboard. Detalle en las retros de `docs/retro/`.
