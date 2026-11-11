# Does Claude need a profiler to fix SGDK slowdown? Frame rate only vs per-loop profile

**Finding:** in an agent loop, Claude Code (Sonnet 5.5) fixed a slow SGDK scene (36 game loops per second, 60 needed) in 12 of 12 runs whatever its `check.sh` printed: just the frame rate, the 2026-11-09 profiler report, or a new per-game-loop profile by function. With the frame rate only, it built its own profiler by switching code off, and needed 6 to 27 checks and USD 0.32 to 1.30. With the per-loop profile it needed 7 to 14 checks and USD 0.33 to 0.62. Without a loop (one `claude -p` reply), the new profile did not help: 0 of 5, and only 1 of 5 replies read the render cost right.

Write-up: https://aitoollabs.dev/posts/sgdk-profiler-claude-fix-loop/

## Results

The scene is `scenes/fighters.c`, Claude's own code from the [2026-11-07 test](../2026-11-07-sgdk-dma-budget-checker/): two 96x112 fighters drawn in software and streamed to VRAM. It runs at 36.0 game loops per second on Genesis Plus GX (NTSC, H40), because every loop that renders a fighter takes two video frames (215,375 busy cycles on average; one frame is 128,005). Steady play is frames 700 to 1,499 of a 1,500-frame run with the scripted pad of that test.

Agent loop (Claude Code 2.1.322, Sonnet 5.5, tools Read/Edit/Write and `Bash(./check.sh)`, USD 4 cap), 4 runs per kind:

| `check.sh` prints | Reached 60 | check.sh calls | Minutes | USD at API prices | Slowest loop at the end (cycles) |
| --- | --- | --- | --- | --- | --- |
| the frame rate only | 4 of 4 | 6, 10, 19, 27 | 2.5 to 10.5 | 0.32, 0.45, 1.22, 1.30 | 49,929 to 121,507 |
| DMA + CPU per frame + loop totals (2026-11-09) | 4 of 4 | 10, 12, 15, 20 | 4.7 to 7.8 | 0.58, 0.68, 0.68, 0.96 | 21,726 to 91,112 |
| DMA + CPU per frame + loops by function (new) | 4 of 4 | 7, 7, 8, 14 | 2.5 to 4.9 | 0.33, 0.38, 0.42, 0.62 | 87,298 to 112,905 |

Totals: frame rate only 62 checks and USD 3.29; the old report 57 and USD 2.90; the per-loop profile 36 and USD 1.74.

One-shot fixes (`claude -p`, no tools, the unchanged scene plus the report), 5 runs per kind, game loops per second:

| Report | Round 1 | Round 2 (its own result, same kind of report) |
| --- | --- | --- |
| none (just the loop rate) | 36.0, 44.3, 30.0, 36.0, 45.3 | 59.1, **60.0**, 58.6, **60.0**, 45.0 |
| DMA + CPU per frame + loop totals | 29.9, 30.0, 25.9, 30.0, 36.0 | 58.6, 44.6, 38.4, 36.0, 38.4 |
| DMA + CPU per frame + loops by function | 36.0, 25.9, 36.0, 25.7, 30.1 | 45.3, 36.0, 30.1, 36.0, 44.3 |

Other measurements:

- **Same picture:** `tools/same_scene.py` (from the 2026-11-09 project) ran the original and all 12 agent-loop fixes with the fighters standing still and only punch presses, and compared every frame from 400 on, half screen per fighter. All 12 show exactly the original's 7 distinct P1 images and 3 P2 images, none missing, none new (`results/same-scene.txt`).
- **What the replies said the render cost:** with the per-loop profile, one-shot replies put a fighter's render at about 200k (1 reply), about 128k per frame (1), and 100k to 112k (3). The real cost is about 195,000 cycles. With the old loop totals: 110k, 128k, 111k, 120k and 170k.
- **How the frame-rate-only runs found the cost:** all four switched the render (or `fillRect`) off, saw 60, and worked from there. Their call logs show 60 early and then lower numbers again; those early 60s were experiments, not fixes.
- **What the runs read outside their folder** (`results/loop-audit.txt`): one frame-rate-only run (rate-2) read the harness's call log after its 22nd of 27 checks, which shows CPU load and the slowest loop; another (rate-3) tried to run the harness with the full report and Claude Code refused the command. No other run saw measurement data beyond its own `check.sh`.
- Cost: 30 one-shot `claude -p` calls, USD 4.05 at API prices; 12 agent-loop runs, USD 7.93. On a subscription.

