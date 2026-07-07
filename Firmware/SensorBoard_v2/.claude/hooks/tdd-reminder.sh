#!/usr/bin/env bash
# PostToolUse (Write): si se crea un archivo de componente C/H sin referencia
# en ningún test Unity, recuerda el ciclo TDD (red primero). No bloquea;
# inyecta contexto adicional.
set -uo pipefail

input="$(cat)"
file_path="$(printf '%s' "$input" | jq -r '.tool_input.file_path // empty')"
[ -z "$file_path" ] && exit 0

project_dir="${CLAUDE_PROJECT_DIR:-.}"

# Solo código de componente C/H bajo components/**; los propios tests bajo
# test_apps/** quedan excluidos (son el test, no el código a testear).
case "$file_path" in
  */test_apps/*|test_apps/*) exit 0 ;;
  */components/*.c|*/components/*.h|components/*.c|components/*.h) ;;
  *) exit 0 ;;
esac

# Los tests Unity se agrupan por componente en test_apps/<comp>_test/main/test_main.c,
# no 1:1 por archivo fuente. Si aún no existe ningún test_apps, no hay nada que comprobar.
[ -d "$project_dir/test_apps" ] || exit 0

base="$(basename "$file_path")"
name="${base%.*}"

if ! grep -rq --include='test_main.c' -- "$name" "$project_dir/test_apps" 2>/dev/null; then
  jq -n --arg f "$file_path" '{
    hookSpecificOutput: {
      hookEventName: "PostToolUse",
      additionalContext: ("Unity: \($f) no aparece referenciado en ningún test_apps/**/test_main.c. Si esto es lógica de negocio (protocolo, cómputo de sensor), escribe el test primero (rojo) antes de seguir implementando.")
    }
  }'
fi

exit 0
