#!/usr/bin/env bash
# Validation: hand-counted routines on both runners, the hang and stray-write
# cases, and the checker self-test. Run runner/build.sh first.
set -euo pipefail
cd "$(dirname "$0")"
{
  python3 tools/asmrun.py 6502 popcount validate/popcount-fixed.s --out val-6502-fixed
  python3 tools/asmrun.py z80 popcount validate/popcount-fixed.asm --out val-z80-fixed
  python3 tools/asmrun.py 6502 popcount validate/popcount-hang.s --out val-6502-hang
  python3 tools/asmrun.py z80 popcount validate/popcount-stray.asm --out val-z80-stray
  python3 tools/selftest.py
} 2>&1 | tee results/validation.txt
