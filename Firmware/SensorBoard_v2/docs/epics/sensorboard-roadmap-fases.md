# EPIC-001 · Roadmap SensorBoard — Fases 1-5

**Spec de conjunto:** `Firmware/docs/superpowers/specs/2026-07-03-sensorboard-roadmap.md`
**Hardware (fuente de verdad):** `docs/hardware.md`

Implementación del firmware completo del SensorBoard desde cero (la Fase 1 descrita como "completada" en el roadmap nunca llegó a implementarse — confirmado por Pablo el 2026-07-03). Orden acordado: **1 → 2 → 4 → 3 → 5** (de menor a mayor complejidad). Integración por **ramas encadenadas** (cada fase parte de la rama de la anterior); los merges a `dev` los aprueba Pablo al final, en orden. Verificación automatizable: `idf.py build`; nunca se flashea sin supervisión.

## Sub-tareas

- [x] Fase 1 — Transporte USB CDC (`usb_comm`): proyecto ESP-IDF + framing + CRC16 + tareas RX/TX + comando `status` (`feat/sb-phase1-usb-cdc`) — retro: `docs/retro/2026-07-03-fase1-usb-cdc.md`
- [x] Fase 2 — Sensores ambientales: SHT40 ×3 en dos buses I2C + ALS analógico en ADC, `sensor_task` + evento `sensor_data` (`feat/sb-phase2-env-sensors`) — retro: `docs/retro/2026-07-03-fase2-env-sensors.md`
- [x] Fase 4 — Sensor de puerta: hall DRV5032 en IO47, ISR→cola, eventos `door_open`/`door_closed` (`feat/sb-phase4-door`) — retro: `docs/retro/2026-07-03-fase4-door.md`
- [x] Fase 3 — Micrófono PDM ICS-41350: I2S modo PDM RX, RMS→dBA, evento `sound_level` (`feat/sb-phase3-mic`) — retro: `docs/retro/2026-07-03-fase3-mic.md`
- [x] Fase 5 — Cámara OV2640: captura JPEG bajo demanda, `send_binary` + `TYPE=0x01` (`feat/sb-phase5-camera`) — retro: `docs/retro/2026-07-03-fase5-camera.md`

**EPIC-001 completada (2026-07-03).** Las 5 fases implementadas con loop completo (spec→TDD→review→docs→retro) en ramas encadenadas. Pendiente: aprobación humana de los merges a `dev` (en orden: fase1→2→4→3→5) y verificación on-target de Fases 3/5 (las 1/2/4 ya pasaron Unity en hardware real).
