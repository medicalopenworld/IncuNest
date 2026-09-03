# SensorBoard (IncuNest) — framework agéntico

Firmware del **SensorBoard**, la placa de sensores periférica de la incubadora IncuNest (dispositivo médico). ESP32-S3-WROOM-1-N16R8, ESP-IDF v6 nativo (no PlatformIO), C, FreeRTOS. Se comunica con la motherboard por USB-CDC con un framing binario propio (`Magic(2B)+Type(1B)+Length(4B LE)+Payload+CRC16-CCITT`) que transporta comandos/eventos JSON.

Este proyecto usa una versión **retargeteada** del framework agéntico "Genesis" (originalmente construido para un monorepo TypeScript/Astro/Next.js) — clean code, SOLID donde aplica en C, SDD (OpenSpec) y TDD a través de un **loop engineering** ejecutado por agentes especializados. Es un experimento deliberado en la rama `claude_agents_tests`, aislado a esta carpeta: no afecta al resto de placas del repo (`motherBoard/`, `Display_HMI/`), que siguen su propio flujo más ligero.

> Arquitectura técnica: `docs/architecture.md`. Orientación general del proceso: `docs/blueprint/README.md`. Contexto histórico (Fase 1 y roadmap completo de fases): `Firmware/docs/superpowers/plans/2026-06-08-sensorboard-phase1.md` y `Firmware/docs/superpowers/specs/2026-07-03-sensorboard-roadmap.md`.

**Estado actual del trabajo (momentum):** @ESTADO.md

## Stack

- **Framework**: ESP-IDF v6 nativo — CMake + `idf.py`. NO PlatformIO, NO Arduino framework.
- **Lenguaje**: C (no C++ para código nuevo). FreeRTOS para concurrencia.
- **Componentes**: `usb_comm` (transporte USB-CDC, framing+CRC, agnóstico al payload — no lo reabras salvo que trabajes justo en él) + un componente ESP-IDF por fase/sensor (Fase 2: SHT40+ALS, Fase 3: micrófono I2S, Fase 4: sensor de puerta, Fase 5: cámara).
- **Tests**: Unity (integrado en ESP-IDF). Sin framework de test nativo/host — las pruebas viven en `test_apps/<componente>_test/` y se ejecutan flasheando hardware real.
- **Sin Node/pnpm en el firmware**: no hay `package.json`. El único uso de Node en este proyecto es el CLI global `openspec` (`@fission-ai/openspec`).

## Comandos

```bash
idf.py build                       # compila (gate automatizable, lo corre el hook de Stop)
idf.py -p COMx flash monitor       # flashea y monitorea en hardware real (SIEMPRE manual, nunca en un hook)
idf.py -C test_apps/<comp>_test build flash monitor -p COMx   # build+flash+run de los tests Unity de un componente
openspec new change <slug>         # nueva propuesta de cambio
openspec status --change <slug>    # progreso de artefactos de un change
openspec archive <slug>            # archiva un change completado, mergea deltas a specs/
```

## Flujo de trabajo: loop engineering

Desarrolla features siguiendo el loop de 12 stages, cada uno con un **commit atómico** (Conventional Commits) y el rol adecuado:

`explore → propose → design → red → green → refactor → verify → review → docs → archive → retro → finish`

Detalle en el skill `loop-engineering`. Skills de acción: `/loop-run`, `/git-feature-start`, `/git-feature-finish`, `/git-release`, `/meta-retro`. Specs con OpenSpec real (CLI `@fission-ai/openspec` instalado — skills `openspec-*`, ver `spec-driven-development`).

## Reglas siempre activas

