#!/usr/bin/env bash
# Runs every benchmark config. Starts a private Ollama (port 11500, data in vendor/ollama-home)
# and a llama-server (port 11600) per config, then calls bench.py. Results go to results/.
set -euo pipefail
cd "$(dirname "$0")"
RUNS=${RUNS:-5}
OUT=results/runs.jsonl
MEM=results/memory.tsv
mkdir -p results vendor/ollama-home
export OLLAMA_HOST=127.0.0.1:11500
OLLAMA=vendor/ollama/bin/ollama
LLAMA_SERVER=vendor/llama.cpp/build/bin/llama-server

if ! curl -sf http://127.0.0.1:11500/api/version >/dev/null; then
  HOME=$PWD/vendor/ollama-home OLLAMA_MODELS=$PWD/vendor/ollama-home/models \
    "$OLLAMA" serve > results/ollama-serve.log 2>&1 &
  OLLAMA_PID=$!
  trap 'kill $OLLAMA_PID 2>/dev/null || true' EXIT
  until curl -sf http://127.0.0.1:11500/api/version >/dev/null; do sleep 1; done
fi

# name | gguf file | ollama model name
MODELS=(
  "qwen2.5-1.5b|models/qwen2.5-1.5b-instruct-q4_k_m.gguf|qwen15"
  "llama-3.2-3b|models/llama-3.2-3b-instruct-q4_k_m.gguf|llama32-3b"
)
# config label | extra llama-server flags | ollama num_thread (0 = default)
CONFIGS=(
  "defaults||0"
  "4-threads|-c 4096 -np 1 -t 4|4"
)

[ -f "$MEM" ] || printf "label\tmodel\tengine\tpeak_rss_mib\n" > "$MEM"

peak_rss_mib() { awk '/VmHWM/ {printf "%.0f", $2/1024}' "/proc/$1/status"; }

for m in "${MODELS[@]}"; do
  IFS='|' read -r name gguf omodel <<<"$m"
  printf "FROM %s\n" "$PWD/$gguf" > "vendor/Modelfile-$omodel"
  "$OLLAMA" create "$omodel" -f "vendor/Modelfile-$omodel" >/dev/null 2>&1
  for c in "${CONFIGS[@]}"; do
    IFS='|' read -r label flags othreads <<<"$c"
    # Unload whatever Ollama has loaded so its runner restarts with this config.
    "$OLLAMA" stop "$omodel" >/dev/null 2>&1 || true
    sleep 2
    # shellcheck disable=SC2086
    "$LLAMA_SERVER" -m "$gguf" --port 11600 $flags > "results/llama-server-$name-$label.log" 2>&1 &
    LS_PID=$!
    until curl -sf http://127.0.0.1:11600/health >/dev/null; do sleep 1; done
    python3 bench.py --label "$label" --model-name "$name" --ollama-model "$omodel" \
      --ollama-threads "$othreads" --runs "$RUNS" --out "$OUT"
    OL_PID=$(pgrep -f "vendor/ollama/lib/ollama/llama-server" | head -1)
    printf "%s\t%s\tollama\t%s\n" "$label" "$name" "$(peak_rss_mib "$OL_PID")" >> "$MEM"
    printf "%s\t%s\tllama.cpp\t%s\n" "$label" "$name" "$(peak_rss_mib "$LS_PID")" >> "$MEM"
    kill "$LS_PID"; wait "$LS_PID" 2>/dev/null || true
  done
done
"$OLLAMA" stop qwen15 >/dev/null 2>&1 || true
"$OLLAMA" stop llama32-3b >/dev/null 2>&1 || true
