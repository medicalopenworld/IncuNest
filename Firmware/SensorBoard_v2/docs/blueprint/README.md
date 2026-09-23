# Blueprint — cómo funciona este proyecto

Este directorio existe porque el framework Genesis original lo referencia (`docs/blueprint/README.md` como la "biblia" de proceso y arquitectura de un producto SaaS multi-equipo). Aquí no hace falta esa ceremonia: este es un firmware embebido de un solo desarrollador. Esta página es solo la orientación de una frase con la que aterriza una sesión nueva — el detalle real vive en `CLAUDE.md` y en los propios skills.

## Qué es este proyecto

Firmware ESP-IDF v6 (C, FreeRTOS, CMake/`idf.py`) para el SensorBoard de la incubadora IncuNest (ESP32-S3-WROOM-1-N16R8), desarrollado en solitario por Pablo Sánchez Bergasa con el framework de agentes Genesis como asistencia. Ver `docs/architecture.md` para la arquitectura técnica.

## Cómo se trabaja

- **Loop engineering** (12 stages, un commit atómico por stage): `explore → propose → design → red → green → refactor → verify → review → docs → archive → retro → finish`. Se dispara con `/loop-run "<feature>"`. Detalle completo en el skill `loop-engineering`.
- **OpenSpec** (`openspec/`) guarda las propuestas de cambio (proposal/design/tasks/spec-deltas) antes de tocar código — skills `openspec-*`.
- **Gitflow**: `feat/*` (producto) o `meta/*` (capa agéntica) desde `dev`, merge `--no-ff`; `release/*` a `main` con tag semver. Nunca push directo a `main`/`dev`. Detalle en el skill `git-flow`.
- **TDD estricto**: test en rojo (Unity, on-target) antes de implementar.

## Dónde mirar cada cosa

Ver la tabla "Mapa de conocimiento" en `CLAUDE.md` — no se duplica aquí. Para el proceso paso a paso, los skills (`loop-engineering`, `git-flow`, `spec-driven-development`, `arch-embedded-layering`) son la fuente de verdad, no este archivo.
