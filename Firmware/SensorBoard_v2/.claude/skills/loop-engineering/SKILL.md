---
name: loop-engineering
description: Cómo orquestar el ciclo de desarrollo de Genesis (loop engineering). Usar al implementar una feature de principio a fin, al ejecutar /loop-run, o ante dudas sobre qué rol/stage toca ahora, el orden de los stages o cómo encadenan OpenSpec, TDD, review y gitflow.
---

# Loop engineering

El ciclo orquestado de Genesis. Cada **stage** produce un **commit atómico** (Conventional Commits) en `feat/<slug>` (producto) o `meta/<slug>` (capa agéntica); al final se integra con `merge --no-ff` a `dev`.

> **Modalidad** (ortogonal): `auto` / `human` / `oneshot` regulan _quién controla_ el ritmo y dónde hay gates humanos — ver skill `loop-modes`. Lo de abajo regula _cuánto proceso_ según el tamaño del cambio.

## Velocidades (escala el proceso al tamaño del cambio)

No todo cambio paga el loop completo de 12 stages. **Las specs no siempre son necesarias.** Elige el nivel por el radio del cambio (criterio: reversibilidad + superficie):

| Nivel       | Criterio                                             | Proceso                                  | Spec            | Rama / modo      |
| ----------- | ---------------------------------------------------- | ---------------------------------------- | --------------- | ---------------- |
| **Trivial** | 1 fichero, <30 líneas, sin cambio de interfaz        | fix/chore directo + gate `idf.py build`  | No              | `oneshot`        |
| **Menor**   | 1–3 ficheros, sin API pública, riesgo bajo           | loop abreviado: explore→red→green→verify | No              | `feat/*` corto   |
| **Medio**   | varios paquetes, cambio de comportamiento/interfaz   | loop completo (12 stages)                | Sí (escenarios) | `feat/*`         |
| **Grande**  | nueva capa, breaking change, coordinación entre apps | loop completo + ADR + OpenSpec formal    | Sí (formal)     | `feat/*` + épica |

Señal por tipo de commit (Conventional ↔ SemVer): `chore`/`fix` → sin spec; `feat` → escenarios mínimos; `feat` con `BREAKING CHANGE` → spec/ADR.

## Stages

| #   | Stage              | Rol(es) / skill                                                        | OpenSpec                  | Commit                                 |
| --- | ------------------ | ---------------------------------------------------------------------- | ------------------------- | -------------------------------------- |
| 1   | Explore            | `architect` + `researcher` (delega a subagente si >15 ficheros a leer) | `openspec-explore`        | `chore(spec): explore <feat>`          |
| 2   | Spec/Propose       | `product-manager`                                                      | `openspec-propose`        | `docs(spec): propose <feat>`           |
| 3   | Design             | `architect` + `scribe` · `arch-embedded-layering`                      | —                         | `docs(adr): <decisión>`                |
| 4   | Red                | `qa-engineer` + `test-writer` · `tdd-cycle`                            | —                         | `test: failing specs for <feat>`       |
| 5   | Green              | `senior-developer` · `tdd-cycle`                                       | `openspec-apply-change`   | `feat: implement <feat>`               |
| 6   | Refactor           | `senior-developer`                                                     | —                         | `refactor: <feat>`                     |
| 7   | Verify             | `qa-engineer` (subagente fresco: ve solo el diff y los criterios)      | — (ver nota)              | `test: verify <feat>`                  |
| 8   | Review             | `code-reviewer` + `security-reviewer` (subagentes frescos en paralelo) | —                         | `fix: review feedback`                 |
| 9   | Docs               | `doc-keeper` + `scribe`                                                | —                         | `docs: update for <feat>`              |
| 10  | Archive            | `product-manager`                                                      | `openspec-archive-change` | `chore(spec): archive <feat>`          |
| 11  | Retro + Automejora | `retro-improver` · `meta-self-improvement`                             | —                         | `chore(meta): retro + improve .claude` |
| 12  | Finish             | `git-flow`                                                             | —                         | `merge --no-ff feat/<slug>` → `dev`    |

> **Verify (stage 7)** no es un paso de OpenSpec: la verificación la hace `qa-engineer` ejecutando `idf.py build` (gate de compilación, automatizable) + comprobando que cada escenario de la spec tiene un `TEST_CASE` Unity. La ejecución en hardware real (`idf.py -p COMx flash monitor`) queda como paso manual documentado, nunca automatizada. OpenSpec se entrega **solo como skills** auto-invocables `openspec-*` (delivery `skills`); Claude las activa según el contexto.

## Reglas del loop

- **Spec según velocidad**: en cambios **medio/grande**, los escenarios de la spec (stage 2) son la fuente de los tests (stage 4) — no se implementa sin spec. En **trivial/menor** la spec se omite (ver Velocidades), pero **el test en rojo sigue siendo obligatorio** salvo en cambios sin lógica (docs, config, bumps).
- **TDD estricto**: red (4) → green (5) → refactor (6). El stage Green no empieza sin tests en rojo.
- **Verify con evidencia**: no se avanza a review sin `idf.py build` en verde y cada escenario de la spec cubierto por un `TEST_CASE`. La ejecución en placa real es evidencia adicional cuando aplica, pero no bloquea el cierre del stage si no hay hardware conectado en ese momento.
- **Review en paralelo**: `code-reviewer` y `security-reviewer` se lanzan a la vez; sus hallazgos se resuelven antes de archivar.
- **Automejora siempre**: el stage 11 deja al framework mejor que al empezar (ver skill `meta-self-improvement`).
- **Un commit por stage**: atómico y con tipo Conventional correcto (ver skill `git-flow`).

## Cómo se invoca

`/loop-run <descripción de la feature>` arranca el ciclo en una rama `feat/<slug>`. Para stages sueltos: `/git-feature-start`, `/git-feature-finish`, `/git-release`, `/meta-retro`.

Detalle de cada pieza: skills `spec-driven-development`, `tdd-cycle`, `arch-embedded-layering`, `git-flow`, `meta-self-improvement`. Visión completa: `docs/blueprint/README.md`.
