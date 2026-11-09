# mdperf: SGDK slowdown reports, and which one helps Claude fix a slow scene

**Finding:** sent a slow SGDK scene with a report and asked for a fix, Claude (Sonnet 5.5) got a CPU-bound scene to 60 game loops per second in 0 of 15 tries, whatever the report said (none, DMA, CPU per function, both, or both plus game loop costs). None came back faster than the original 36.0 and 11 came back slower; a second round with the new numbers reached 58.6 at best. With `./check.sh` in the loop (build plus mdperf, run as often as it liked), it reached 60 in 3 of 3 runs, after 5, 5 and 26 checks, for USD 0.27 to 0.82 each. The other slow scene was easy: 12 of 12 one-shot fixes reached 60, even with no report at all.

Write-up: https://aitoollabs.dev/posts/sgdk-slowdown-claude-fix/

## Results

The two scenes are Claude's own code from the [2026-11-07 test](../2026-11-07-sgdk-dma-budget-checker/), written with "must run at a steady 60 fps on a real NTSC console": two 96x112 fighters drawn in software (`scenes/fighters.c`, 36.0 game loops/s) and an animated title logo whose tiles are generated every frame (`scenes/title.c`, 32.0). Steady play is frames 700 to 1,499 of a 1,500-frame run with the scripted pad of that test, on Genesis Plus GX (NTSC, H40).

One-shot fixes (`claude -p`, no tools, the unchanged scene plus one kind of report), 3 runs each, game loops per second:

| Scene | Report | Round 1 | Round 2 (its own result, same kind of report) |
| --- | --- | --- | --- |
| title (32.0) | none (just the loop rate) | 60, 60, 60 | not needed |
| | DMA (mddma) | 60, 60, 60 | not needed |
| | CPU by function (mdprof) | 60, 60, 60 | not needed |
| | both | 60, 60, 60 | not needed |
| fighters (36.0) | none (just the loop rate) | 36.0, 30.1, 36.0 | 44.3, 58.6, 30.1 |
| | DMA | 30.0, 30.0, 25.9 | 20.2, 36.0, 36.0 |
| | CPU by function | 36.0, 36.0, 30.0 | 44.4, 36.0, 51.6 |
| | both | 25.7, 25.9, 30.1 | 36.0, 44.3, 36.0 |
| | both + game loop costs | 35.9, 25.9, 25.9 | 36.0, 36.0, 45.0 |

Agent loop (Claude Code 2.1.319, Sonnet 5.5, tools Read/Edit/Write and `Bash(./check.sh)`, USD 4 cap), fighters scene:

| Run | check.sh calls | Final loops/s | Slowest loop (cycles, one frame is 128,005) | Mean CPU load | USD at API prices |
| --- | --- | --- | --- | --- | --- |
| loop-1 | 5 | 60.0 | 115,747 | 19.9% | 0.27 |
| loop-2 | 5 | 60.0 | 21,751 | 16.6% | 0.39 |
| loop-3 | 26 | 60.0 | 107,608 | 19.7% | 0.82 |

Other measurements:

- **Same picture:** all 12 title fixes gave a frame 1,000 byte-identical to the original's. For the fighters, `tools/same_scene.py` ran the original and each 60 fps fix with the fighters standing still and only punch presses, and compared every frame from 400 on, half screen per fighter: each fix showed exactly the original's 7 distinct P1 images and 3 P2 images, none missing, none new (`results/same-scene.txt`).
- **Why one-shot failed on the fighters:** 12 of the 15 round-one fixes split the software render into one 32-pixel column per frame (all 15 after round two). That spreads the work, but every column still walks all the rectangles, and the two fighters now render in the same frames: the slowest game loop got more expensive in 10 of 15 runs (226,410 cycles before, up to 277,021 after). The fixes that reached 60 also cached poses: the idle animation has 6 frames but only 3 distinct images, and P2 never punches.
- **What the per-frame profile hid:** mdprof reports video frames, capped at 100%. The original's busiest frames read "127,903 busy cycles (100% of a frame)", with about 111k of them in `fillRect`, `renderFighter` and `memset_`. All 9 round-one replies that had the CPU report put one fighter's render at 100k to 128k cycles. The game loops that rendered really took two frames and 215,375 busy cycles on average, against 20,440 for a loop with no render, so a render costs about 195,000 cycles. A third of that per fighter, with both fighters now rendering every frame, is about 130,000 plus the 20,000 base: still over 128,005. mdperf now adds the per-loop view, but the three replies that had it still estimated 100k to 112k, and that kind of report went 0 of 6.
- **The loop runs stumbled first too:** in 2 of 3, the first check after an edit was slower than the original (25.9 loops/s). All three final versions render by column and cache poses.
- Cost: 43 one-shot `claude -p` calls (42 fixes, one compile fix), USD 5.35 at API prices; the three loop runs, USD 1.48. On a subscription.

