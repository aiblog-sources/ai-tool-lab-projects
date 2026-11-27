#!/usr/bin/env bash
# Full-image check: convert two corpus images with trunc and lab, then run the 2026-10-30 pixcheck round trip
# (pack palettes, SGDK rescomp + build in Docker, Genesis Plus GX screenshot, compare per pixel). MIT license.
set -euo pipefail
HERE="$(cd "$(dirname "$0")/.." && pwd)"
TMP="$(mktemp -d)"
for img in limits-landscape-snes-sonnet-5-5 naive-dungeon-snes-opus-5-5; do
  for m in trunc lab; do
    python3 "$HERE/mdcolor.py" convert "$HERE/corpus/$img.png" "$TMP/$img-$m.png" --method "$m" --snap-snes
    bash "$HERE/../2026-10-30-pixel-art-limits-checker/tools/md_roundtrip.sh" "$TMP/$img-$m.png" "$HERE/results/roundtrip/$img-$m"
    rm -f "$HERE/results/roundtrip/$img-$m/"{build.log,mdprof.txt,last-frame.csv}
    sed -i "s#$TMP/##g" "$HERE/results/roundtrip/$img-$m/"pixcheck.*
  done
done
rm -rf "$TMP"
