#!/usr/bin/env bash
# Run bench.py on one server and one model: SERVER=llama-server or SERVER=ollama, ONLY="<model name from models.tsv>".
# Same server settings as the 2026-11-12 structured outputs run: 2 slots, 2 threads, 6 GB memory cap.
# Resumable: requests already in results/raw.jsonl are skipped. LIMIT = number of GSM8K test problems (default 100).
set -euo pipefail
cd "$(dirname "$0")"
export LC_ALL=C
SERVER=${SERVER:?SERVER=llama-server or ollama}
ONLY=${ONLY:?ONLY=model name}
LIMIT=${LIMIT:-100}
OUT=${OUT:-results/raw.jsonl}
LS_CONDS=${LS_CONDS:-free,answer_only,reasoning_first,answer_first,prefixed}
OL_CONDS=${OL_CONDS:-answer_only,reasoning_first,answer_first,prefixed}
mkdir -p results/logs
file=$(awk -F'\t' -v n="$ONLY" '$1 == n {print $2}' models.tsv)
[ -n "$file" ] || { echo "unknown model $ONLY" >&2; exit 1; }
slug=$(echo "$ONLY" | tr 'A-Z .' 'a-z--'); omodel=$(echo "$slug" | tr -d '-')
cpu_busy() {  # percent of all CPU time busy over 5 s
  read -r _ u n s i w q sq _ < /proc/stat; sleep 5
  read -r _ u2 n2 s2 i2 w2 q2 sq2 _ < /proc/stat
  local busy=$(( (u2+n2+s2+q2+sq2) - (u+n+s+q+sq) )) idle=$(( (i2+w2) - (i+w) ))
  echo $(( 100 * busy / (busy + idle) ))
}
[ -f results/server.tsv ] || printf "server\tmodel\tbackground_cpu_pct\tminutes\n" > results/server.tsv
bg=$(cpu_busy); t0=$(date +%s)
if [ "$SERVER" = llama-server ]; then
  BIN=$PWD/vendor/llama.cpp/build/bin; PORT=11711; label=llama-server-b11554; served=x; conds=$LS_CONDS
  trap 'pkill -f "$BIN/llama-server" 2>/dev/null || true' EXIT
  systemd-run --user --scope --quiet -p MemoryMax=6G -p MemorySwapMax=0 \
    env LD_LIBRARY_PATH="$BIN" "$BIN/llama-server" -m "models/$file" --port $PORT -c 8192 -np 2 -t 2 \
    --reasoning off --cache-ram 0 > "results/logs/llama-server-$slug.log" 2>&1 &
  until curl -sf "http://127.0.0.1:$PORT/health" >/dev/null; do sleep 0.5; done
else
  OLLAMA=$PWD/vendor/ollama-0.40.16/bin/ollama; OHOME=$PWD/vendor/home; PORT=11500; label=ollama-0.40.16
  served=$omodel; conds=$OL_CONDS
  trap 'pkill -f "$OLLAMA serve" 2>/dev/null || true' EXIT
  systemd-run --user --scope --quiet -p MemoryMax=6G -p MemorySwapMax=0 \
    --setenv=HOME="$OHOME" --setenv=OLLAMA_MODELS="$OHOME/models" --setenv=OLLAMA_HOST=127.0.0.1:$PORT \
    --setenv=OLLAMA_NUM_PARALLEL=2 --setenv=OLLAMA_CONTEXT_LENGTH=8192 "$OLLAMA" serve \
    > "results/logs/ollama-$slug.log" 2>&1 &
  until curl -sf "http://127.0.0.1:$PORT/api/version" >/dev/null; do sleep 0.5; done
  OLLAMA_HOST=127.0.0.1:$PORT "$OLLAMA" list | grep -q "^$omodel" || {
    echo "FROM $PWD/models/$file" > "$OHOME/Modelfile-$omodel"
    OLLAMA_HOST=127.0.0.1:$PORT "$OLLAMA" create "$omodel" -f "$OHOME/Modelfile-$omodel" >/dev/null 2>&1; }
fi
python3 bench.py --base "http://127.0.0.1:$PORT" --server "$label" --model "$ONLY" --served-model "$served" \
  --conds "$conds" --out "$OUT" --limit "$LIMIT"
printf "%s\t%s\t%s\t%.1f\n" "$label" "$ONLY" "$bg" "$(echo "($(date +%s) - $t0) / 60" | bc -l)" | tee -a results/server.tsv