These are emulator numbers from Genesis Plus GX. Nothing was checked on a real console.

## How it works

- `profiler/mdprof.c` is the 2026-10-28 profiler with one new option, `--fncsv FILE`: cycles per function per video frame (`frame,function,cycles`), counted frames only, idle functions left out. `profiler/build.sh` recompiles only this frontend against the Genesis Plus GX core that the [2026-10-28 project](../2026-10-28-sgdk-frame-profiler/) built (`HOOK_CPU=1`, pinned commit), and writes `mdprof-loops` next to the old `mdprof`.
- `tools/mdperf.py` (from the [2026-11-09 project](../2026-11-09-sgdk-slowdown-fix-reports/)) now runs `mdprof-loops` and adds a "game loops by function" section: a game loop runs from the frame where the CPU left the wait-for-VBlank function to the frame before it left it next, and the functions are added up over all of those frames. It checks that the functions of each loop add up to its busy cycles (0 cycles off in every run here). On the original scene:

```text
== Game loops by function (mdperf) ==
budget: 128,005 cycles per video frame (NTSC); a game loop that needs more misses the next VBlank and the frame is shown twice
game loops measured: 479; cycles below are added up over every video frame a loop spans
  160 loops took 1 video frame: busy cycles 20,329 to 20,515 (mean 20,440)
  319 loops took 2 video frames: busy cycles 211,305 to 226,410 (mean 215,375)
most expensive loop #1: loop starting in frame 1321, 226,410 busy cycles over 2 video frames (177% of one frame)
      142,127  fillRect
       44,984  renderFighter
       18,336  memset_
        9,409  flushQueue
        2,380  DMA_queueDmaFast
        1,776  updateFighterSprites.constprop.1
```

- `tools/loop_test.py --kind rate|loops|loopfn` is the agent loop; `tools/loop_check.py` is its `check.sh` and prints only the kind of report it was given. `tools/fix_test.py` is the one-shot test (`--round2` for the second round). `tools/summary.py` prints the tables (`results/summary.txt`).

## Run it

Needs Docker and Python 3, the 2026-10-28 profiler core (`../2026-10-28-sgdk-frame-profiler/profiler/build.sh`), the mddma build from 2026-11-07 (`checker/build.sh` there), the mdsprites build from 2026-11-05 for the pixel check, and SGDK 2.11 checked out (default `~/.cache/mddma/sgdk-v2.11`). Builds use the `ghcr.io/stephane-d/sgdk` image with SGDK 2.11's `inc`, `lib`, `res`, `src` and `rescomp.jar` mounted over it.

```sh
profiler/build.sh
# your own SGDK project, after make (out/rom.bin and out/symbol.txt):
python3 tools/mdperf.py path/to/project --input 100-400:R --input 120-124:A --out report/
# the tests (Claude Code logged in):
python3 tools/fix_test.py --reps 5 && python3 tools/fix_test.py --reps 5 --round2
for k in rate loops loopfn; do python3 tools/loop_test.py --kind $k --reps 4; done
python3 tools/summary.py
```

## Files

- `results/fighters-<kind>-<rep>/`: one-shot runs (prompt, reply, `main.c`, every report kind, `frames.csv`, `shot.png`, `result.json`); `-r2` is round two. `results/loop-<kind>-<rep>/`: agent-loop runs with every `check.sh` call in `check-calls.jsonl`. `results/fighters-baseline/fns.csv.gz` is the per-function, per-frame log of the original scene; the other runs' logs were left out to keep the repo small (rerun `mdperf.py` to get them).
- `results/summary.txt`, `results/same-scene.txt`, `results/loop-audit.txt`.

## Versions

Genesis Plus GX (libretro) `60bee4c724859710ce05672239e5b601ad01382e`, SGDK 2.11 (m68k-elf-gcc 13.2.0 from the SGDK image), Claude Code 2.1.322, Sonnet 5.5, Docker `gcc:14`.

## Limits

- One scene, CPU-bound, written by Claude. Four agent-loop runs and five one-shot runs per kind: read the differences in checks and cost as a direction, not a rate.
- The frame-rate-only loop is not a clean "no profiler" condition: one run read the harness's call log late in its run, and all four could read the harness source.
- The pixel check covers standing still and punching, not walking, and not punch latency.

MIT license. Genesis Plus GX has its own non-commercial license; it is not included here.