- **TDD estricto**: test Unity en rojo antes de implementar. No hay lógica de protocolo/sensor sin test. Verificación automatizable = `idf.py build` (compila); el flasheo/monitor en hardware real queda siempre manual (nunca se automatiza en un hook — no hay garantía de que haya un dispositivo conectado).
- **Límites de componente**: `usb_comm` es agnóstico al payload — ninguna fase después de la 1 debería tocar su framing/CRC/tareas RX-TX, solo llamar a `sensorBoard_comm_send_json()`/`send_binary()`. Cada fase vive en su propio componente ESP-IDF con header público limpio. Detalle en el skill `arch-embedded-layering` y en las reglas `embedded-*` de `Firmware/.claude/rules/`.
- **Spec antes que código**: los escenarios de OpenSpec (`#### Scenario:` WHEN/THEN) son la fuente de los `TEST_CASE` de Unity.
- **Gitflow**: `feat/*` (producto) y `meta/*` (capa agéntica) → `dev` (merge --no-ff); `release/*` → `main` (tag semver, versión leída de `SB_PROTO_FW_VERSION`). Nunca push directo a `main`/`dev` (hook `guard-push`). **El gate de merge/tag (`guard-merge`) exige aprobación humana explícita en las tres modalidades del loop, incluida `auto`** — dispositivo médico, la integración irreversible siempre se confirma. Estándar de nombres en el skill `git-flow`.
- **Modalidad y velocidad**: el trabajo corre en una modalidad — `auto` (desatendido, salvo el gate de merge/release que siempre para), `human` (gates también en plan/épica) u `oneshot` (tarea puntual) — fijada con `/loop-mode` (skill `loop-modes`; SessionStart la pregunta). El proceso escala al tamaño del cambio (skill `loop-engineering`); las specs no siempre son necesarias para cambios triviales.
- **Learnings obligatorios**: cerrar una sub-tarea de épica exige registrar el paso 7 en `docs/retro/` (lo blinda el hook `require-retro`).
- **Automejora**: cada loop termina revisando si conviene mejorar `.claude`/`CLAUDE.md` (agente `retro-improver`).

## Convenciones por zona

**La capa agéntica vive un nivel arriba, en `Firmware/.claude/`** (compartida por las tres placas desde la generalización del framework), no en `SensorBoard_v2/.claude/`. En esta carpeta solo queda lo personal: `.claude/settings.local.json` (gitignored), `.loop-mode` y `logs/`. Ojo: el `.gitignore` de la raíz ignora `.claude` completo, así que `Firmware/.claude/` **no está versionado** y solo existe en el checkout principal — un worktree nuevo no lo trae.

Las reglas detalladas se cargan automáticamente según la zona del repo que toques (`Firmware/.claude/rules/`): `embedded-motherboard`, `embedded-display-hmi`, `embedded-shared` (límites de módulo, ISR, allocation — no hay aún una regla `embedded-*` específica del SensorBoard), `testing` (Unity/idf.py), `commits` (autoría, Conventional Commits, ramas), `security` (framing/CRC, fail-safe de sensores/actuadores), `tooling` (escritura de ficheros, worktrees, PlatformIO).

## Enforcement (hooks)

`Firmware/.claude/settings.json` activa: bloqueo de edición de `.env*`/`dependencies.lock`/`managed_components/**`/specs archivadas (PreToolUse `protect-files`), bloqueo de push a `main`/`dev` (`guard-push`) y de merge/tag sin aprobación **en cualquier modalidad** (`guard-merge`); `clang-format` automático al editar `.c`/`.h` (PostToolUse `format-file`); al cerrar turno (Stop): `idf.py build` como gate de compilación (`run-affected-tests` — nunca flashea ni monitorea) y **gate de learnings** que exige el paso 7 antes de cerrar una sub-tarea de épica (`require-retro`); contexto de sesión + pregunta de modalidad (SessionStart `session-context`); registro de subagentes (SubagentStop `subagent-log`).

### Modo desatendido (retirado)

El antiguo Stop hook `unattended-loop.sh` (re-despertaba al agente mientras quedaran checkboxes `[ ]` en `docs/epics`) **ya no existe en disco ni en ninguna rama**: se perdió al mover el framework a `Firmware/.claude/`. Si en el futuro hace falta, se recupera con `git show 234cac8:Firmware/SensorBoard_v2/.claude/hooks/unattended-loop.sh`, se coloca en `Firmware/.claude/hooks/` y se registra **solo en `settings.local.json`**. No lo registres sin el script presente: cada Stop fallará con "No such file or directory".

