---
description: Inicia una rama de feature desde dev siguiendo gitflow. Usar al empezar a trabajar en una feature nueva.
argument-hint: "<slug-de-la-feature>"
disable-model-invocation: true
allowed-tools: Bash(git *)
---

# /git-feature-start — nueva rama de feature

Slug: **$ARGUMENTS**

Estado actual:

- Rama: !`git branch --show-current`
- Limpio: !`git status --short`

## Pasos

1. Si hay cambios sin commitear, PARA y avisa (no arranques una feature sobre trabajo sucio).
2. Cambia a `dev` y actualiza:
   ```bash
   git checkout dev
   git pull --ff-only 2>/dev/null || true
   ```
3. Crea la rama de feature (normaliza `$ARGUMENTS` a kebab-case):
   ```bash
   git checkout -b feat/$ARGUMENTS
   ```
4. Confirma la rama creada y recuerda al usuario que ahora puede ejecutar `/loop-run "<feature>"`.

Si `dev` no existe aún, créala desde `main` antes del paso 3.
