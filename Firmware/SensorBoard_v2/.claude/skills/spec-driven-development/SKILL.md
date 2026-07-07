---
name: spec-driven-development
description: Cómo hacer desarrollo guiado por specs (SDD) con OpenSpec en Genesis y ligarlo a TDD. Usar al crear una propuesta de cambio, escribir o leer specs, entender las skills openspec-*, o conectar los escenarios de la spec con los tests.
---

# Spec-Driven Development con OpenSpec

OpenSpec mantiene una **spec viva** (`openspec/specs/`) como fuente de verdad del comportamiento actual, y gestiona cada modificación como un **change** con deltas auditables.

## Estructura

```
openspec/
├─ specs/<dominio>/spec.md        # comportamiento ACTUAL (fuente de verdad — protegido)
└─ changes/
   ├─ <change-name>/
   │  ├─ proposal.md              # por qué, alcance
   │  ├─ design.md                # enfoque técnico
   │  ├─ tasks.md                 # checklist con checkboxes
   │  └─ specs/<dominio>/spec.md  # DELTA: ## ADDED / ## MODIFIED / ## REMOVED Requirements
   └─ archive/<date>-<change>/    # changes completados (deltas ya fusionados)
```

## Skills de OpenSpec

OpenSpec está en **delivery mode `skills`**: cada paso es una skill `openspec-*` **auto-invocable** (Claude la activa según el contexto; también `/openspec-...`). No hay comandos `/opsx:*`.

| Skill                     | Stage | Función                                                  |
| ------------------------- | ----- | -------------------------------------------------------- |
| `openspec-explore`        | 1     | Socio de pensamiento: lee el código, sopesa opciones     |
| `openspec-propose`        | 2     | Crea el change (proposal + design + tasks + delta specs) |
| `openspec-apply-change`   | 5     | Implementa según las tasks                               |
| `openspec-archive-change` | 10    | Fusiona deltas en `specs/` y archiva el change           |
| `openspec-sync-specs`     | —     | Sincroniza deltas a `specs/` sin archivar (opcional)     |

> **Verify (stage 7) no es un paso de OpenSpec**: lo hace `qa-engineer` ejecutando `idf.py build` y comprobando que cada escenario de la spec tiene un `TEST_CASE` Unity (ver skill `tdd-cycle`).

## Sync de specs: CLI real, no un script propio

El CLI `openspec` **sí existe y está instalado** (`@fission-ai/openspec`, v1.5.0 — `openspec doctor`
→ `ok`). No hay ni ha habido nunca un script propio (`scripts/openspec-sync.mjs`): promover los
deltas a `openspec/specs/` se hace con los comandos reales del CLI:

```bash
openspec new change <slug>                  # crea el change (proposal/design/tasks/specs scaffold)
openspec instructions <artifact> --change <slug> --json   # instrucciones/plantilla por artefacto
openspec status --change <slug> --json      # progreso, artefactos pendientes, contextFiles
openspec list --json                        # changes activos
openspec archive <change-name>              # fusiona los deltas de specs/ en openspec/specs/ y archiva el change
openspec validate --all                     # valida que los artefactos son estructuralmente correctos
```

`openspec archive` es el que sustituye a cualquier "script de sync": mueve el change completado a
`changes/archive/<fecha>-<nombre>/` y aplica los deltas (`## ADDED/MODIFIED/REMOVED/RENAMED
Requirements`) sobre `openspec/specs/<capability>/spec.md`. Para sincronizar deltas **sin** archivar
todavía (change aún activo), la skill `openspec-sync-specs` hace el merge de forma agent-driven
(lee delta + spec principal y aplica el cambio con criterio, no una sustitución mecánica). El hook
`protect-files` sigue bloqueando ediciones **manuales** a `openspec/specs/**`: solo `openspec
archive`/`openspec-sync-specs` tocan ese árbol.

## Escenarios = contrato = tests

Cada requisito de la spec lleva **escenarios** en formato Given/When/Then. Son el puente con TDD:

1. `propose`: el `product-manager` escribe escenarios testeables.
2. `red`: el `test-writer` convierte cada escenario en un test que falla (skill `tdd-cycle`).
3. `green`/`refactor`: el `senior-developer` implementa hasta verde.
4. `verify`: el `qa-engineer` confirma que cada escenario tiene test y pasa.
5. `archive`: los deltas se fusionan; la spec viva queda como nuevo estado de verdad.

## Reglas

- **No edites `openspec/specs/` a mano** (lo bloquea un hook): cambia vía deltas en el change y `openspec-archive-change`.
- Un escenario sin test es un contrato sin verificar: no se archiva.
- Mantén las specs como única fuente de verdad; los tests son su materialización, no documentación duplicada.
