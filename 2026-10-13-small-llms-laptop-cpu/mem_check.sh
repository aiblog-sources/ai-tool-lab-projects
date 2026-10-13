#!/usr/bin/env bash
# Follow-up to run_all.sh: memory with llama-server's host-RAM prompt cache turned off (--cache-ram 0).
# For each model: RSS right after load, then 4 chat requests (serve_bench.py --latency-only), then peak RSS.
# Pass FULL="<model name>" to also run the 20 questions for that model.
set -euo pipefail
cd "$(dirname "$0")"
export LC_ALL=C
BIN=$PWD/vendor/llama.cpp/build/bin
FULL=${FULL:-}
ONLY=${ONLY:-}
OUT=results/memory-no-prompt-cache.tsv
[ -f "$OUT" ] || printf "model\tstatus\tload_s\trss_after_load_mib\tpeak_rss_mib\n" > "$OUT"
tail -n +2 models.tsv | while IFS=$'\t' read -r name file _; do
  [ -n "$ONLY" ] && [ "$name" != "$ONLY" ] && continue
  slug=$(echo "$name" | tr 'A-Z .' 'a-z--')
  if curl -sf http://127.0.0.1:11700/health >/dev/null; then echo "port 11700 already in use" >&2; exit 1; fi
  t0=$(date +%s.%N)
  systemd-run --user --scope --quiet -p MemoryMax=6G -p MemorySwapMax=0 \
    env LD_LIBRARY_PATH="$BIN" "$BIN/llama-server" -m "models/$file" --port 11700 -c 4096 -np 1 \
    -t 2 --load-mode none --reasoning off --cache-ram 0 > "results/logs/server-nocache-$slug.log" 2>&1 &
  wrapper=$!
  status=ok
  until curl -sf http://127.0.0.1:11700/health >/dev/null; do
    if ! kill -0 "$wrapper" 2>/dev/null; then status=died; break; fi
    sleep 0.2
  done
  load=$(echo "$(date +%s.%N) - $t0" | bc)
  if [ "$status" = ok ]; then
    pid=$(pgrep -n -x llama-server)
    after=$(awk '/VmRSS/ {printf "%.0f", $2/1024}' "/proc/$pid/status")
    extra=--latency-only
    [ "$name" = "$FULL" ] && extra=
    # shellcheck disable=SC2086
    python3 serve_bench.py --model "$name" --tag no-prompt-cache --out results/serve-no-prompt-cache.jsonl $extra \
      || status=bench-error
    if [ -r "/proc/$pid/status" ]; then
      peak=$(awk '/VmHWM/ {printf "%.0f", $2/1024}' "/proc/$pid/status")
      kill "$pid"
    else
      peak=; status=killed
    fi
    wait "$wrapper" 2>/dev/null || true
    while curl -sf http://127.0.0.1:11700/health >/dev/null; do sleep 0.5; done
  else
    after=; peak=
  fi
  printf "%s\t%s\t%.1f\t%s\t%s\n" "$name" "$status" "$load" "$after" "$peak" | tee -a "$OUT"
done
sed -i -e "s#$PWD/##g" -e "s#$HOME#~#g" results/logs/*.log
