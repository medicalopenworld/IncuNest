# Estado del proyecto

> Testigo de momentum: qué se está haciendo ahora y cuál es el siguiente paso. Una ventana nueva de Claude Code empieza leyendo este archivo. Lo actualiza `retro-improver` al cerrar cada tarea (paso 7).

## Épica activa

**EPIC-001 · Roadmap SensorBoard — Fases 1-5** (`docs/epics/sensorboard-roadmap-fases.md`)

Implementación del firmware completo desde cero, en orden 1 → 2 → 4 → 3 → 5, con ramas encadenadas desde `claude_agents_tests` y merges a `dev` pendientes de aprobación humana al final. Hardware confirmado y documentado en `docs/hardware.md`.

**Próximo paso:** Fase 5 — cámara OV2640 en `feat/sb-phase5-camera` (rama desde `feat/sb-phase3-mic`): esp32-camera (DVP según `docs/hardware.md`, PWDN IO21, sin RESET), SCCB compartiendo el bus I2C principal (handle vía `sb_env_get_main_i2c_bus()` — riesgo: compatibilidad esp32-camera con el driver i2c_master nuevo, investigar antes de diseñar), activar `TYPE=0x01`, implementar `send_binary` (única fase que toca `usb_comm`, acotado a TX), comando `capture`, `sensors.cam`.

**Fases cerradas:**
- Fase 1 (USB CDC) — `feat/sb-phase1-usb-cdc`, spec `2026-07-03-sb-phase1-usb-cdc`, retro `docs/retro/2026-07-03-fase1-usb-cdc.md`.
- Fase 2 (SHT40 ×3 + ALS) — `feat/sb-phase2-env-sensors`, spec `2026-07-03-sb-phase2-env-sensors`, retro `docs/retro/2026-07-03-fase2-env-sensors.md`.
- Fase 4 (puerta) — `feat/sb-phase4-door`, spec `2026-07-03-sb-phase4-door`, retro `docs/retro/2026-07-03-fase4-door.md`.
- Fase 3 (micrófono PDM) — `feat/sb-phase3-mic`, spec `2026-07-03-sb-phase3-mic`, retro `docs/retro/2026-07-03-fase3-mic.md`. Tests Unity de Fases 1/2/4 ya ejecutados en hardware real por Pablo (todo PASS tras corregir un vector CRC del plan).

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
