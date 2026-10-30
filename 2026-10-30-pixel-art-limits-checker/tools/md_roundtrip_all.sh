#!/usr/bin/env bash
# Round trip every Mega Drive background of the AI test that pixcheck passes (first passing stage of each job).
set -uo pipefail
HERE="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$HERE/results/md-roundtrip"
mkdir -p "$OUT"
for job in "$HERE"/ai-test/*-landscape-md-* "$HERE"/ai-test/*-dungeon-md-*; do
  [ -d "$job" ] || continue
  for st in v0 check1 check2 plain1; do
    if [ -f "$job/$st/art.png" ] && grep -q ": PASS" "$job/$st/pixcheck.txt"; then
      name="$(basename "$job")-$st"
      if "$HERE/tools/md_roundtrip.sh" "$job/$st/art.png" "$OUT/$name" > /dev/null 2>&1; then
        echo "$name $(grep 'pixels that differ' "$OUT/$name/compare.txt") user_tiles=$(tail -2 "$OUT/$name/last-frame.csv" | head -1 | awk -F, '{print $(NF-1)}')"
      else
        echo "$name round trip FAILED: $(tail -3 "$OUT/$name/build.log" 2>/dev/null | tr '\n' ' ')"
      fi
      break
    fi
  done
done
