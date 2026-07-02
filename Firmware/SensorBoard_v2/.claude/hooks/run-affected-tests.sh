#!/usr/bin/env bash
# Stop: gate de compilación al cerrar el turno. Si hay cambios de C/H sin
# commitear, corre `idf.py build` (solo compila, NUNCA flashea ni abre el
# puerto serie) y bloquea el cierre si el build falla. El proyecto ESP-IDF
# (CMakeLists.txt en la raíz) aún puede no existir — se crea en un cambio
# futuro — en cuyo caso no hay nada que compilar todavía y no es un fallo.
# Respeta stop_hook_active para no chocar con el cap de bloqueos.
set -uo pipefail

input="$(cat)"
active="$(printf '%s' "$input" | jq -r '.stop_hook_active // false')"
[ "$active" = "true" ] && exit 0

project_dir="${CLAUDE_PROJECT_DIR:-.}"

# Solo si hay cambios sin commitear en fuentes C (evita correr en sesiones de lectura/docs).
if ! git -C "$project_dir" diff --quiet -- '*.c' '*.h' 2>/dev/null; then
  if [ ! -f "$project_dir/CMakeLists.txt" ]; then
    exit 0   # el proyecto ESP-IDF aún no está scaffolded: nada que compilar
  fi

  if ! out="$(cd "$project_dir" && idf.py build 2>&1)"; then
    reason="$(printf '%s' "$out" | tail -20)"
    jq -n --arg r "$reason" '{decision: "block", reason: ("idf.py build en rojo; corrige antes de cerrar:\n" + $r)}'
    exit 0
  fi
fi

exit 0
