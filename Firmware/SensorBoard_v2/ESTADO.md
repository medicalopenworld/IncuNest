# Estado del proyecto

> Testigo de momentum: qué se está haciendo ahora y cuál es el siguiente paso. Una ventana nueva de Claude Code empieza leyendo este archivo. Lo actualiza `retro-improver` al cerrar cada tarea (paso 7).

## Épica activa

**EPIC-001 · Roadmap SensorBoard — Fases 1-5** (`docs/epics/sensorboard-roadmap-fases.md`)

Implementación del firmware completo desde cero, en orden 1 → 2 → 4 → 3 → 5, con ramas encadenadas desde `claude_agents_tests` y merges a `dev` pendientes de aprobación humana al final. Hardware confirmado y documentado en `docs/hardware.md`.

**Próximo paso:** Fase 4 — sensor de puerta en `feat/sb-phase4-door` (rama desde `feat/sb-phase2-env-sensors`): hall DRV5032 en IO47, ISR de flanco con hand-off por cola, debounce considerando los 5 Hz internos del sensor, eventos `door_open`/`door_closed`, `sensors.door` en status.

**Fases cerradas:**
- Fase 1 (USB CDC) — `feat/sb-phase1-usb-cdc`, spec `2026-07-03-sb-phase1-usb-cdc`, retro `docs/retro/2026-07-03-fase1-usb-cdc.md`.
- Fase 2 (SHT40 ×3 + ALS) — `feat/sb-phase2-env-sensors`, spec `2026-07-03-sb-phase2-env-sensors`, retro `docs/retro/2026-07-03-fase2-env-sensors.md`.

Verificación on-device (flash + Unity en placa) de ambas fases pendiente de sesión manual con hardware.

## Épicas cerradas

- **EPIC-000 · Adaptación del framework Genesis a ESP-IDF** — archivada como `openspec/changes/archive/2026-07-02-adapt-genesis-esp-idf/`, retro en `docs/retro/2026-07-03-epic-000-adapt-genesis-esp-idf.md`.

## Últimas decisiones relevantes

- La Fase 1 que el roadmap marcaba "completada" nunca se implementó — se parte de cero (confirmado por Pablo, 2026-07-03).
- Pinout y sensores confirmados por Pablo en `docs/hardware.md`: ALS es analógico (ADC en IO1), el micrófono es PDM (no I2S estándar), SHT40 ×3 repartidos en dos buses I2C.
- El gate de merge/tag (`guard-merge.sh`) es siempre-on, independiente de `.loop-mode` (firmware de dispositivo médico).
- La verificación automática (Stop hook) es solo `idf.py build` (compilación); nunca flashea ni ejecuta Unity en el dispositivo sin supervisión.

## Backlog

Ver `docs/epics/sensorboard-roadmap-fases.md` (EPIC-001, activa) y `docs/epics/README.md` (índice).
