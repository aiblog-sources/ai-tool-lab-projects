#!/usr/bin/env bash
# Interleaved speed pass: every round runs llama-bench once per model (pp256 and tg64), so a burst
# of background load hits all models alike instead of skewing one. Records the background CPU
# share before each round. Same settings as run_all.sh (2 threads, no mmap, 6 GB cap).
set -euo pipefail
cd "$(dirname "$0")"
export LC_ALL=C
BIN=$PWD/vendor/llama.cpp/build/bin
ROUNDS=${ROUNDS:-3}
OUT=results/speed-rounds.jsonl
BG=results/speed-rounds-background.tsv
[ -f "$BG" ] || printf "round\tbusy_pct_before\tstarted_utc\n" > "$BG"
busy() {
  read -r _ a b c d e f g _ < /proc/stat; sleep 5; read -r _ A B C D E F G _ < /proc/stat
  echo "scale=1; 100 * (($A+$B+$C+$F+$G) - ($a+$b+$c+$f+$g)) / (($A+$B+$C+$D+$E+$F+$G) - ($a+$b+$c+$d+$e+$f+$g))" | bc
}
for r in $(seq 1 "$ROUNDS"); do
  printf "%s\t%s\t%s\n" "$r" "$(busy)" "$(date -u +%FT%TZ)" >> "$BG"
  tail -n +2 models.tsv | while IFS=$'\t' read -r name file _; do
    systemd-run --user --scope --quiet -p MemoryMax=6G -p MemorySwapMax=0 \
      env LD_LIBRARY_PATH="$BIN" "$BIN/llama-bench" -m "models/$file" -t 2 -p 256 -n 64 -r 1 -lm none -o jsonl \
      2>/dev/null | sed "s/^{/{\"round\": $r, /" >> "$OUT"
    echo "round $r $name done $(date +%T)"
  done
done
