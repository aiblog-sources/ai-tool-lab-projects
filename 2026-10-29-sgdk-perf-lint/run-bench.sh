#!/usr/bin/env bash
# Builds the cost-table ROM (bench/) with SGDK in Docker, profiles it headless
# with mdprof and writes results/bench-*. Needs mdprof built first:
#   WORK=~/.cache/mdprof ../2026-10-28-sgdk-frame-profiler/profiler/build.sh
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
WORK="${WORK:-$HOME/.cache/mdprof}"
SGDK_IMAGE="${SGDK_IMAGE:-ghcr.io/stephane-d/sgdk:latest}"
OUT="$HERE/results"
mkdir -p "$OUT"
rm -rf "$WORK/bench" && mkdir -p "$WORK/bench" && cp -r "$HERE/bench/src" "$HERE/bench/res" "$WORK/bench/"
docker run --rm --network none --user "$(id -u):$(id -g)" -v "$WORK/bench:/src" "$SGDK_IMAGE" > "$WORK/bench/build.log" 2>&1 \
  || { tail -30 "$WORK/bench/build.log"; exit 1; }
S="$WORK/bench/out/symbol.txt"
addr() { awk -v n="$1" '$3==n {print $1}' "$S"; }
docker run --rm --network none --user "$(id -u):$(id -g)" -v "$WORK:/w" -w /w gcc:14 \
  ./mdprof gpgx/genesis_plus_gx_libretro.so bench/out/rom.bin bench/out/symbol.txt 4000 --from 60 \
  --csv bench/frames.csv --ram "$(addr phase):phase" --ram "$(addr game_frames):game_frames" \
  > "$OUT/bench-report.txt"
cp "$WORK/bench/frames.csv" "$OUT/bench-frames.csv"
sha256sum "$WORK/bench/out/rom.bin" | awk '{print $1"  bench rom.bin"}' > "$OUT/bench-rom.sha256"
# disassembly of the case functions, to see what GCC emitted for each one
docker run --rm --network none --user "$(id -u):$(id -g)" -v "$WORK/bench:/src" --entrypoint sh "$SGDK_IMAGE" \
  -c 'm68k-elf-objdump -d --no-show-raw-insn out/rom.out' > "$WORK/bench/rom.dis" 2>/dev/null || true
python3 "$HERE/tools/costs.py" "$OUT/bench-frames.csv" "$WORK/bench/rom.dis" | tee "$OUT/costs.txt"
