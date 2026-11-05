#!/usr/bin/env bash
# Builds a ROM with SGDK (Docker), runs it twice in mdsprites (limits off, to get
# the picture without losses, then limits on) and writes results/.
# Run checker/build.sh once first.
# SGDK_DIR=path/to/SGDK-checkout builds against that version's headers and library
# on top of the Docker image's compiler (the image tagged latest is SGDK 2.09).
# OUT=dir picks where results go. Arguments: screenshots as FRAME:rom/name.ppm.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
WORK="${WORK:-$HOME/.cache/mdsprites}"
SGDK_IMAGE="${SGDK_IMAGE:-ghcr.io/stephane-d/sgdk:latest}"
FRAMES="${FRAMES:-3600}"
OUT="${OUT:-$HERE/results}"
MOUNTS=()
if [ -n "${SGDK_DIR:-}" ]; then
  for d in inc lib res src; do MOUNTS+=(-v "$SGDK_DIR/$d:/sgdk/$d:ro"); done
  MOUNTS+=(-v "$SGDK_DIR/bin/rescomp.jar:/sgdk/bin/rescomp.jar:ro")
fi
mkdir -p "$OUT"
rm -rf "${WORK:?}/rom" && mkdir -p "$WORK/rom" && cp -r "$HERE/testrom/src" "$HERE/testrom/res" "$WORK/rom/"
docker run --rm --network none --user "$(id -u):$(id -g)" "${MOUNTS[@]}" -v "$WORK/rom:/src" "$SGDK_IMAGE" > "$WORK/rom/build.log" 2>&1 || { tail -30 "$WORK/rom/build.log"; exit 1; }
PH=$(awk '$3=="phase" {print $1}' "$WORK/rom/out/symbol.txt")
run() { docker run --rm --network none --user "$(id -u):$(id -g)" -v "$WORK:/w" -w /w gcc:14 ./mdsprites gpgx/genesis_plus_gx_libretro.so rom/out/rom.bin rom/out/symbol.txt "$FRAMES" "$@"; }
SHOTS=()
for s in "$@"; do SHOTS+=(--shot "$s"); done
run --nolimit --dump rom/truth.raw --csv rom/nolimit.csv --ram "$PH:phase" | tee "$OUT/report-nolimit.txt"
run --truth rom/truth.raw --csv rom/frames.csv --events rom/events.csv --ram "$PH:phase" "${SHOTS[@]}" | tee "$OUT/report.txt"
python3 "$HERE/tools/ppm2png.py" "$WORK"/rom/*.ppm --out "$OUT" 2>/dev/null || true
rm -f "${WORK:?}/rom/truth.raw"
cp "$WORK/rom/frames.csv" "$WORK/rom/events.csv" "$OUT/"
grep -E "VDP sprites|Sprite frame|sprites and" "$WORK/rom/build.log" > "$OUT/rescomp.txt" || true
sha256sum "$WORK/rom/out/rom.bin" | awk '{print $1"  rom.bin"}' > "$OUT/rom.sha256"
python3 "$HERE/tools/analyze.py" "$OUT/frames.csv" "$OUT/events.csv" | tee "$OUT/analysis.txt"