These are emulator numbers from Genesis Plus GX. Nothing was checked on a real console.

## How it works

- `tools/mdperf.py` runs a built SGDK project through two existing checkers with the same scripted pad: **mddma** (DMA budget per frame, from [2026-11-07](../2026-11-07-sgdk-dma-budget-checker/)) and **mdprof** (68000 cycles per function, from [2026-10-28](../2026-10-28-sgdk-frame-profiler/)). It writes one report, a merged per-frame CSV, and a game loop section: busy cycles from each frame where a loop started to the frame before the next one did. Emulation is deterministic; mdperf checks that both tools logged the same CPU load per frame (0 mismatches in every run here).
- `tools/fix_test.py` is the one-shot test (`--round2` for the second round), `tools/loop_test.py` the agent loop (`tools/loop_check.py` is its `check.sh`), `tools/same_scene.py` the pixel check, `tools/summary.py` the tables, `tools/backfill_loops.py` added the loop numbers to round-one runs measured before that section existed (it asserts that fps and load reproduce).

## Run it

Needs Docker and Python 3, the mddma and mdprof builds (`checker/build.sh` and `profiler/build.sh` in their folders), and SGDK 2.11 checked out (default `~/.cache/mddma/sgdk-v2.11`). Builds use the `ghcr.io/stephane-d/sgdk` image with SGDK 2.11's `inc`, `lib`, `res`, `src` and `rescomp.jar` mounted over it, as in the 2026-11-07 project.

```sh
# your own SGDK project, after make (out/rom.bin and out/symbol.txt):
python3 tools/mdperf.py path/to/project --input 100-400:R --input 120-124:A --out report/
# the tests (Claude Code logged in):
python3 tools/fix_test.py --reps 3
python3 tools/fix_test.py --scenes fighters --kinds loops --reps 3
python3 tools/fix_test.py --scenes fighters --round2 --reps 3
python3 tools/loop_test.py --reps 3 --budget 4
python3 tools/summary.py
```

## Files

- `scenes/`: the two original scenes (Claude's code from the 2026-11-07 test).
- `results/<scene>-<kind>-<rep>/`: prompt, reply, `main.c`, reports (`report.txt` both, `report-dma.txt`, `report-prof.txt`, `report-loops.txt`), `frames.csv`, `shot.png` (frame 1,000), `result.json`. `-r2` folders are round two (`in-*` is the input it got). `results/loop-<n>/`: the agent loop, with every `check.sh` call in `check-calls.jsonl`.
- `results/summary.txt`: the full table.

## Versions

Genesis Plus GX (libretro) `60bee4c724859710ce05672239e5b601ad01382e`, SGDK 2.11 (m68k-elf-gcc 13.2.0 from the SGDK image), Claude Code 2.1.319, Sonnet 5.5, Docker `gcc:14`.

## Limits

- Two scenes, both Claude's own, both CPU-bound. A DMA-bound scene could favor the DMA report.
- Three runs per cell. Read the one-shot cells as "never got there", not as rates.
- The loop runs had the full mdperf report; I did not run the loop with a weaker report, so "the loop helps" and "the report in the loop helps" are not separated.
- The pixel check covers standing still and punching, not walking, and not punch latency.

MIT license. Genesis Plus GX has its own non-commercial license; it is not included here.
