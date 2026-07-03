# Estado del proyecto

> Testigo de momentum: qué se está haciendo ahora y cuál es el siguiente paso. Una ventana nueva de Claude Code empieza leyendo este archivo. Lo actualiza `retro-improver` al cerrar cada tarea (paso 7).

## Épica activa

**EPIC-001 · Roadmap SensorBoard — Fases 1-5** (`docs/epics/sensorboard-roadmap-fases.md`)

Implementación del firmware completo desde cero, en orden 1 → 2 → 4 → 3 → 5, con ramas encadenadas desde `claude_agents_tests` y merges a `dev` pendientes de aprobación humana al final. Hardware confirmado y documentado en `docs/hardware.md`.

**Próximo paso:** Fase 2 — sensores ambientales en `feat/sb-phase2-env-sensors` (rama desde `feat/sb-phase1-usb-cdc`): SHT40 ×2 en bus I2C IO41/IO42 (0x44+0x46) + SHT40 en bus principal IO4/IO5 (0x44) + ALS-PT19 por ADC en IO1, `sensor_task` con polling, evento `sensor_data`, extensión de `status` con `sensors{}`.

**Fases cerradas:** Fase 1 (USB CDC) completada en `feat/sb-phase1-usb-cdc` — 8 commits, spec archivada como `2026-07-03-sb-phase1-usb-cdc`, retro en `docs/retro/2026-07-03-fase1-usb-cdc.md`. Verificación on-device (flash + Unity en placa) pendiente de sesión manual con hardware.

## Épicas cerradas

- **EPIC-000 · Adaptación del framework Genesis a ESP-IDF** — archivada como `openspec/changes/archive/2026-07-02-adapt-genesis-esp-idf/`, retro en `docs/retro/2026-07-03-epic-000-adapt-genesis-esp-idf.md`.

## Últimas decisiones relevantes

- La Fase 1 que el roadmap marcaba "completada" nunca se implementó — se parte de cero (confirmado por Pablo, 2026-07-03).
- Pinout y sensores confirmados por Pablo en `docs/hardware.md`: ALS es analógico (ADC en IO1), el micrófono es PDM (no I2S estándar), SHT40 ×3 repartidos en dos buses I2C.
- El gate de merge/tag (`guard-merge.sh`) es siempre-on, independiente de `.loop-mode` (firmware de dispositivo médico).
- La verificación automática (Stop hook) es solo `idf.py build` (compilación); nunca flashea ni ejecuta Unity en el dispositivo sin supervisión.

## Backlog

Ver `docs/epics/sensorboard-roadmap-fases.md` (EPIC-001, activa) y `docs/epics/README.md` (índice).
