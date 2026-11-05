#!/usr/bin/env bash
# Checks any ROM with its symbol.txt: limits off, then limits on against that picture.
# usage: rom_check.sh <dir under $WORK with out/rom.bin> <frames> [mdsprites options, e.g. --input ...]
set -euo pipefail
WORK="${WORK:-$HOME/.cache/mdsprites}"
D="$1"; FRAMES="$2"; shift 2
run() { docker run --rm --network none --user "$(id -u):$(id -g)" -v "$WORK:/w" -w /w gcc:14 ./mdsprites gpgx/genesis_plus_gx_libretro.so "$D/out/rom.bin" "$D/out/symbol.txt" "$FRAMES" "$@"; }
run --nolimit --dump "$D/truth.raw" "$@" > /dev/null
run --truth "$D/truth.raw" --csv "$D/frames.csv" --events "$D/events.csv" "$@"
rm -f "${WORK:?}/${D:?}/truth.raw"
