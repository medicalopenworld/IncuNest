#!/usr/bin/env bash
# require-retro.sh — Stop hook que BLINDA el paso 7 (learnings, ADR-0001).
# Si la rama de trabajo cerró sub-tarea(s) de épica (marcó [x] en docs/epics) pero NO registró nada en
# docs/retro/, bloquea el cierre pidiendo el paso de learnings (un aprendizaje aplicado o un descarte
# motivado). Es el guardarraíl que hace imposible saltarse la mejora continua en silencio.
#
# No aplica en modo "oneshot" (cambios puntuales). Tiene salvaguarda anti-bucle.
set -uo pipefail
cd "${CLAUDE_PROJECT_DIR:-.}" || exit 0

input=$(cat 2>/dev/null || echo '{}')
active=$(printf '%s' "$input" | jq -r '.stop_hook_active // false' 2>/dev/null || echo false)

mode="$(cat .claude/.loop-mode 2>/dev/null | tr -d '[:space:]' || true)"
[ "$mode" = "oneshot" ] && exit 0   # los cambios puntuales no exigen retro

base="$(git rev-parse --verify --quiet dev >/dev/null 2>&1 && echo dev || echo HEAD~1)"
# ¿Esta rama marcó algún checkbox [x] en docs/epics respecto a la base?
closed="$(git diff "$base"...HEAD -- docs/epics 2>/dev/null | grep -cE '^\+.*\[x\]' || true)"
[ "${closed:-0}" -eq 0 ] && exit 0   # no se cerró trabajo de épica → nada que exigir

# ¿Se registró algo en docs/retro/ en esta rama?
retro_touched="$(git diff --name-only "$base"...HEAD -- docs/retro 2>/dev/null | grep -c . || true)"
if [ "${retro_touched:-0}" -gt 0 ]; then
  exit 0   # hay registro de learnings → cierre permitido
fi

# Salvaguarda anti-bucle: si ya estamos reintentando por este hook, deja pasar con aviso.
if [ "$active" = "true" ]; then
  printf '{"systemMessage":"⚠ Paso 7 sin registro en docs/retro/ tras varios intentos. Revisa manualmente."}\n'
  exit 0
fi

reason="PASO 7 OBLIGATORIO (ADR-0001): esta rama marcó ${closed} checkbox(es) [x] en docs/epics pero no \
registró nada en docs/retro/. Antes de cerrar, aplica el paso de learnings (skill meta-self-improvement): \
repasa la sub-tarea sobre la EVIDENCIA (diff, reintentos, fricciones) y, según el umbral (segunda \
ocurrencia, coste observable, generalizable), o bien enruta un aprendizaje a su sitio (hook/rule/skill/\
agent/.claude, CLAUDE.md o docs) y regístralo en docs/retro/<fecha>-<slug>.md, o bien anótalo ahí como \
descartado con motivo (filtro de segunda ocurrencia). No cierres sin dejar ese registro trazable."

if command -v jq >/dev/null 2>&1; then
  jq -nc --arg r "$reason" '{decision:"block", reason:$r}'
fi
exit 0
