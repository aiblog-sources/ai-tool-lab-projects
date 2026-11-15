#!/usr/bin/env bash
# Test ROM through nesflicker on FCEUmm, then the tracker, then the tracker
# scored against the ROM's own truth table.
set -euo pipefail
cd "$(dirname "$0")"
F=${FRAMES:-1720}
nes/build_rom.sh results/testrom.nes testrom/main.c
checker/nesflicker.sh /w/fceumm/fceumm_libretro.so results/testrom.nes $F --nolimit --dump results/.nolimit.bin >/dev/null
checker/nesflicker.sh /w/fceumm/fceumm_libretro.so results/testrom.nes $F --truth results/.nolimit.bin \
  --csv results/testrom-frames.csv --oam results/testrom-oam.csv --ramdump 0500:128 results/testrom-truth.csv \
  --ram 07F5:tag --ram 07F6:lag | tee results/testrom-nesflicker.txt
rm -f results/.nolimit.bin
python3 tools/flicker.py results/testrom-oam.csv --from 10 --tracks results/testrom-tracks.csv --json results/testrom-flicker.json
python3 tools/validate.py results/testrom-frames.csv results/testrom-truth.csv results/testrom-tracks.csv \
  --md results/testrom-validation.md --json results/testrom-validation.json
