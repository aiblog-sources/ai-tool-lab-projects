#!/usr/bin/env bash
# Follow-up to run_all.sh: start our llama-server with the exact flags Ollama 0.40.3 passes to
# its bundled llama-server (copied from results/ollama-serve.log), so the only differences left
# are the llama.cpp version and Ollama's Go proxy in front. Also records anonymous vs
# file-backed memory, which separates mmap'd model pages from real allocations.
set -euo pipefail
cd "$(dirname "$0")"
RUNS=${RUNS:-5}
OUT=results/runs.jsonl
MEM=results/memory-detail.tsv
export OLLAMA_HOST=127.0.0.1:11500
OLLAMA=vendor/ollama/bin/ollama
LLAMA_SERVER=vendor/llama.cpp/build/bin/llama-server
LLAMA_LIBS=$PWD/vendor/llama.cpp/build/bin
OLLAMA_FLAGS="-c 4096 -np 1 --load-mode none --flash-attn auto -b 512 -ub 512 --context-shift --keep 4"
mkdir -p results

HOME=$PWD/vendor/ollama-home OLLAMA_MODELS=$PWD/vendor/ollama-home/models \
  "$OLLAMA" serve > results/ollama-serve-same-flags.log 2>&1 &
OLLAMA_PID=$!
trap 'kill $OLLAMA_PID 2>/dev/null || true' EXIT
until curl -sf http://127.0.0.1:11500/api/version >/dev/null; do sleep 1; done

[ -f "$MEM" ] || printf "label\tmodel\tengine\tpeak_rss_mib\trss_anon_mib\trss_file_mib\n" > "$MEM"
mem_row() {  # label model engine pid
  awk -v l="$1" -v m="$2" -v e="$3" '
    /VmHWM/ {h=$2} /RssAnon/ {a=$2} /RssFile/ {f=$2}
    END {printf "%s\t%s\t%s\t%.0f\t%.0f\t%.0f\n", l, m, e, h/1024, a/1024, f/1024}' "/proc/$4/status"
}

for m in "qwen2.5-1.5b|models/qwen2.5-1.5b-instruct-q4_k_m.gguf|qwen15" \
         "llama-3.2-3b|models/llama-3.2-3b-instruct-q4_k_m.gguf|llama32-3b"; do
  IFS='|' read -r name gguf omodel <<<"$m"
  # label | extra llama-server flags | ollama num_thread
  for c in "ollama-flags||0" "ollama-flags-4t|-t 4|4" "mmap-4096|-c 4096 -np 1|0"; do
    IFS='|' read -r label extra othreads <<<"$c"
    flags="$OLLAMA_FLAGS $extra"
    [ "$label" = "mmap-4096" ] && flags="$extra"
    # Unload both models, so the only Ollama runner alive is the one this config starts.
    "$OLLAMA" stop qwen15 >/dev/null 2>&1 || true
    "$OLLAMA" stop llama32-3b >/dev/null 2>&1 || true
    sleep 2
    # shellcheck disable=SC2086
    systemd-run --user --scope --quiet -p MemoryMax=6G -p MemorySwapMax=0 \
      env LD_LIBRARY_PATH="$LLAMA_LIBS" "$LLAMA_SERVER" -m "$gguf" --port 11600 $flags \
      > "results/llama-server-$name-$label.log" 2>&1 &
    LS_PID=$!
    until curl -sf http://127.0.0.1:11600/health >/dev/null; do sleep 1; done
    if [ "$label" = "mmap-4096" ]; then
      # Memory only: same settings as run_all.sh "matched", read with the anon/file split.
      curl -sf http://127.0.0.1:11600/completion -d '{"prompt":"Hello","n_predict":8}' >/dev/null
      curl -sf http://127.0.0.1:11500/api/generate -d \
        "{\"model\":\"$omodel\",\"prompt\":\"Hello\",\"raw\":true,\"stream\":false,\"options\":{\"num_predict\":8}}" >/dev/null
    else
      python3 bench.py --label "$label" --model-name "$name" --ollama-model "$omodel" \
        --ollama-threads "$othreads" --runs "$RUNS" --out "$OUT"
    fi
    OL_PID=$(pgrep -n -f "lib/ollama/llama-server")
    mem_row "$label" "$name" ollama "$OL_PID" >> "$MEM"
    mem_row "$label" "$name" llama.cpp "$LS_PID" >> "$MEM"
    kill "$LS_PID"; wait "$LS_PID" 2>/dev/null || true
  done
done
"$OLLAMA" stop qwen15 >/dev/null 2>&1 || true
"$OLLAMA" stop llama32-3b >/dev/null 2>&1 || true
cat "$MEM"
