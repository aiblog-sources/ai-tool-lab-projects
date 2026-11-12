#!/usr/bin/env bash
# Does /v1 keep a library model's shipped temperature on Ollama 0.40.16? Starts 0.40.16 on the model store of the
# 2026-10-23 test (LIBRARY_MODELS, pulled from the Ollama library then), sends one /v1 request with no sampler
# fields to each model, and records the sampler line its llama-server logs. Writes results/library-v1-check.txt.
set -euo pipefail
cd "$(dirname "$0")"
LIBRARY_MODELS=${LIBRARY_MODELS:?set LIBRARY_MODELS to an Ollama models folder with ministral-3:3b and llama-guard3:1b}
OLLAMA=$PWD/vendor/ollama-0.40.16/bin/ollama
PORT=11500 LOG=results/logs/library-check.log OUT=results/library-v1-check.txt
trap 'pkill -f "$OLLAMA serve" 2>/dev/null || true' EXIT
systemd-run --user --scope --quiet -p MemoryMax=6G -p MemorySwapMax=0 --setenv=OLLAMA_MODELS="$LIBRARY_MODELS" \
  --setenv=OLLAMA_NOPRUNE=1 --setenv=OLLAMA_HOST=127.0.0.1:$PORT "$OLLAMA" serve > "$LOG" 2>&1 &
until curl -sf "http://127.0.0.1:$PORT/api/version" >/dev/null; do sleep 0.5; done
: > "$OUT"
for m in ministral-3:3b llama-guard3:1b; do
  shipped=$(curl -s "http://127.0.0.1:$PORT/api/show" -d "{\"model\":\"$m\"}" | python3 -c 'import json,sys; print(json.load(sys.stdin).get("parameters","").replace("\n","; "))')
  start=$(wc -l < "$LOG")
  curl -s "http://127.0.0.1:$PORT/v1/chat/completions" -H 'Content-Type: application/json' \
    -d "{\"model\":\"$m\",\"messages\":[{\"role\":\"user\",\"content\":\"Say hi.\"}],\"max_tokens\":4}" > /dev/null
  sleep 1
  { echo "## Ollama 0.40.16 | $m | /v1 | sent nothing | shipped: $shipped"; tail -n +"$((start + 1))" "$LOG" | grep -E "top_k =" | tail -1; echo; } >> "$OUT"
done
sed -i -e "s#$LIBRARY_MODELS#<library-models>#g" -e "s#$HOME#~#g" "$LOG"
cat "$OUT"