## Roles (subagentes)

`architect` (componentes ESP-IDF, tareas/colas FreeRTOS), `senior-developer` (TDD con Unity/idf.py), `qa-engineer` (matriz de test: unit Unity + integración on-device, sin E2E), `test-writer` (tests Unity en rojo), `product-manager` (propuestas OpenSpec con el CLI real), `researcher` (docs.espressif.com/freertos.org), `security-reviewer` (framing/CRC/fail-safe de dispositivo médico), `code-reviewer` (FreeRTOS, allocation, ISR safety), `scribe` (ADRs en `docs/adr/`), `doc-keeper` (README/CHANGELOG/docs), `retro-improver` (learnings → `.claude`/CLAUDE.md). Ver `/agents`.

## Mapa de conocimiento (dónde consultar qué)

| Necesitas saber…               | Mira en…                                                                                       |
| ------------------------------ | ---------------------------------------------------------------------------------------------- |
| **Cómo actúan los agentes**    | `Firmware/.claude/` (un nivel arriba, no versionado) — `rules/`, `skills/`, `agents/`, `hooks/` |
| **Por qué** (decisiones)       | [`docs/adr/`](docs/adr/README.md) — índice de ADRs vigentes                                    |
| **Qué hemos aprendido**        | [`docs/retro/`](docs/retro/README.md) — destilado de aprendizajes aplicados                    |
| **Cómo es el sistema**         | [`docs/architecture.md`](docs/architecture.md) + [`docs/blueprint/`](docs/blueprint/README.md) |
| **Historia del protocolo/Fase 1** | `Firmware/docs/superpowers/plans/2026-06-08-sensorboard-phase1.md` (un nivel arriba de este proyecto) |
| **Roadmap de fases 2-5**       | `Firmware/docs/superpowers/specs/2026-07-03-sensorboard-roadmap.md` |
| **Dónde estamos ahora**        | [`@ESTADO.md`](ESTADO.md) (testigo) + [`docs/epics/`](docs/epics/README.md) (backlog)          |
| **Qué cambió y versión**       | [`CHANGELOG.md`](CHANGELOG.md) (Keep a Changelog + SemVer, versión desde `SB_PROTO_FW_VERSION`) |

Al **cerrar una tarea**, el conocimiento nuevo se enruta a su capa (paso 7, skill `meta-self-improvement`): comportamiento→`.claude`, decisión→ADR, aprendizaje→retro, hecho de toda sesión→este archivo.

## Memoria / momentum (continuidad entre sesiones)

- **`@ESTADO.md`** (raíz, cargado arriba) es el testigo: dice la épica/tarea activa y el próximo paso. Una ventana nueva empieza leyéndolo.
- **`docs/epics/`** es el backlog estructurado (índice + un archivo por épica con checkboxes).
- Al cerrar cada tarea, **actualiza `ESTADO.md` y los checkboxes de la épica**. El `retro-improver` es el responsable.

## No hacer

- **Atribución: el único autor es Pablo Sánchez Bergasa** (GitHub: `pablo18393`). NUNCA añadas `Co-Authored-By: Claude` (ni Anthropic) ni el trailer `Claude-Session` a commits, merges, releases o PRs. Configurado en `Firmware/.claude/settings.json` (`includeCoAuthoredBy: false`, `attribution` vacía).
- No edites `openspec/specs/**` a mano (fuente de verdad; usa deltas + `openspec archive`).
- No reabras el framing/CRC/tareas RX-TX de `usb_comm` desde un componente de fase — es la señal de que el diseño se está saliendo de su capa (skill `arch-embedded-layering`).
- No automatices `idf.py flash`/`idf.py monitor` en ningún hook — requieren hardware real conectado y no son seguros de disparar sin supervisión.
- No hagas commits que mezclen varios stages del loop.
- Este es un proyecto de un solo desarrollador: no asumas convenciones de revisión multi-ingeniero (PRs con múltiples reviewers, etc.) salvo que se indique lo contrario.
