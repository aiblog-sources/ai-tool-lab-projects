#!/usr/bin/env bash
# For every model in models.tsv:
#   1. llama-bench: prompt processing (512 tokens) and generation (128 tokens), 3 repetitions.
#   2. llama-server: load time, peak memory, first-token latency and the 20-question sanity check.
# Everything runs inside a systemd user scope capped at 6 GB, with the same settings for every model:
# 2 threads (the physical cores, see thread_check.sh), 4,096-token context, 1 slot, weights read into RAM (no mmap), thinking off.
set -euo pipefail
cd "$(dirname "$0")"
export LC_ALL=C
BIN=$PWD/vendor/llama.cpp/build/bin
THREADS=${THREADS:-2}
ONLY=${ONLY:-}
mkdir -p results/logs
MEM=results/server.tsv
[ -f "$MEM" ] || printf "model\tstatus\tload_s\tpeak_rss_mib\trss_anon_mib\n" > "$MEM"
BG=results/background-cpu.tsv
[ -f "$BG" ] || printf "model\tbusy_pct_before\n" > "$BG"
# CPU busy share over 5 s with nothing of ours running: other programs on the machine.
busy() {
  read -r _ a b c d e f g _ < /proc/stat; sleep 5; read -r _ A B C D E F G _ < /proc/stat
  echo "scale=1; 100 * (($A+$B+$C+$F+$G) - ($a+$b+$c+$f+$g)) / (($A+$B+$C+$D+$E+$F+$G) - ($a+$b+$c+$d+$e+$f+$g))" | bc
}
cap() { systemd-run --user --scope --quiet -p MemoryMax=6G -p MemorySwapMax=0 "$@"; }

tail -n +2 models.tsv | while IFS=$'\t' read -r name file _; do
  [ -n "$ONLY" ] && [ "$name" != "$ONLY" ] && continue
  slug=$(echo "$name" | tr 'A-Z .' 'a-z--')
  gguf=models/$file
  bg=$(busy)
  printf "%s\t%s\n" "$name" "$bg" >> "$BG"
  echo "=== $name $(date +%T) (background CPU busy: $bg%)"

  cap env LD_LIBRARY_PATH="$BIN" "$BIN/llama-bench" -m "$gguf" -t "$THREADS" -p 512 -n 128 -r 3 \
    -lm none -o jsonl >> results/llama-bench.jsonl 2> "results/logs/bench-$slug.log" \
    || echo "llama-bench failed for $name (exit $?)" | tee -a "results/logs/bench-$slug.log"

  if curl -sf http://127.0.0.1:11700/health >/dev/null; then echo "port 11700 already in use" >&2; exit 1; fi
  t0=$(date +%s.%N)
  cap env LD_LIBRARY_PATH="$BIN" "$BIN/llama-server" -m "$gguf" --port 11700 -c 4096 -np 1 \
    -t "$THREADS" --load-mode none --reasoning off > "results/logs/server-$slug.log" 2>&1 &
  wrapper=$!
  status=ok
  pid=
  until curl -sf http://127.0.0.1:11700/health >/dev/null; do
    if ! kill -0 "$wrapper" 2>/dev/null; then status=died; break; fi
    sleep 0.2
  done
  # $! is the subshell around systemd-run; the server itself is the newest llama-server process.
  [ "$status" = ok ] && pid=$(pgrep -n -x llama-server)
  load=$(echo "$(date +%s.%N) - $t0" | bc)
  if [ "$status" = ok ]; then
    python3 serve_bench.py --model "$name" --out results/serve.jsonl || status=bench-error
    awk -v m="$name" -v s="$status" -v l="$load" '
      /VmHWM/ {h=$2} /RssAnon/ {a=$2}
      END {printf "%s\t%s\t%.1f\t%.0f\t%.0f\n", m, s, l, h/1024, a/1024}' "/proc/$pid/status" >> "$MEM"
    kill "$pid"; wait "$wrapper" 2>/dev/null || true
    while curl -sf http://127.0.0.1:11700/health >/dev/null; do sleep 0.5; done
  else
    printf "%s\t%s\t%.1f\t\t\n" "$name" "$status" "$load" >> "$MEM"
  fi
done
# Scrub machine-specific paths from the logs before they are committed.
sed -i -e "s#$PWD/##g" -e "s#$HOME#~#g" results/logs/*.log
cat "$MEM"
