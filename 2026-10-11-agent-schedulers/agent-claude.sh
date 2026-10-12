#!/usr/bin/env bash
# The task from agent-prompt.txt, run the way a systemd timer or cron would run it: a fresh process,
# no terminal, Claude Code in headless mode. Uses your Claude Code login (no API key needed).
set -euo pipefail
BASE="${BASE:-$HOME/.cache/agent-schedulers}"
HERE="$(cd "$(dirname "$0")" && pwd)"
TASK="$BASE/agent-task"
mkdir -p "$BASE/results"
"$HERE/make-task.sh" "$TASK"
start=$(date +%s.%N)
systemd-run --user --wait --collect --quiet -p WorkingDirectory="$TASK" \
  -p StandardOutput=truncate:"$BASE/results/claude-run.json" -E PATH="$PATH" -E HOME="$HOME" \
  "$(command -v claude)" -p "$(cat "$HERE/agent-prompt.txt")" \
  --model claude-haiku-5-5 --permission-mode auto --output-format json
echo "wall_s $(echo "$(date +%s.%N) - $start" | bc)"
echo "count.txt: $(cat "$TASK/count.txt")"
