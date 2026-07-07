#!/usr/bin/env bash
# unattended-loop.sh — Stop hook del MODO DESATENDIDO.
# Mantiene el ciclo planificar→ejecutar→planificar… mientras queden tareas en el
# backlog (checkboxes "[ ]" en docs/epics). El humano siempre puede parar con Esc.
#
# Parada automática:
#   - backlog agotado (0 pendientes) → para.
#   - sin progreso durante 3 ciclos seguidos (no baja el nº de pendientes) → para,
#     para no girar en vano si me atasco.
#
# Estado entre ciclos en .claude/.unattended-state (gitignored): "<pendientes> <ciclos_sin_progreso>".
set -euo pipefail

DIR="${CLAUDE_PROJECT_DIR:-$(pwd)}"
STATE="$DIR/.claude/.unattended-state"
MAX_STALE=3

input=$(cat 2>/dev/null || echo '{}')

# El re-despertar automático SOLO aplica en modo "auto" (ver skill loop-modes / .claude/.loop-mode).
# En "human" el control es del usuario; en "oneshot" es una tarea puntual; sin modo fijado, no forzamos
# el loop (el SessionStart ya pide elegir modo). El gate de learnings (require-retro) sigue aparte.
mode="$(cat "$DIR/.claude/.loop-mode" 2>/dev/null | tr -d '[:space:]' || true)"
if [ "$mode" != "auto" ]; then
  rm -f "$STATE"
  # El modo se valida contra una allowlist y la salida se construye con jq (escape seguro):
  # el contenido del fichero .loop-mode nunca se interpola crudo en el JSON de control del hook.
  case "$mode" in
    human | oneshot) msg="Modo ${mode}: el loop desatendido (auto) está inactivo; el control es tuyo." ;;
    *) msg="Modo del loop no fijado o no válido. Elige con /loop-mode auto|human|oneshot (no re-despierto automáticamente)." ;;
  esac
  jq -nc --arg m "$msg" '{systemMessage: $m}'
  exit 0
fi

# Cuenta checkboxes pendientes en el backlog de épicas.
pending=$(grep -rEl '^\s*-\s*\[ \]' "$DIR/docs/epics" 2>/dev/null | xargs grep -hcE '^\s*-\s*\[ \]' 2>/dev/null | paste -sd+ - | bc 2>/dev/null || echo 0)
pending=${pending:-0}

# Carga estado previo.
prev_pending=0; stale=0
if [ -f "$STATE" ]; then read -r prev_pending stale < "$STATE" 2>/dev/null || true; fi
prev_pending=${prev_pending:-0}; stale=${stale:-0}

stop() { # $1 = mensaje para el usuario
  rm -f "$STATE"
  jq -nc --arg m "$1" '{systemMessage: $m}'
  exit 0
}

# Backlog agotado → fin del loop.
if [ "$pending" -eq 0 ]; then
  stop "✓ Modo desatendido: backlog de docs/epics sin tareas pendientes. Loop detenido."
fi

# Salvaguarda anti-bucle: ¿hubo progreso respecto al ciclo anterior?
if [ "$pending" -lt "$prev_pending" ]; then
  stale=0
else
  stale=$((stale + 1))
fi

if [ "$stale" -ge "$MAX_STALE" ]; then
  stop "⏸ Modo desatendido en pausa: $pending tareas pendientes pero sin progreso en $MAX_STALE ciclos. Revisa manualmente (posible bloqueo)."
fi

# Persiste estado y re-despierta para seguir con el plan.
printf '%s %s\n' "$pending" "$stale" > "$STATE"

reason="MODO DESATENDIDO ACTIVO — no te detengas: quedan $pending tareas en docs/epics. \
Continúa el plan por ti mismo: (1) lee @ESTADO.md y docs/epics/README.md para el próximo paso inmediato; \
(2) crea la rama feat/* (producto) o meta/* (capa agéntica) desde dev si toca; (3) ejecuta siguiendo el loop engineering (TDD, commits atómicos Conventional); \
(4) gate verde 'idf.py build' (compile-only; el flasheo/monitor en hardware real sigue siendo manual); (5) merge --no-ff a dev (guard-merge.sh pide aprobación humana igual en modo auto — para, muestra el estado y espera antes de crear la marca .claude/.merge-approved); \
(6) marca el checkbox [x] en docs/epics y actualiza ESTADO.md; \
(7) PASO DE LEARNINGS (skill meta-self-improvement, convención en docs/adr/0000-template.md): repasa la sub-tarea sobre la EVIDENCIA \
(diff, reintentos, fricciones). Si detectas un aprendizaje que cumpla el umbral —segunda ocurrencia del \
mismo patrón, coste observable o generalizable— enrútalo a su sitio (hook/rule/skill/agent en .claude, \
CLAUDE.md, o docs) y regístralo en docs/retro/<fecha>-<slug>.md; si es de un caso único, anótalo como \
descartado con motivo (filtro de segunda ocurrencia: NO crees reglas por un fallo de primera vez). \
Si la mejora es arriesgada (tocar hooks de seguridad, bajar umbrales), déjala como propuesta para revisión humana. \
Si el backlog quedara vacío, propón y registra nuevas tareas de optimización antes de parar. \
Avanza UNA tarea o sub-tarea coherente por ciclo."

# jq construye el JSON con escaping correcto. Sin jq, no emitimos JSON (parar es seguro):
# evitamos un fallback con escape frágil que podría romper el control del hook.
if command -v jq >/dev/null 2>&1; then
  jq -nc --arg r "$reason" '{decision:"block", reason:$r}'
fi
exit 0
