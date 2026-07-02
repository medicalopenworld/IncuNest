# Retro — EPIC-000 · Adaptación del framework Genesis a ESP-IDF

**Fecha de cierre:** 2026-07-03
**Change OpenSpec:** `openspec/changes/archive/2026-07-02-adapt-genesis-esp-idf/`

## Qué se hizo

Retarget completo del framework agéntico (copiado de un monorepo TypeScript/Astro/Next.js) al stack real del SensorBoard: ESP32-S3, ESP-IDF v6, C, FreeRTOS, Unity/`idf.py`. 9 secciones de tareas: config OpenSpec, rules, 11 agentes, 8 hooks, docs mínimas, skills, `CLAUDE.md` y verificación final. Todo completado y validado (`openspec validate --all` limpio).

## Aprendizajes

1. **Un `.gitignore` heredado puede invalidar todo el trabajo en silencio.** El patrón raíz `.claude` excluía el framework completo de git — nada de la épica era commiteable hasta detectarlo con `git check-ignore`. Lección: al versionar directorios de configuración nuevos, verificar con `git check-ignore` antes de dar el trabajo por terminado.
2. **Los ficheros "personales" copiados de otra máquina son deuda inmediata**: `settings.local.json` traía rutas absolutas de macOS de otro autor y permisos de un toolchain (pnpm/npm) inexistente aquí. Al importar un framework, auditar primero lo no versionado.
3. **Una regla siempre-on debe reflejarse en toda la documentación que describe modos**: al hacer incondicional el gate de merge (`guard-merge.sh`), el skill `loop-modes` quedó describiendo un modo `auto` "sin gates" que ya no existía. Los cambios de comportamiento en hooks exigen grep de la documentación de skills que los mencione.

## Señal para el futuro

La verificación automatizable en este proyecto es **solo `idf.py build`**; cualquier evidencia en hardware real (flash/monitor/Unity on-target) es manual. Las fases del roadmap deben redactar sus criterios de verify con esa separación explícita.
