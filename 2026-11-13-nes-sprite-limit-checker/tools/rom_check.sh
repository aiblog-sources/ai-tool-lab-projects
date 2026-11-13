#!/usr/bin/env bash
# rom_check.sh <dir-with-rom.nes, relative to the repo folder> <frames> [nessprites options]
# Runs the ROM twice in FCEUmm (limit off with every frame saved, then limit
# on compared with it) and writes frames.csv and events.csv next to the ROM.
set -euo pipefail
cd "$(dirname "$0")/.."
D="$1"; F="$2"; shift 2
checker/nessprites.sh /w/fceumm/fceumm_libretro.so "$D/rom.nes" "$F" --nolimit --dump "$D/.nolimit.bin" "$@" >/dev/null
checker/nessprites.sh /w/fceumm/fceumm_libretro.so "$D/rom.nes" "$F" --truth "$D/.nolimit.bin" \
  --csv "$D/frames.csv" --events "$D/events.csv" --flagaddr 07F0 --ram 07F1:nmi --ram 07F6:lag "$@"
rm -f "$D/.nolimit.bin"
