#!/usr/bin/env bash
# Cold start: how long from "nothing loaded" to the first generated token.
#   Ollama:    unload the model, then time one request (Ollama loads on demand).
#   llama.cpp: start llama-server, wait for /health, then send the same request.
# Both use a 4096-token context and 1 slot. The GGUF files are already in the page cache
# after run_all.sh, so this measures loading from RAM, not from disk.
set -euo pipefail
export LC_ALL=C   # printf %f needs a dot as the decimal separator
cd "$(dirname "$0")"
REPS=${REPS:-3}
OUT=results/coldstart.tsv
export OLLAMA_HOST=127.0.0.1:11500
OLLAMA=vendor/ollama/bin/ollama
LLAMA_SERVER=vendor/llama.cpp/build/bin/llama-server
LLAMA_LIBS=$PWD/vendor/llama.cpp/build/bin
mkdir -p results

HOME=$PWD/vendor/ollama-home OLLAMA_MODELS=$PWD/vendor/ollama-home/models \
  "$OLLAMA" serve > results/ollama-serve-coldstart.log 2>&1 &
OLLAMA_PID=$!
trap 'kill $OLLAMA_PID 2>/dev/null || true' EXIT
until curl -sf http://127.0.0.1:11500/api/version >/dev/null; do sleep 1; done

now() { date +%s.%N; }
printf "model\tengine\trep\tseconds_to_first_token\n" > "$OUT"

for m in "qwen2.5-1.5b|models/qwen2.5-1.5b-instruct-q4_k_m.gguf|qwen15" \
         "llama-3.2-3b|models/llama-3.2-3b-instruct-q4_k_m.gguf|llama32-3b"; do
  IFS='|' read -r name gguf omodel <<<"$m"
  for rep in $(seq 1 "$REPS"); do
    "$OLLAMA" stop "$omodel" >/dev/null 2>&1 || true
    sleep 3
    t0=$(now)
    curl -sf http://127.0.0.1:11500/api/generate -d \
      "{\"model\":\"$omodel\",\"prompt\":\"Hello\",\"raw\":true,\"stream\":false,\"options\":{\"num_predict\":1}}" >/dev/null
    printf "%s\tollama\t%s\t%.2f\n" "$name" "$rep" "$(echo "$(now) - $t0" | bc)" >> "$OUT"

    t0=$(now)
    env LD_LIBRARY_PATH="$LLAMA_LIBS" "$LLAMA_SERVER" -m "$gguf" --port 11600 -c 4096 -np 1 \
      > /dev/null 2>&1 &
    LS_PID=$!
    until curl -sf http://127.0.0.1:11600/health >/dev/null; do sleep 0.1; done
    curl -sf http://127.0.0.1:11600/completion -d '{"prompt":"Hello","n_predict":1}' >/dev/null
    printf "%s\tllama.cpp\t%s\t%.2f\n" "$name" "$rep" "$(echo "$(now) - $t0" | bc)" >> "$OUT"
    kill "$LS_PID"; wait "$LS_PID" 2>/dev/null || true
  done
done
"$OLLAMA" stop qwen15 >/dev/null 2>&1 || true
"$OLLAMA" stop llama32-3b >/dev/null 2>&1 || true
cat "$OUT"
