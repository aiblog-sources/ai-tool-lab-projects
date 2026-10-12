#!/usr/bin/env bash
# Runs every benchmark config. Starts a private Ollama (port 11500, data in vendor/ollama-home)
# and a llama-server (port 11600) per config, then calls bench.py. Results go to results/.
#
# Every llama-server we start runs in a systemd user scope capped at 6 GB, so a config that
# asks for too much memory gets killed on its own instead of taking the laptop down.
set -euo pipefail
cd "$(dirname "$0")"
RUNS=${RUNS:-5}
OUT=results/runs.jsonl
MEM=results/memory.tsv
CAP=6G
mkdir -p results vendor/ollama-home
export OLLAMA_HOST=127.0.0.1:11500
OLLAMA=vendor/ollama/bin/ollama
LLAMA_SERVER=vendor/llama.cpp/build/bin/llama-server
# The build moves with vendor/, so point the loader at its shared libraries (only for our
# llama-server: Ollama must keep loading its own bundled copies).
LLAMA_LIBS=$PWD/vendor/llama.cpp/build/bin

HOME=$PWD/vendor/ollama-home OLLAMA_MODELS=$PWD/vendor/ollama-home/models \
  "$OLLAMA" serve > results/ollama-serve.log 2>&1 &
OLLAMA_PID=$!
trap 'kill $OLLAMA_PID 2>/dev/null || true' EXIT
until curl -sf http://127.0.0.1:11500/api/version >/dev/null; do sleep 1; done

# name | gguf file | ollama model name
MODELS=(
  "qwen2.5-1.5b|models/qwen2.5-1.5b-instruct-q4_k_m.gguf|qwen15"
  "llama-3.2-3b|models/llama-3.2-3b-instruct-q4_k_m.gguf|llama32-3b"
)
# config label | extra llama-server flags | ollama num_thread (0 = default)
# "defaults" is each tool exactly as shipped. "matched" gives llama-server the context size and
# slot count Ollama uses. "4-threads" adds 4 threads to both.
CONFIGS=(
  "defaults||0"
  "matched|-c 4096 -np 1|0"
  "4-threads|-c 4096 -np 1 -t 4|4"
)

[ -f "$MEM" ] || printf "label\tmodel\tengine\tpeak_rss_mib\n" > "$MEM"

peak_rss_mib() { awk '/VmHWM/ {printf "%.0f", $2/1024}' "/proc/$1/status"; }

for m in "${MODELS[@]}"; do
  IFS='|' read -r name gguf omodel <<<"$m"
  printf "FROM %s\n" "$PWD/$gguf" > "results/Modelfile-$omodel"
  "$OLLAMA" create "$omodel" -f "results/Modelfile-$omodel" >/dev/null 2>&1
  rm "results/Modelfile-$omodel"
  for c in "${CONFIGS[@]}"; do
    IFS='|' read -r label flags othreads <<<"$c"
    # Unload everything Ollama has loaded so its runner restarts with this config and is the
    # only one alive when we read its memory.
    "$OLLAMA" stop qwen15 >/dev/null 2>&1 || true
    "$OLLAMA" stop llama32-3b >/dev/null 2>&1 || true
    sleep 2
    log="results/llama-server-$name-$label.log"
    # shellcheck disable=SC2086
    systemd-run --user --scope --quiet -p MemoryMax=$CAP -p MemorySwapMax=0 \
      env LD_LIBRARY_PATH="$LLAMA_LIBS" "$LLAMA_SERVER" -m "$gguf" --port 11600 $flags > "$log" 2>&1 &
    LS_PID=$!
    until curl -sf http://127.0.0.1:11600/health >/dev/null; do
      if ! kill -0 "$LS_PID" 2>/dev/null; then break; fi
      sleep 1
    done
    if ! kill -0 "$LS_PID" 2>/dev/null; then
      echo "llama-server $name $label died before serving (memory cap $CAP?)" | tee -a "$log"
      printf "%s\t%s\tllama.cpp\tkilled over %s cap\n" "$label" "$name" "$CAP" >> "$MEM"
      continue
    fi
    python3 bench.py --label "$label" --model-name "$name" --ollama-model "$omodel" \
      --ollama-threads "$othreads" --runs "$RUNS" --out "$OUT" \
      || echo "bench failed for $name $label" | tee -a "$log"
    OL_PID=$(pgrep -n -f "lib/ollama/llama-server")
    printf "%s\t%s\tollama\t%s\n" "$label" "$name" "$(peak_rss_mib "$OL_PID")" >> "$MEM"
    printf "%s\t%s\tllama.cpp\t%s\n" "$label" "$name" "$(peak_rss_mib "$LS_PID")" >> "$MEM"
    kill "$LS_PID"; wait "$LS_PID" 2>/dev/null || true
  done
done
"$OLLAMA" stop qwen15 >/dev/null 2>&1 || true
"$OLLAMA" stop llama32-3b >/dev/null 2>&1 || true
printf "server\t-\tollama serve (Go)\t%s\n" "$(peak_rss_mib "$OLLAMA_PID")" >> "$MEM"
