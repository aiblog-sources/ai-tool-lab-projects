#!/usr/bin/env bash
# What happens to llama.cpp when another program already keeps one logical CPU busy.
# Runs llama-bench at 2, 3 and 4 threads on the smallest model, recording background load first.
set -euo pipefail
cd "$(dirname "$0")"
export LC_ALL=C
BIN=$PWD/vendor/llama.cpp/build/bin
OUT=results/thread-check.md
read -r _ a b c d e f g _ < /proc/stat; sleep 5; read -r _ A B C D E F G _ < /proc/stat
bg=$(echo "scale=1; 100 * (($A+$B+$C+$F+$G) - ($a+$b+$c+$f+$g)) / (($A+$B+$C+$D+$E+$F+$G) - ($a+$b+$c+$d+$e+$f+$g))" | bc)
{
  echo "Background CPU busy before the test: $bg% of 4 logical CPUs ($(date -u +%FT%TZ))"
  echo
  env LD_LIBRARY_PATH="$BIN" "$BIN/llama-bench" -m models/Qwen_Qwen3.5-0.8B-Q4_K_M.gguf \
    -t 2,3,4 -p 128 -n 64 -r 3 -lm none 2>/dev/null
} | tee -a "$OUT"
