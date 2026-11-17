#!/usr/bin/env bash
# Everything except the Claude runs (tools/ai_speed.py needs Claude Code):
# build, validate on the test ROM, the cost ROM, Claude's 19 scenes, and the
# same scenes on the nes.h with the cast fix.
set -euo pipefail
cd "$(dirname "$0")"
checker/build.sh
nes/build_rom.sh results/testrom.nes testrom/main.c testrom/burn.s
checker/nesprof.sh results/testrom.nes 240 --loops results/testrom-loops.csv --loopfns results/testrom-loopfns.csv \
  --funcs results/testrom-funcs.csv --csv results/testrom-frames.csv --ram 07F6:lag --ram 07F2:loops_done \
  | tee results/testrom-nesprof.txt
python3 tools/validate.py results/testrom-{loops,loopfns,frames,funcs}.csv --md results/testrom-validation.md
nes/build_rom.sh results/costrom.nes costrom/main.c
checker/nesprof.sh results/costrom.nes 450 --funcs results/costrom-funcs.csv --pcs results/costrom-pcs.csv --top 30 \
  > results/costrom-nesprof.txt
python3 tools/costs.py results/costrom-funcs.csv --md results/costrom-table.md
python3 tools/profile_scenes.py            # needs ../2026-11-15-nes-flicker-checker
python3 tools/profile_scenes.py --castfix
