#!/usr/bin/env bash
# Builds the test ROM with SGDK (Docker) and runs it in mddma on an NTSC and a
# PAL console, writing results/. Run checker/build.sh once first.
# SGDK_DIR=path/to/SGDK-checkout builds against that version's headers and
# library on top of the Docker image's compiler (the image tagged latest is
# SGDK 2.09; the results here use a v2.11 checkout; 2.09 gave the same table).
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
WORK="${WORK:-$HOME/.cache/mddma}"
SGDK_IMAGE="${SGDK_IMAGE:-ghcr.io/stephane-d/sgdk:latest}"
FRAMES="${FRAMES:-2700}"
OUT="${OUT:-$HERE/results}"
MOUNTS=()
if [ -n "${SGDK_DIR:-}" ]; then
  for d in inc lib res src; do MOUNTS+=(-v "$SGDK_DIR/$d:/sgdk/$d:ro"); done
  MOUNTS+=(-v "$SGDK_DIR/bin/rescomp.jar:/sgdk/bin/rescomp.jar:ro")
fi
mkdir -p "$OUT" && rm -f "$OUT"/*.png
rm -rf "${WORK:?}/rom" && mkdir -p "$WORK/rom" && cp -r "$HERE/testrom/src" "$WORK/rom/"
# ONE_ENTRY=1: each phase queues its tiles as one entry instead of 1 KB entries
if [ "${ONE_ENTRY:-0}" = 1 ]; then sed -i 's/^#define ONE_ENTRY 0/#define ONE_ENTRY 1/' "$WORK/rom/src/main.c"; fi
docker run --rm --network none --user "$(id -u):$(id -g)" "${MOUNTS[@]}" -v "$WORK/rom:/src" "$SGDK_IMAGE" > "$WORK/rom/build.log" 2>&1 || { tail -30 "$WORK/rom/build.log"; exit 1; }
SYM="$WORK/rom/out/symbol.txt"
PH=$(awk '$3=="phase" {print $1}' "$SYM")
SX=$(awk '$3=="spr_x" {print $1}' "$SYM")
run() { docker run --rm --network none --user "$(id -u):$(id -g)" -v "$WORK:/w" -w /w gcc:14 ./mddma gpgx/genesis_plus_gx_libretro.so rom/out/rom.bin rom/out/symbol.txt "$FRAMES" --from 30 --ram "$PH:phase" --ram "$SX:spr_x" "$@"; }
run --csv rom/ntsc.csv | tee "$OUT/report-ntsc.txt"
# second NTSC pass for screenshots: the middle game loop of some phases, picked from the first CSV
# (a phase that drops frames takes more than PHASE_FRAMES video frames)
read -r -a SHOTS <<< "$(python3 "$HERE/tools/pick_shots.py" "$WORK/rom/ntsc.csv" rom/ntsc 1,5,6,8,12,15,16,20)"
run "${SHOTS[@]}" > /dev/null
run --pal --csv rom/pal.csv | tee "$OUT/report-pal.txt"
python3 "$HERE/tools/ppm2png.py" "$WORK"/rom/*.ppm --out "$OUT" 2>/dev/null || true
cp "$WORK/rom/ntsc.csv" "$WORK/rom/pal.csv" "$OUT/"
sha256sum "$WORK/rom/out/rom.bin" | awk '{print $1"  rom.bin"}' > "$OUT/rom.sha256"
python3 "$HERE/tools/analyze.py" "$OUT/ntsc.csv" "$OUT/pal.csv" | tee "$OUT/analysis.txt"
