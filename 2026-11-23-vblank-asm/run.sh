#!/usr/bin/env bash
# Builds the runners, stores my reference routines' runs as the references,
# and checks that the checker flags the routines that are wrong on purpose.
set -euo pipefail
cd "$(dirname "$0")"
runner/build.sh
for t in pal row col big; do
  python3 tools/vbrun.py 6502 $t validate/ref-6502-$t.s --out ref-6502-$t --save-ref | head -1
  python3 tools/vbrun.py z80 $t validate/ref-z80-$t.asm --out ref-z80-$t --save-ref | head -1
done
echo "--- wrong on purpose (each should fail):"
python3 tools/vbrun.py 6502 big validate/slow-6502-big.s --out slow-6502-big | head -1
python3 tools/vbrun.py 6502 row validate/noscroll-6502-row.s --out noscroll-6502-row | head -1
python3 tools/vbrun.py z80 big validate/slow-z80-big.asm --out slow-z80-big | head -1
python3 tools/vbrun.py 6502 huge validate/best-6502-huge.s --out best-6502-huge | head -1
python3 tools/vbrun.py z80 huge validate/best-z80-huge.asm --out best-z80-huge | head -1
