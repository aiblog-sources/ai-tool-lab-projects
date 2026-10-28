#!/usr/bin/env bash
# Builds the test ROM with SGDK (Docker), runs mdprof on it headless and writes
# the results into results/. Run profiler/build.sh once first.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
WORK="${WORK:-$HOME/.cache/mdprof}"
SGDK_IMAGE="${SGDK_IMAGE:-ghcr.io/stephane-d/sgdk:latest}"
FRAMES="${FRAMES:-1500}"
OUT="$HERE/results"
mkdir -p "$OUT"
rm -rf "$WORK/rom" && mkdir -p "$WORK/rom" && cp -r "$HERE/testrom/src" "$WORK/rom/"
docker run --rm --network none --user "$(id -u):$(id -g)" -v "$WORK/rom:/src" "$SGDK_IMAGE" > "$WORK/rom/build.log" 2>&1
S="$WORK/rom/out/symbol.txt"
addr() { awk -v n="$1" '$3==n {print $1}' "$S"; }
VT_LO=$(printf '%x' $((0x$(addr vtimer) + 2)))   # low word of the u32 vtimer
docker run --rm --network none --user "$(id -u):$(id -g)" -v "$WORK:/w" -w /w gcc:14 \
  ./mdprof gpgx/genesis_plus_gx_libretro.so rom/out/rom.bin rom/out/symbol.txt "$FRAMES" --from 60 \
  --csv rom/frames.csv --json rom/summary.json \
  --ram "$(addr game_frames):game_frames" --ram "$VT_LO:vtimer_lo" --ram "$(addr sgdk_load):sgdk_load" \
  --ram "$(addr phase):phase" --ram "$(addr div_per_sprite):div" --ram "$(addr dma_bytes_queued):dma_q" \
  --shot 200:rom/phase-a.ppm --shot 450:rom/phase-b.ppm --shot 1200:rom/phase-d.ppm \
  | tee "$OUT/report.txt"
cp "$WORK/rom/frames.csv" "$WORK/rom/summary.json" "$OUT/"
sha256sum "$WORK/rom/out/rom.bin" | awk '{print $1"  rom.bin"}' > "$OUT/rom.sha256"
python3 "$HERE/tools/ppm2png.py" "$WORK"/rom/*.ppm --out "$OUT"
# emulator speed without the hook, for the overhead figure
docker run --rm --network none --user "$(id -u):$(id -g)" -v "$WORK:/w" -w /w gcc:14 \
  ./mdprof gpgx/genesis_plus_gx_libretro.so rom/out/rom.bin rom/out/symbol.txt "$FRAMES" --nohook | tee "$OUT/nohook.txt"
python3 "$HERE/tools/analyze.py" "$OUT/frames.csv" | tee "$OUT/analysis.txt"
