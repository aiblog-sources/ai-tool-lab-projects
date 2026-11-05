#!/usr/bin/env bash
# Does the same source give the same sprite losses in every build? Builds the test ROM
# N times with SGDK's rescomp and counts the disc-phase losses of each build.
set -euo pipefail
HERE="$(cd "$(dirname "$0")/.." && pwd)"
export WORK="${WORK:-$HOME/.cache/mdsprites}"
N="${1:-5}"; IMG="${SGDK_IMAGE:-ghcr.io/stephane-d/sgdk:latest}"
MOUNTS=()
if [ -n "${SGDK_DIR:-}" ]; then for d in inc lib res src; do MOUNTS+=(-v "$SGDK_DIR/$d:/sgdk/$d:ro"); done; MOUNTS+=(-v "$SGDK_DIR/bin/rescomp.jar:/sgdk/bin/rescomp.jar:ro"); fi
for i in $(seq 1 "$N"); do
  rm -rf "${WORK:?}/reb"; mkdir -p "$WORK/reb"; cp -r "$HERE/testrom/src" "$HERE/testrom/res" "$WORK/reb/"; if [ -n "${OPT_LEVEL:-}" ]; then sed -i "s/ FAST\$/ $OPT_LEVEL/" "$WORK/reb/res/sprites.res"; fi
  docker run --rm --network none --user "$(id -u):$(id -g)" "${MOUNTS[@]}" -v "$WORK/reb:/src" "$IMG" > "$WORK/reb/build.log" 2>&1
  PH=$(awk '$3=="phase" {print $1}' "$WORK/reb/out/symbol.txt")
  "$HERE/tools/rom_check.sh" reb 1600 --ram "$PH:phase" > /dev/null
  python3 - "$WORK/reb/frames.csv" "$(sha256sum "$WORK/reb/out/rom.bin" | cut -c1-12)" "$i" <<'PY'
import csv, statistics, sys
rows = list(csv.DictReader(open(sys.argv[1])))
out = []
for ph in (10, 11, 12):
    rs = [r for r in rows if int(r["phase"]) == ph][5:]
    out.append(f"phase {ph}: lost {statistics.median(int(r['sprites_lost']) for r in rs):g}, {statistics.median(int(r['real_pixels']) for r in rs):g} px")
print(f"build {sys.argv[3]} {sys.argv[2]}  " + "; ".join(out))
PY
done
