#!/usr/bin/env bash
# Runs json_bench.py against every model in models.tsv, one llama-server at a time.
# 3 slots (one per mode) of 2,048 tokens, 2 threads, no host-RAM prompt cache, 6 GB memory cap.
# ONLY="<model name>" runs one model; LIMIT=<n> passes --limit (smoke test).
set -euo pipefail
cd "$(dirname "$0")"
export LC_ALL=C
BIN=$PWD/vendor/llama.cpp/build/bin
ONLY=${ONLY:-}
LIMIT=${LIMIT:-0}
OUT=${OUT:-results/raw.jsonl}
PORT=11710
mkdir -p results/logs
[ -f results/server.tsv ] || printf "model\tstatus\tbackground_cpu_pct\tpeak_rss_mib\tminutes\n" > results/server.tsv

cpu_busy() {  # percent of all CPU time busy over 5 s, before the model loads
  read -r _ u n s i w q sq _ < /proc/stat; sleep 5
  read -r _ u2 n2 s2 i2 w2 q2 sq2 _ < /proc/stat
  local busy=$(( (u2+n2+s2+q2+sq2) - (u+n+s+q+sq) )) idle=$(( (i2+w2) - (i+w) ))
  echo $(( 100 * busy / (busy + idle) ))
}

tail -n +2 models.tsv | while IFS=$'\t' read -r name file _; do
  [ -n "$ONLY" ] && [ "$name" != "$ONLY" ] && continue
  slug=$(echo "$name" | tr 'A-Z .' 'a-z--')
  if curl -sf "http://127.0.0.1:$PORT/health" >/dev/null; then echo "port $PORT already in use" >&2; exit 1; fi
  bg=$(cpu_busy)
  t0=$(date +%s)
  systemd-run --user --scope --quiet -p MemoryMax=6G -p MemorySwapMax=0 \
    env LD_LIBRARY_PATH="$BIN" "$BIN/llama-server" -m "models/$file" --port $PORT -c 6144 -np 3 \
    -t 2 --reasoning off --cache-ram 0 > "results/logs/server-$slug.log" 2>&1 &
  wrapper=$!
  status=ok
  until curl -sf "http://127.0.0.1:$PORT/health" >/dev/null; do
    if ! kill -0 "$wrapper" 2>/dev/null; then status=died; break; fi
    sleep 0.5
  done
  peak=
  if [ "$status" = ok ]; then
    pid=$(pgrep -n -x llama-server)
    python3 json_bench.py --base "http://127.0.0.1:$PORT" --model "$name" --out "$OUT" --limit "$LIMIT" \
      || status=bench-error
    if [ -r "/proc/$pid/status" ]; then
      peak=$(awk '/VmHWM/ {printf "%.0f", $2/1024}' "/proc/$pid/status")
      kill "$pid"
    else
      status=killed
    fi
    wait "$wrapper" 2>/dev/null || true
    while curl -sf "http://127.0.0.1:$PORT/health" >/dev/null; do sleep 0.5; done
  fi
  [ "$LIMIT" = 0 ] && printf "%s\t%s\t%s\t%s\t%.1f\n" "$name" "$status" "$bg" "$peak" \
    "$(echo "($(date +%s) - $t0) / 60" | bc -l)" | tee -a results/server.tsv
done
sed -i -e "s#$PWD/##g" -e "s#$HOME#~#g" results/logs/*.log
