#!/usr/bin/env bash
# Key order test on upstream llama-server (llama.cpp b11554): per model, start llama-server (2 slots, 2 threads,
# 6 GB memory cap, same flags as the 2026-10-22 run), run order_probe.py and order_bench.py, stop it.
# Run after run.sh, not at the same time (both use the CPU). ONLY, LIMIT and REPS work as in run.sh.
set -euo pipefail
cd "$(dirname "$0")"
export LC_ALL=C
BIN=$PWD/vendor/llama.cpp/build/bin
ONLY=${ONLY:-}
LIMIT=${LIMIT:-0}
REPS=${REPS:-2}
OUT=${OUT:-results/raw.jsonl}
PORT=11711
mkdir -p results/logs
cleanup() { pkill -f "$BIN/llama-server" 2>/dev/null || true; }
trap cleanup EXIT

tail -n +2 models.tsv | while IFS=$'\t' read -r name file _; do
  [ -n "$ONLY" ] && [ "$name" != "$ONLY" ] && continue
  slug=$(echo "$name" | tr 'A-Z .' 'a-z--')
  if curl -s "http://127.0.0.1:$PORT/" >/dev/null; then echo "port $PORT already in use" >&2; exit 1; fi
  t0=$(date +%s)
  systemd-run --user --scope --quiet -p MemoryMax=6G -p MemorySwapMax=0 \
    env LD_LIBRARY_PATH="$BIN" "$BIN/llama-server" -m "models/$file" --port $PORT -c 8192 -np 2 -t 2 \
    --reasoning off --cache-ram 0 > "results/logs/llama-server-$slug.log" 2>&1 &
  wrapper=$!
  until curl -sf "http://127.0.0.1:$PORT/health" >/dev/null; do kill -0 "$wrapper" 2>/dev/null || exit 1; sleep 0.5; done
  python3 order_probe.py --llama-server "http://127.0.0.1:$PORT"
  python3 order_bench.py --base "http://127.0.0.1:$PORT" --model "$name" --out "$OUT" --limit "$LIMIT" --reps "$REPS"
  kill "$(pgrep -n -x llama-server)"
  wait "$wrapper" 2>/dev/null || true
  while curl -s "http://127.0.0.1:$PORT/" >/dev/null; do sleep 0.5; done
  [ "$LIMIT" = 0 ] && printf "llama-server b11554\t%s\tok\t-\t-\t%.1f\n" "$name" \
    "$(echo "($(date +%s) - $t0) / 60" | bc -l)" | tee -a results/server.tsv
done
sed -i -e "s#$PWD/##g" -e "s#$HOME#~#g" results/logs/llama-server-*.log 2>/dev/null || true
echo DONE
