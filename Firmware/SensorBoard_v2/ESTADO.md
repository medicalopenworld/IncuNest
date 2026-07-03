# Estado del proyecto

> Testigo de momentum: qué se está haciendo ahora y cuál es el siguiente paso. Una ventana nueva de Claude Code empieza leyendo este archivo. Lo actualiza `retro-improver` al cerrar cada tarea (paso 7).

## Épica activa

**EPIC-001 · Roadmap SensorBoard — Fases 1-5** (`docs/epics/sensorboard-roadmap-fases.md`)

Implementación del firmware completo desde cero, en orden 1 → 2 → 4 → 3 → 5, con ramas encadenadas desde `claude_agents_tests` y merges a `dev` pendientes de aprobación humana al final. Hardware confirmado y documentado en `docs/hardware.md`.

**Próximo paso:** Fase 3 — micrófono PDM en `feat/sb-phase3-mic` (rama desde `feat/sb-phase4-door`): ICS-41350 por I2S en modo PDM RX (IO40 clk, IO39 data), `audio_task` con RMS→dBA (calibración documentada como pendiente), evento `sound_level`, `sensors.mic` en status. Validar no-contención con el transporte (prio 5).

**Fases cerradas:**
- Fase 1 (USB CDC) — `feat/sb-phase1-usb-cdc`, spec `2026-07-03-sb-phase1-usb-cdc`, retro `docs/retro/2026-07-03-fase1-usb-cdc.md`.
- Fase 2 (SHT40 ×3 + ALS) — `feat/sb-phase2-env-sensors`, spec `2026-07-03-sb-phase2-env-sensors`, retro `docs/retro/2026-07-03-fase2-env-sensors.md`.
- Fase 4 (puerta) — `feat/sb-phase4-door`, spec `2026-07-03-sb-phase4-door`, retro `docs/retro/2026-07-03-fase4-door.md`.

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
