# Estado del proyecto

> Testigo de momentum: qué se está haciendo ahora y cuál es el siguiente paso. Una ventana nueva de Claude Code empieza leyendo este archivo. Lo actualiza `retro-improver` al cerrar cada tarea (paso 7).

## Épica activa

Ninguna. **EPIC-001 (Roadmap SensorBoard, Fases 1-5) se completó el 2026-07-03** — ver `docs/epics/sensorboard-roadmap-fases.md`.

**Próximo paso (requiere a Pablo):**

1. **Aprobar e integrar los merges a `dev`** (gate `guard-merge`, siempre humano), en orden de encadenado:
   `feat/sb-phase1-usb-cdc` → `feat/sb-phase2-env-sensors` → `feat/sb-phase4-door` → `feat/sb-phase3-mic` → `feat/sb-phase5-camera`
   (al estar encadenadas, basta merge --no-ff de `feat/sb-phase5-camera` a `dev` para integrarlo todo; los 5 merges separados preservan mejor la trazabilidad por fase — a elección).
2. **Verificación on-target pendiente:** Fases 3 y 5 (Unity + integración con hardware real); las Fases 1/2/4 ya pasaron sus tests Unity en placa (2026-07-03). Checklists manuales en los `tasks.md` archivados de cada change.
3. Extender el **parser de la motherboard** para los nuevos `cmd`/`event` (fuera de este repo): `sensor_data`, `door_open/closed` + re-aserción, `sound_level`, `capture`/`TYPE=0x01`, y los contratos de fail-safe del README (heartbeat >90 s, hall averiado, flapping).

## Épicas cerradas

- **EPIC-001 · Roadmap SensorBoard Fases 1-5** — 5 changes OpenSpec archivados (`2026-07-03-sb-phase{1,2,4,3,5}-*`), 5 retros en `docs/retro/`, ~40 commits en ramas encadenadas.
- **EPIC-000 · Adaptación del framework Genesis a ESP-IDF** — archivada como `2026-07-02-adapt-genesis-esp-idf`.

## Últimas decisiones relevantes

- TX con dos colas: JSON siempre prioritario; máx. 1 binario en vuelo (Fase 5).
- Enlace USB = canal de confianza intra-dispositivo (riesgo documentado en README).
- Gates de plausibilidad en todos los sensores (temp, dB, señal viva PDM) — CRC/lectura OK ≠ dato válido.
- La verificación automática (Stop hook) es solo `idf.py build`; flash/Unity on-target siempre manual.

## Seguimientos diferidos (candidatos a próximas tareas)

Calibraciones (ALS, dBA + ponderación A), recovery de bus I2C colgado, pool estático cJSON, contratos de fail-safe en la motherboard. Detalle en las retros de `docs/retro/`.
