---
description: Ejecuta una retro y automejora del framework on-demand (sin estar dentro de un loop). Usar para extraer aprendizajes y mejorar .claude/CLAUDE.md tras una sesión de trabajo.
argument-hint: "[tema o feature de referencia]"
disable-model-invocation: true
allowed-tools: Bash(git *), Read, Write, Edit, Grep, Glob, Task
---

# /meta-retro — retro + automejora del framework

Referencia: **$ARGUMENTS**

Cambios recientes: !`git log --oneline -15`

## Ejecución

Delega al agente `retro-improver` (vía Task) siguiendo el skill `meta-self-improvement`:

1. Analiza la sesión/iteración reciente con evidencia (`.claude/logs/loop.log`, diff, fricciones).
2. Escribe los aprendizajes en `docs/retro/<fecha-actual>-<ref>.md`.
3. Aplica mejoras concretas a `.claude/` (hooks, rules, agents, skills) y `CLAUDE.md`, eligiendo la frontera correcta (hook/rule/skill/CLAUDE.md/agent).
4. Respeta los límites: no toques `openspec/specs/**` ni relajes seguridad.

Al terminar, reporta el retro escrito y la lista de mejoras aplicadas (archivo + qué + por qué). Si procede, deja el commit `chore(meta): retro + improve .claude`.
