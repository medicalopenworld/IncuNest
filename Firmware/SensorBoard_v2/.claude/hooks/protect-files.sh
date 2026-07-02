#!/usr/bin/env bash
# PreToolUse (Edit|Write): bloquea ediciones a archivos protegidos.
# Secretos, el lockfile del gestor de componentes de ESP-IDF, internals de git
# y las specs-fuente-de-verdad de OpenSpec (estas solo deben cambiar vía
# openspec-archive-change; los deltas dentro de un cambio abierto sí son
# editables). Devuelve "deny" estructurado.
set -euo pipefail

input="$(cat)"
file_path="$(printf '%s' "$input" | jq -r '.tool_input.file_path // empty')"

[ -z "$file_path" ] && exit 0

deny() {
  jq -n --arg reason "$1" '{
    hookSpecificOutput: {
      hookEventName: "PreToolUse",
      permissionDecision: "deny",
      permissionDecisionReason: $reason
    }
  }'
  exit 0
}

case "$file_path" in
  *.env|*.env.*) [ "${file_path##*/}" = ".env.example" ] || deny "Edición de secretos bloqueada: $file_path. Usa .env.example o el gestor de entornos." ;;
esac

case "$file_path" in
  */dependencies.lock|dependencies.lock) deny "El lockfile del gestor de componentes de ESP-IDF solo cambia vía 'idf.py add-dependency' (component manager), no edición directa: $file_path" ;;
  */managed_components/*|managed_components/*) deny "managed_components/ lo gestiona el component manager de ESP-IDF, no edición directa: $file_path" ;;
  */.git/*|.git/*) deny "Internals de git protegidos: $file_path" ;;
esac

case "$file_path" in
  */openspec/changes/*/specs/*|openspec/changes/*/specs/*) exit 0 ;;  # deltas de un cambio abierto: sí editables
  */openspec/specs/*|openspec/specs/*) deny "Las specs de OpenSpec son fuente de verdad; modifícalas vía deltas y openspec-archive-change, no directamente: $file_path" ;;
esac

exit 0
