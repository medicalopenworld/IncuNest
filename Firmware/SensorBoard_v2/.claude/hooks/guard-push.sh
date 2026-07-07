#!/usr/bin/env bash
# PreToolUse (Bash, if: "Bash(git push *)"): impide push directo a main/dev.
# El flujo gitflow exige que main/dev se actualicen vía merge --no-ff
# (/git-feature-finish, /git-release), no con push directo desde una sesión.
set -euo pipefail

input="$(cat)"
cmd="$(printf '%s' "$input" | jq -r '.tool_input.command // empty')"

if printf '%s' "$cmd" | grep -Eq 'git[[:space:]]+push.*(origin[[:space:]]+)?(main|dev)([[:space:]]|$|:)'; then
  jq -n '{
    hookSpecificOutput: {
      hookEventName: "PreToolUse",
      permissionDecision: "deny",
      permissionDecisionReason: "Push directo a main/dev bloqueado (gitflow). Integra vía feature-finish (a dev) o release (a main)."
    }
  }'
  exit 0
fi

exit 0
