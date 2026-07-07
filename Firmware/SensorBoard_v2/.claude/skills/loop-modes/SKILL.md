---
name: loop-modes
description: Las modalidades de ejecución del loop de Genesis (auto / human / oneshot) y sus puntos de aprobación humana. Usar al arrancar trabajo en una sesión, al decidir si parar a pedir aprobación, o ante dudas sobre cuánta autonomía aplicar. El modo activo vive en .claude/.loop-mode.
---

# Modalidades del loop

Genesis ejecuta el trabajo en una de **tres modalidades**. El modo activo se guarda en
`.claude/.loop-mode` (gitignored, personal). Lo fija el usuario con `/loop-mode <modo>`; si no está
fijado, el hook `SessionStart` te pide preguntarlo antes de la primera tarea. El **paso de learnings**
(blindado por hook) aplica en TODAS las modalidades.

**Gate de merge/release siempre activo, independiente del modo:** a diferencia del resto de gates
descritos abajo (que sí varían por modalidad), `guard-merge.sh` bloquea `git merge`/`git tag v*` en
**las tres modalidades** — firmware de dispositivo médico, la integración irreversible a `dev`/`main`
siempre exige la marca `.claude/.merge-approved` de un solo uso, incluso en `auto`. No lo desactives ni
lo condiciones a `.loop-mode` de nuevo.

## `auto` — full desatendido

Tú tomas el control de la ejecución tarea a tarea: el Stop hook (`unattended-loop.sh`) te re-despierta
hasta agotar el backlog (`docs/epics`) o pararse por la salvaguarda anti-bucle. Sin gate de plan/épica
(ejecutas sin esperar aprobación previa) ni de aprobación intermedia: exploras, ejecutas, verificas
(`idf.py build`, compile-only — el flasheo/monitor en hardware real sigue siendo manual). Al llegar al
merge, `guard-merge.sh` te para igual que en `human` (ver arriba) — muestra al usuario el estado (tareas
hechas, build verde, diff resumido), espera su aprobación explícita, crea la marca
(`printf ok > .claude/.merge-approved`) y reintenta. Tras el merge, sacas learnings y sigues. Combina
con `defaultMode: "bypassPermissions"` para el resto de permisos. El humano interrumpe con `Esc`.

## `human` — human-in-the-loop (supervisado)

Metes al humano en el loop en **tres gates** (para y espera su visto bueno; no continúes sin él) — el
segundo y tercero son el mismo gate de merge/release que ya aplica siempre (ver arriba); en `human`
además paras en el primero (plan/épica), que en `auto` no existe:

1. **Plan/épica aterrizada** — tras explorar, presenta la épica desglosada en tareas (o la spec) y
   **espera aprobación/feedback** antes de ejecutar nada. (Solo en `human`; en `auto` este paso no para.)
2. **Antes del merge** — cuando todas las tareas están hechas y `idf.py build` está verde, **para antes
   del `merge --no-ff` a `dev`** y pide validación. El hook `guard-merge.sh` lo bloquea si intentas
   mergear sin aprobación.
3. **Antes de cada release/tag** — antes de cortar `release/* → main` y `git tag v<x.y.z>`, pide
   aprobación.

Entre gates ejecutas con autonomía (loop TDD, commits atómicos). El paso de learnings se hace **tras**
la validación del merge.

## `oneshot` — una tarea puntual

Para cambios triviales/menores sin loop ni re-despertar: haces la tarea, verificas, commiteas (o
mergeas si procede), y **paras** (no re-despiertas el backlog). Apto para typos, bumps menores, fixes
de 1 fichero. No exige spec. El gate de learnings solo aplica si el cambio tocó la capa agéntica.

## Cómo se elige el proceso (multi-velocidad)

Ortogonal a la modalidad, escala el proceso al **tamaño del cambio** (ver `loop-engineering`):
trivial/menor → sin spec, loop abreviado u `oneshot`; medio/grande → loop completo, spec/ADR. Las
specs **no siempre son necesarias**.

## Cambiar de modo

`/loop-mode auto` · `/loop-mode human` · `/loop-mode oneshot` · `/loop-mode` (muestra el actual).
