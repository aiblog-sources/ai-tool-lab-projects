#!/usr/bin/env bash
# Runs the task from agent-prompt.txt as a Hermes cron job on a local model (llama.cpp's llama-server, CPU only).
# Hermes refuses context windows under 64K tokens. llama-server caps a slot at the model's training context, so
# a 32K model needs YaRN scaling, for example:
#   LLAMA_ARGS="--rope-scaling yarn --rope-scale 2 --yarn-orig-ctx 32768 --override-kv qwen2.context_length=int:65536"
# Usage: LLAMA_SERVER=/path/llama-server MODEL=/path/model.gguf [LLAMA_ARGS=...] ./agent-hermes.sh
# Check the RSS it prints: a 3B model at 64K needed 7.3 GB here.
set -euo pipefail

BASE="${BASE:-$HOME/.cache/agent-schedulers}"
HERE="$(cd "$(dirname "$0")" && pwd)"
TASK="$BASE/agent-task"
export HERMES_HOME="$BASE/hermes-home"
HERMES="$BASE/venv/bin/hermes"
export LD_LIBRARY_PATH="$(dirname "$LLAMA_SERVER")"

"$HERE/make-task.sh" "$TASK"

# shellcheck disable=SC2086
"$LLAMA_SERVER" -m "$MODEL" -c 65536 --parallel 1 -t 4 --jinja ${LLAMA_ARGS:-} --host 127.0.0.1 --port 8089 \
  > "$BASE/results/llama-server.log" 2>&1 &
echo $! > "$BASE/llama-server.pid"
until curl -sf http://127.0.0.1:8089/health > /dev/null; do sleep 2; done
echo "llama-server RSS after load: $(awk '/VmRSS/{print $2}' /proc/"$(cat "$BASE/llama-server.pid")"/status) kB"

cat > "$HERMES_HOME/config.yaml" <<EOF
model:
  default: local
  provider: custom
  base_url: http://127.0.0.1:8089/v1
  api_key: none
EOF

"$HERMES" cron create "0 9 * * *" "$(cat "$HERE/agent-prompt.txt")" --workdir "$TASK" --deliver local --name count-md
start=$(date +%s)
"$HERMES" cron run count-md            # runs the job now and waits for it
echo "elapsed_s $(( $(date +%s) - start ))"
echo "count.txt: $(cat "$TASK/count.txt" 2>/dev/null || echo missing)"
grep print_timing "$BASE/results/llama-server.log" || true
kill "$(cat "$BASE/llama-server.pid")"
