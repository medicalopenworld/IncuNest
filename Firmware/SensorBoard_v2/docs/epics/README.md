# Épicas (backlog)

Un archivo por épica bajo `docs/epics/`, con sub-tareas como checkboxes (`- [ ]` / `- [x]`). `require-retro.sh` compara el diff de una rama contra `dev` para detectar checkboxes recién marcados `[x]`; si una rama cierra trabajo de épica sin registrar nada en `docs/retro/`, bloquea el cierre del turno. `retro-improver` es responsable de marcar los checkboxes y mantener `ESTADO.md` al día tras cada tarea.

## Convención de nombres

`docs/epics/<slug-de-la-epica>.md`.

## Índice

- **[EPIC-001 · Roadmap SensorBoard — Fases 1-5](sensorboard-roadmap-fases.md)** — activa. Firmware completo desde cero en orden 1→2→4→3→5.
- EPIC-000 · Adaptación del framework Genesis a ESP-IDF — cerrada sin fichero de épica (anterior a esta convención); trazada en `openspec/changes/archive/2026-07-02-adapt-genesis-esp-idf/` y `docs/retro/2026-07-03-epic-000-adapt-genesis-esp-idf.md`.
