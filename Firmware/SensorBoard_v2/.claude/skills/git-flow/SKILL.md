---
name: git-flow
description: Convenciones de git, ramas y commits del framework Genesis (gitflow + Conventional Commits + merge --no-ff + releases semver). Úsalo al crear ramas, hacer commits por stage del loop, integrar features o preparar releases.
---

# Gitflow en Genesis

## Estándar de nombres (convención única)

Fuente de verdad de cómo se nombra TODO en el repo. Respétalo siempre (lo refuerza el paso 7 de learnings).

| Artefacto                 | Patrón                                                | Ejemplo                                   |
| ------------------------- | ----------------------------------------------------- | ----------------------------------------- |
| Rama de **producto**      | `feat/<slug-kebab>`                                   | `feat/cart-checkout`                      |
| Rama de **capa agéntica** | `meta/<slug-kebab>`                                   | `meta/epic-10-flujo`                      |
| Rama de feature de épica  | `feat/epic-<n>-<slug>` o `meta/epic-<n>-<slug>`       | `meta/epic-10-flujo`                      |
| Rama de release           | `release/<x.y.z>`                                     | `release/0.1.0`                           |
| Rama de hotfix            | `hotfix/<slug-kebab>`                                 | `hotfix/csp-header`                       |
| Commit                    | `tipo(scope): descripción` (Conventional, imperativo) | `feat(meta): añadir paso 7`               |
| Tag de release            | `v<x.y.z>` (semver)                                   | `v0.1.0`                                  |
| Épica                     | `docs/epics/EPIC-<n>-<slug>.md`                       | `docs/epics/EPIC-8-loop-learnings.md`     |
| Sub-tarea de épica        | `T<n>.<m>` (en el archivo de la épica)                | `T8.4`                                    |
| ADR                       | `docs/adr/<NNNN>-<slug>.md` (4 dígitos)               | `docs/adr/0001-stage-learnings-paso-7.md` |
| Retro                     | `docs/retro/<YYYY-MM-DD>-<feature-slug>.md`           | `docs/retro/2026-06-27-epic-8.md`         |

**Slug**: kebab-case, ASCII, corto y descriptivo del _qué_ (no del número de tarea suelto). Para ramas de
épica usa `epic-<n>-<tema>`, no `epic-<n>-t<m>` (el tema es más legible que el ordinal).

**Scopes de commit habituales**: `meta` (framework/.claude/momentum), `deps`/`build` (dependencias/CMake),
`spec`, `adr`, `research`, o el componente ESP-IDF tocado (`usb_comm`, `sht40_als`, `i2s_mic`,
`door_sensor`, `camera`…). Tipos: ver tabla de stages abajo. No hay `commitlint` (no es un proyecto
Node): el formato lo exige la convención y lo verifica `code-reviewer` en el stage de review; los
`merge:` están exentos.

## Ramas

| Rama            | Propósito                                              | Recibe de                | Integra con                   |
| --------------- | ------------------------------------------------------ | ------------------------ | ----------------------------- |
| `main`          | Producción. Releases versionadas (tag semver `vX.Y.Z`) | `release/*`              | —                             |
| `dev`           | Integración continua                                   | `feat/*` (merge --no-ff) | `release/*`                   |
| `feat/<slug>`   | Una feature = un loop                                  | `dev`                    | `dev` (merge --no-ff)         |
| `release/<ver>` | Estabiliza épicas terminadas                           | `dev`                    | `main` (+ back-merge a `dev`) |
| `hotfix/<slug>` | Arreglo urgente                                        | `main`                   | `main` y `dev`                |

> Un hook bloquea el push directo a `main`/`dev`: se actualizan solo vía merge desde `feature-finish`/`release`.

## Commits atómicos por stage

Cada stage del loop produce **un commit** con el tipo Conventional correcto:

| Stage        | Tipo de commit                         |
| ------------ | -------------------------------------- |
| Explore      | `chore(spec): explore <feat>`          |
| Propose      | `docs(spec): propose <feat>`           |
| Design (ADR) | `docs(adr): <decisión>`                |
| Red          | `test: failing specs for <feat>`       |
| Green        | `feat: implement <feat>`               |
| Refactor     | `refactor: <feat>`                     |
| Verify       | `test: verify <feat>`                  |
| Review       | `fix: review feedback`                 |
| Docs         | `docs: update for <feat>`              |
| Archive      | `chore(spec): archive <feat>`          |
| Retro        | `chore(meta): retro + improve .claude` |

Atribución: el único autor es **Pablo Sánchez Bergasa** (GitHub: `pablo18393`); jamás `Co-Authored-By: Claude/Anthropic` ni `Claude-Session`.

Reglas: un commit hace una sola cosa coherente; el mensaje explica el porqué cuando no es obvio; no hay `commitlint` (sin `commit-msg` hook de Node) — el formato lo verifica `code-reviewer` por convención.

## Integración de una feature

```bash
git checkout dev && git pull
git checkout -b feat/<slug>
# ... loop con commits atómicos por stage ...
git checkout dev
git merge --no-ff feat/<slug> -m "merge: feat/<slug> -> dev"
git branch -d feat/<slug>
```

El `--no-ff` preserva el agrupamiento de la feature en el historial (legible de un vistazo).

## Release (épicas terminadas)

```bash
git checkout -b release/<ver> dev
# estabilizar (solo fixes), bump de versión, CHANGELOG
git checkout main && git merge --no-ff release/<ver>
git tag -a v<ver> -m "release v<ver>"
git checkout dev && git merge --no-ff release/<ver>   # back-merge
```

Versionado semver: MAJOR (breaking), MINOR (feature), PATCH (fix).
