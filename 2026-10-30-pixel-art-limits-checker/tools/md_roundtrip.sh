#!/usr/bin/env bash
# Mega Drive round trip for one background PNG: pixcheck --export -> SGDK rescomp + build (Docker) -> mdprof
# screenshot (Genesis Plus GX, headless) -> pixel compare with the source snapped to the 9-bit palette.
# Usage: tools/md_roundtrip.sh image.png outdir     (needs mdprof built, see ../2026-10-28-sgdk-frame-profiler)
set -euo pipefail
HERE="$(cd "$(dirname "$0")/.." && pwd)"
IMG="$1"; OUT="$2"
MDPROF="${MDPROF:-$HOME/.cache/mdprof}"          # folder with mdprof and gpgx/genesis_plus_gx_libretro.so
WORK="${WORK:-$HOME/.cache/pixcheck}/rom-$(basename "$OUT")"
SGDK_IMAGE="${SGDK_IMAGE:-ghcr.io/stephane-d/sgdk:latest}"
mkdir -p "$OUT"
rm -rf "$WORK" && mkdir -p "$WORK" && cp -r "$HERE/mdrom/src" "$HERE/mdrom/res" "$WORK/"
python3 "$HERE/pixcheck/pixcheck.py" md "$IMG" --export "$WORK/res/bg.png" --json "$OUT/pixcheck.json" > "$OUT/pixcheck.txt" || { cat "$OUT/pixcheck.txt"; exit 1; }
sed -i "s#$HERE/##g" "$OUT/pixcheck.txt" "$OUT/pixcheck.json"     # keep reports free of local paths
cp "$WORK/res/bg.png" "$OUT/export.png"
docker run --rm --network none --user "$(id -u):$(id -g)" -v "$WORK:/src" "$SGDK_IMAGE" > "$OUT/build.log" 2>&1 || { tail -20 "$OUT/build.log"; exit 1; }
S="$WORK/out/symbol.txt"
addr() { awk -v n="$1" '$3==n {print $1}' "$S"; }
docker run --rm --network none --user "$(id -u):$(id -g)" -v "$MDPROF:/m:ro" -v "$WORK:/r" -w /m gcc:14 \
  ./mdprof gpgx/genesis_plus_gx_libretro.so /r/out/rom.bin /r/out/symbol.txt 150 \
  --ram "$(addr user_tiles):user_tiles" --ram "$(addr shown):shown" --csv /r/frames.csv --shot 140:/r/shot.ppm > "$OUT/mdprof.txt"
python3 "$HERE/tools/compare_shot.py" "$IMG" "$WORK/shot.ppm" "$OUT/shot.png" | tee "$OUT/compare.txt"
tail -1 "$WORK/frames.csv" > "$OUT/last-frame.csv"; head -1 "$WORK/frames.csv" >> "$OUT/last-frame.csv"
