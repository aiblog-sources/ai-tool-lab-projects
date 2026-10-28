#!/usr/bin/env bash
# Profiles SGDK's own platformer sample (MIT, from the SGDK repo at tag v2.10,
# the newest sample that builds with the SGDK Docker image used here) under a
# scripted pad: walk right 40 frames, left 40 frames, jump in each, repeated.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
WORK="${WORK:-$HOME/.cache/mdprof}"
SGDK_IMAGE="${SGDK_IMAGE:-ghcr.io/stephane-d/sgdk:latest}"
OUT="$HERE/results/platformer"
mkdir -p "$OUT"
if [ ! -d "$WORK/sgdk-v2.10" ]; then
  git clone -q --depth 1 --branch v2.10 https://github.com/Stephane-D/SGDK "$WORK/sgdk-v2.10"
fi
rm -rf "$WORK/plat" && cp -r "$WORK/sgdk-v2.10/sample/game/platformer" "$WORK/plat" && rm -rf "$WORK/plat/out"
docker run --rm --network none --user "$(id -u):$(id -g)" -v "$WORK/plat:/src" "$SGDK_IMAGE" > "$WORK/plat/build.log" 2>&1
IN=()
for f in $(seq 60 80 1100); do
  IN+=(--input "$f-$((f+39)):R" --input "$((f+40))-$((f+79)):L" --input "$((f+20))-$((f+30)):C")
done
docker run --rm --network none --user "$(id -u):$(id -g)" -v "$WORK:/w" -w /w gcc:14 \
  ./mdprof gpgx/genesis_plus_gx_libretro.so plat/out/rom.bin plat/out/symbol.txt 1200 --from 60 "${IN[@]}" \
  --csv plat/frames.csv --json plat/summary.json | tee "$OUT/report.txt"
cp "$WORK/plat/frames.csv" "$WORK/plat/summary.json" "$OUT/"
