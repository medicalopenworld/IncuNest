---
name: meta-self-improvement
description: Protocolo de retro y automejora del framework Genesis. Usar en el stage final del loop para extraer aprendizajes y mejorar .claude/ y CLAUDE.md (hooks, rules, prompts de agentes, skills), o cuando una iteración revele fricción repetida que convenga sistematizar.
---

# Automejora del framework

Genesis se automejora: cada iteración del loop termina dejando el framework mejor configurado. Lo
ejecuta el agente `retro-improver`. El criterio canónico de este protocolo se documenta como un ADR
en `docs/adr/` (plantilla en [`docs/adr/0000-template.md`](../../../docs/adr/0000-template.md); a
día de hoy no existe todavía un ADR numerado específico para el "paso 7" — cuando se escriba, sigue
la convención `docs/adr/<NNNN>-<slug>.md` del skill `git-flow`, p.ej.
`docs/adr/0001-stage-learnings-paso-7.md`). El texto operativo se repite además en los hooks
`unattended-loop.sh`/`require-retro.sh` por necesidad — deben poder aplicarse de forma autónoma sin
depender de que el modelo relea el ADR. El "paso 7" es el stage 11 del loop formal (ver tabla de
stages en `loop-engineering`).

## Regla central: filtro de segunda ocurrencia

**No conviertas en convención un fallo de primera vez.** La primera ocurrencia puede ser casualidad; commitear una regla por cada error ahoga las instrucciones en lecciones de un solo uso y, paradójicamente, hace que el modelo las ignore (un `CLAUDE.md`/rules inflado se desobedece). La convención nace en la **segunda ocurrencia del mismo patrón** — o si hay coste observable (reintento, corrección manual, fallo de CI) o es claramente generalizable. Lo demás se **registra como descartado con motivo**, no se sistematiza.

## Pregunta de enrutado (decisiva)

Antes de elegir dónde aplicar una mejora, pregunta en orden:

1. **¿Debe ser imposible sin depender del contexto del modelo?** → **hook** (determinista; gana a cualquier instrucción de texto).
2. ¿Es un procedimiento multi-paso repetido? → **skill**.
3. ¿Es una convención de una zona del repo? → **rule** (`paths:`).
4. ¿El agente lo necesita en TODA sesión y no lo infiere del código? → **CLAUDE.md** (conciso).
5. ¿Es decisión de diseño o hallazgo de investigación? → **docs/** (ADR/research/retro).
6. Si nada aplica → descartar (anotándolo).

## Taxonomía de aprendizajes (clasifica antes de enrutar)

A error evitable/guardarraíl · B convención ignorada · C procedimiento repetido · D hecho de sesión que falta · E fricción de proceso · F prompt/agente desalineado · G deuda de doc · H aprendizaje del stack. Detalle y ejemplos en el research. ROI alto y sistematizable: A–C.

## Protocolo

1. **Observa la iteración** (evidencia, no intuición):
   - `.claude/logs/loop.log`, el diff completo, qué stages necesitaron reintentos o intervención.
   - ¿Qué prompt no disparó el agente correcto? ¿Qué regla faltó? ¿Qué hook habría evitado un error? ¿Qué se repitió a mano?
2. **Registra aprendizajes** en `docs/retro/<YYYY-MM-DD>-<feature>.md`: qué fue bien, qué costó, qué se repitió, qué decisión conviene convertir en convención.
3. **Aplica mejoras concretas** (lo esencial), eligiendo la frontera correcta:

| Síntoma observado                        | Mejora                                            | Dónde                                         |
| ---------------------------------------- | ------------------------------------------------- | --------------------------------------------- |
| Un error que debería ser imposible       | Guardarraíl determinista                          | **hook** (`.claude/hooks/` + `settings.json`) |
| Convención ignorada en una zona del repo | Regla con `paths:`                                | **rule** (`.claude/rules/`)                   |
| Procedimiento multi-paso repetido        | Empaquetar el procedimiento                       | **skill** (`.claude/skills/`)                 |
| Hecho estable que falta en cada sesión   | Añadir (conciso)                                  | **CLAUDE.md**                                 |
| Un agente no se activó o divagó          | Afinar `description` (disparo) o el system prompt | **agent** (`.claude/agents/`)                 |

## Límites

- No edites `openspec/specs/**` (protegido) ni relajes guardarraíles de seguridad.
- Toda mejora debe justificarse por algo observado en ESTA iteración, no especulativa.
- Mide el coste: no añadas fricción que ralentice sin aportar valor proporcional.
- Mejoras arriesgadas → déjalas como propuesta anotada para validación humana, no las apliques en silencio.

## Mantenimiento del momentum (continuidad entre ventanas)

El framework recuerda dónde está mediante tres piezas que **debes mantener al día al cerrar cada tarea**:

1. **`ESTADO.md`** (raíz): épica/tarea activa, próximo paso inmediato, decisiones vigentes, rama de trabajo, cómo reanudar. Es lo que carga `CLAUDE.md` (`@ESTADO.md`) en cada sesión.
2. **`docs/epics/`**: marca los checkboxes de la subtarea cerrada y el estado de la épica (⏳/🔄/✅) en su archivo y en el índice `README.md`.
3. **Memoria de subagente** (`memory: project` en `retro-improver`/`product-manager`): recuerda decisiones y contexto que un agente nuevo necesitaría.

Protocolo al terminar una tarea/épica: actualizar `ESTADO.md` → marcar checkbox en `docs/epics/` → commit. Si paras a mitad, `ESTADO.md` debe apuntar exactamente a la siguiente tarea pendiente para que una ventana nueva reanude sin ambigüedad.

## Salida

Commit `chore(meta): retro + improve .claude` con: el retro escrito y la lista de cambios a `.claude`/`CLAUDE.md` (archivo + qué + por qué).
