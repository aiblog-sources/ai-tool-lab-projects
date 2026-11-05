#!/usr/bin/env bash
# SGDK's own samples (MIT, tag v2.11) under a scripted pad.
# SGDK_DIR must point at a v2.11 checkout.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
export WORK="${WORK:-$HOME/.cache/mdsprites}"
S="${SGDK_DIR:-$WORK/sgdk-v2.11}"
IMG="${SGDK_IMAGE:-ghcr.io/stephane-d/sgdk:latest}"
OUT="$HERE/results-samples"
mkdir -p "$OUT"
MOUNTS=(); for d in inc lib res src; do MOUNTS+=(-v "$S/$d:/sgdk/$d:ro"); done; MOUNTS+=(-v "$S/bin/rescomp.jar:/sgdk/bin/rescomp.jar:ro")
IN=(); for f in $(seq 120 200 2300); do IN+=(--input "$f-$((f+139)):R" --input "$((f+140))-$((f+199)):L" --input "$((f+30))-$((f+40)):C" --input "$((f+90))-$((f+100)):A"); done
for s in game/sonic game/platformer; do
  n=$(basename "$s"); [ -d "$S/sample/$s" ] || { echo "no sample $s"; continue; }
  rm -rf "${WORK:?}/s-$n"; cp -r "$S/sample/$s" "$WORK/s-$n"; rm -rf "${WORK:?}/s-$n/out"
  if ! docker run --rm --network none --user "$(id -u):$(id -g)" "${MOUNTS[@]}" -v "$WORK/s-$n:/src" "$IMG" > "$WORK/s-$n/build.log" 2>&1; then echo "$n: build failed"; tail -3 "$WORK/s-$n/build.log"; continue; fi
  echo "== $n"
  "$HERE/tools/rom_check.sh" "s-$n" 2400 "${IN[@]}" --shot "1200:s-$n/shot.ppm" | tee "$OUT/$n-report.txt"
  python3 "$HERE/tools/explain.py" "$WORK/s-$n/frames.csv" "$WORK/s-$n/events.csv" --from 60 | tee "$OUT/$n-explain.txt"
  cp "$WORK/s-$n/frames.csv" "$OUT/$n-frames.csv"
  python3 "$HERE/tools/ppm2png.py" "$WORK/s-$n/shot.ppm" --out "$OUT" && mv "$OUT/shot.png" "$OUT/$n-shot.png"
done
