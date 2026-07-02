#!/usr/bin/env bash
# SubagentStop: telemetría ligera del loop (qué rol terminó y cuándo).
# No bloquea; solo registra en .claude/logs/ (gitignored).
set -uo pipefail

input="$(cat)"
agent_type="$(printf '%s' "$input" | jq -r '.agent_type // "unknown"')"
session="$(printf '%s' "$input" | jq -r '.session_id // "?"')"
log_dir="${CLAUDE_PROJECT_DIR:-.}/.claude/logs"
mkdir -p "$log_dir"
printf '%s\tsubagent_stop\t%s\t%s\n' "$(date -u +%FT%TZ)" "$agent_type" "$session" >> "$log_dir/loop.log"
exit 0
