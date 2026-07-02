---
description: Prepara y publica una release desde dev a main con tag semver y back-merge (gitflow). Usar cuando hay épicas terminadas en dev listas para producción.
argument-hint: "<major|minor|patch | X.Y.Z>"
disable-model-invocation: true
allowed-tools: Bash(git *), Bash(idf.py *), Bash(grep *)
---

# /git-release — publicar release a main

> **Atribución**: ningún commit/tag/PR del release lleva `Co-Authored-By: Claude` ni `Claude-Session`. Autor único: Pablo Sánchez Bergasa (GitHub: `pablo18393`).

No hay `package.json` en este repo (CMake/ESP-IDF puro, no Node). La fuente de verdad de la
versión es la macro `SB_PROTO_FW_VERSION`, definida en
`SensorBoard/components/usb_comm/include/sensorBoard_comm_protocol.h` (parte de Phase 1, ver
`Firmware/docs/superpowers/plans/2026-06-08-sensorboard-phase1.md`).

Versión actual:
```bash
grep -oP '(?<=SB_PROTO_FW_VERSION\s{2}")[^"]+' SensorBoard/components/usb_comm/include/sensorBoard_comm_protocol.h 2>/dev/null
```

> **Si el comando anterior no imprime nada**: casi seguro que `sensorBoard_comm_protocol.h` todavía
> no existe (Phase 1 no ha aterrizado en este repo todavía). PARA y dilo explícitamente al
> usuario — no hay versión que leer, no sigas con el release asumiendo `0.0.0` ni ningún valor por
> defecto silencioso. Este comando solo tiene sentido una vez exista el header.

Argumento (bump o versión exacta): **$ARGUMENTS**

## Pasos

1. Parte de `dev` actualizado y limpio. Calcula la nueva versión semver a partir de `$ARGUMENTS` (major/minor/patch o `X.Y.Z` explícito) y de la versión actual leída arriba.
2. Crea la rama de release:
   ```bash
   git checkout dev
   git checkout -b release/<ver>
   ```
3. Estabiliza (solo fixes, nada de features nuevas):
   - Actualiza el valor de `SB_PROTO_FW_VERSION` en `sensorBoard_comm_protocol.h` a `<ver>`.
   - Actualiza `CHANGELOG.md`: mueve lo de `## [Unreleased]` a `## [<ver>] - <fecha actual>`.
   - Corre `idf.py build`. Debe compilar sin errores (gate de compilación; los tests Unity en
     hardware real son manuales, ver skill `tdd-cycle`).
   - Commit `chore(release): v<ver>`.
4. Integra en `main` y etiqueta:
   ```bash
   git checkout main
   git merge --no-ff release/<ver> -m "release: v<ver>"
   git tag -a v<ver> -m "release v<ver>"
   ```
5. Back-merge a `dev` para no perder los commits de estabilización:
   ```bash
   git checkout dev
   git merge --no-ff release/<ver> -m "merge: release/<ver> -> dev"
   git branch -d release/<ver>
   ```
6. Reporta la versión publicada, el tag y el grafo (`git log --oneline --graph -20`).

Nota: el push a `main`/`dev` lo hace el usuario manualmente (los hooks bloquean push directo desde la sesión). Recuérdaselo con los comandos exactos (`git push origin main --tags`, `git push origin dev`).
