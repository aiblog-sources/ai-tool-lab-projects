#!/usr/bin/env bash
# Test ROM through nessprites on FCEUmm (limit on, checked against the
# no-limit picture) and on Mesen (the overflow flag the ROM reads).
set -euo pipefail
cd "$(dirname "$0")"
F=${FRAMES:-1260}
nes/build_rom.sh results/testrom.nes testrom/main.c
checker/nessprites.sh /w/fceumm/fceumm_libretro.so results/testrom.nes $F --nolimit --dump results/.nolimit.bin
checker/nessprites.sh /w/fceumm/fceumm_libretro.so results/testrom.nes $F --truth results/.nolimit.bin \
  --csv results/testrom-frames.csv --events results/testrom-events.csv --flagaddr 07F0 --ram 07F5:layout --ram 07F1:nmi --ram 07F6:lag \
  | tee results/testrom-fceumm.txt
rm -f results/.nolimit.bin
checker/nessprites.sh /w/mesen/Libretro/mesen_libretro.so results/testrom.nes $F \
  --csv results/testrom-mesen-frames.csv --flagaddr 07F0 --ram 07F5:layout --ram 07F1:nmi | tee results/testrom-mesen.txt
python3 tools/by_layout.py results/testrom-frames.csv results/testrom-events.csv results/testrom-mesen-frames.csv | tee results/testrom-table.md
