#!/usr/bin/env bash
# Feed each AI Mega Drive background PNG to SGDK rescomp as-is (what you get if you drop the file into res/),
# and record the first error line. Writes results/rescomp-raw.txt.
set -uo pipefail
HERE="$(cd "$(dirname "$0")/.." && pwd)"
SGDK_IMAGE="${SGDK_IMAGE:-ghcr.io/stephane-d/sgdk:latest}"
WORK="${WORK:-$HOME/.cache/pixcheck}/rescomp-raw"
for png in "$HERE"/ai-test/*-md-*/v0/art.png; do
  job="$(basename "$(dirname "$(dirname "$png")")")"
  case "$job" in *knight*) continue;; esac
  d="$WORK/$job"; rm -rf "$d"; mkdir -p "$d"; cp -r "$HERE/mdrom/src" "$HERE/mdrom/res" "$d/"; cp "$png" "$d/res/bg.png"
  mode=$(python3 -c "from PIL import Image; im=Image.open('$png'); print(im.mode, len(im.getcolors(1<<24) or []))")
  if docker run --rm --network none --user "$(id -u):$(id -g)" -v "$d:/src" "$SGDK_IMAGE" > "$d/build.log" 2>&1; then
    echo "$job ($mode): built"
  else
    echo "$job ($mode): $(grep -m1 '^Error' "$d/build.log")"
  fi
done | tee "$HERE/results/rescomp-raw.txt"
