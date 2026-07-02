#!/usr/bin/env bash
# PostToolUse (Edit|Write): formatea el archivo C/H editado con clang-format.
# Recibe la ruta como argumento (vía `jq ... | xargs`). Best-effort: nunca
# rompe el flujo (siempre exit 0); los diagnósticos van a stderr. No hay
# toolchain Node/prettier en este repo (proyecto CMake/ESP-IDF puro) — el
# resto de extensiones no se tocan.
set -uo pipefail

file_path="${1:-}"
[ -z "$file_path" ] && exit 0
[ -f "$file_path" ] || exit 0

case "$file_path" in
  *.c|*.h)
    clang-format -i "$file_path" >/dev/null 2>&1 || echo "clang-format: no aplicado a $file_path" >&2
    ;;
esac

exit 0
