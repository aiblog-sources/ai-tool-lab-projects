# sgdkhot: a line profiler for SGDK games, and what it says about a lint

**Finding:** charged line by line, the 40 small Mega Drive games Claude wrote for the
[2026-10-29 lint test](../2026-10-29-sgdk-perf-lint/) spend their time where the lint was not looking. Only 9 of the
lint's 252 findings cost at least 1% of a frame (1,280 cycles) per frame; 191 cost under 0.2%, and 39 never ran. The
hottest line of a game carried a lint finding in 11 of 40. Giving Claude the measured line profile with its "make it
faster" prompt did about as well as the prompt alone (Sonnet 5.5 -1.46 vs -1.42 points of CPU load, Opus 5.5 -1.93
vs -1.83; program by program, lower in 14, higher in 16, same in 10).

Write-up: https://aitoollabs.dev/posts/sgdk-line-profiler/

## What it does

- `profiler/mdlines.c` is mdprof (from the [2026-10-28](../2026-10-28-sgdk-frame-profiler/) and
  [2026-11-11](../2026-11-11-sgdk-loop-profile/) projects) with one new option, `--lines FILE`. It keeps a shadow
  call stack from Genesis Plus GX's per-instruction CPU hook: a `JSR`/`BSR` pushes a frame, the stack pointer rising
  above a frame pops it, and a jump to an exception vector (VBlank, HBlank...) starts a new context. Every busy cycle
  (idle = the wait-for-VBlank function) goes either to the game instruction that ran it or, for SGDK and libgcc code,
  to the game instruction that called into it. "Game code" = symbols whose `symbol.txt` source path is the project's
  `src/` (outside `boot/`), so the build needs `-g`.
- `tools/sgdkhot.py` builds the project with `EXTRA_FLAGS=-g` (debug info only: the ROM is byte-identical, checked),
  runs mdlines with a scripted pad, maps the call sites to C lines with `m68k-elf-addr2line -i` (SGDK code that LTO
  inlined into a game function is charged to the game line that used it), runs
  [sgdklint](../2026-10-29-sgdk-perf-lint/lint/sgdklint.py) on the same source and joins the two:

```text
== Lines by measured cost (sgdkhot: mdlines + sgdklint) ==
budget 128,005 cycles per video frame (NTSC); 1,200 frames counted; busy 24,334 cycles per frame on average (19.0%)
    7,378 /frame  30.3% of busy  src/main.c:57  buf[digits - 1 - i] = '0' + (char)(value % 10);
                     6,674 in __umodsi3: 8.00 calls/frame, 834 cycles each
                 lint [div] hot: '%' on 32-bit / constant: 1,041 cycles (s32 % s32); u16 % u16 is 126
    3,662 /frame  15.0% of busy  src/main.c:62  VDP_drawTextBG(BG_A, buf, x, y);
                     2,418 in VDP_setTileMapDataRowEx.constprop.0: 2.00 calls/frame, 1,209 cycles each
...
== Lint findings, by measured cost ==
   matters     7,378 /frame (5.76% of a frame)  src/main.c:57 [div] hot
```

Verdicts: `matters` at least 1% of a frame per frame on average, `small` 0.2 to 1%, `negligible` under 0.2%,
`not run` no cycles on that line in the run. A line that starts a DMA from 68000 memory also carries the CPU stall
(the core moves the CPU clock past the transfer).

## Is it right?

- **Validation ROM** (`validate/src/main.c`, counts known by construction): 100 `s32` divisions per game loop on one
  line, 50 multiplies on another, a `noinline` helper, and 30 divisions in the VBlank callback. The loop takes two
  frames, and the report shows 50.00 `__divsi3` calls per frame on the division line, 25.00 `__mulsi3` on the multiply
  line, and 30.00 on the callback's own line, at 576 and 573 cycles per call: the interrupt work did not leak into the
  line it interrupted. Output: `results/validate.txt`.
- **Same run as before:** all 154 programs of the 2026-10-29 test that built (v0, lint, lint2 and plain rounds) give
  exactly the CPU load and dropped frames that mdprof reported then, and the cycles charged to lines add up to the
  busy cycles of `frames.csv` within rounding (at most 685 cycles over a 1,200-frame run). No call stack overflowed.
- **Speed:** 1,076 frames per second with the line table on the HUD game, against 1,585 for the old frontend and
  1,978 with the hook off (three runs each, 2-core laptop), so about 18x real time.

## Results on the 40 games (`results/summary.md`)

| Rule | Findings | matters | small | negligible | not run |
| --- | --- | --- | --- | --- | --- |
| text | 152 | 2 | 3 | 126 | 21 |
| div | 68 | 4 | 7 | 45 | 12 |
| tile-loop | 21 | 2 | 3 | 12 | 4 |
| mul32 | 11 | 1 | 0 | 8 | 2 |
| all | 252 | 9 | 13 | 191 | 39 |

- 214 of the 252 findings were `guarded` (inside an `if`); 2 of those mattered. The games mostly redraw text only
  when a value changes, so a `VDP_drawText` that costs 1,500 to 3,900 cycles a call runs once every few hundred
  frames.
- The lint's cost per operation was close: measured cycles per libgcc call / lint estimate, median 0.94 (0.66 to
  2.22, 43 findings). Its cost per frame was not: median 0.11 of what it claimed, because it assumes every loop pass
  runs the line.
- The hottest game line (leaving out `SYS_doVBlankProcess`) was the game's own code, often with SGDK sprite calls
  inlined into it, in 17 of 40; a `VDP_updateSprites` / DMA line in 15 (623 to 3,572 cycles per frame across the 32
  such lines); a libgcc division in 5; `VDP_drawText` in 3. A median of 1.4% of busy cycles sat on lines the lint
  flagged (0 to 76.5%).

**Fix round** (`tools/fix_hot.py`): the 2026-10-29 `plain` prompt ("make it run faster on the real hardware without
changing what the game does or how it looks") plus the sgdkhot report of the original code, same model that wrote
each game, one reply, up to 3 build-repair rounds. 44 calls, USD 4.26 at API prices (run on a subscription), Claude
Code 2.1.332.

| Model | Round | Mean load before | Mean load after | Change | Faster / slower | Dropped frames after |
| --- | --- | --- | --- | --- | --- | --- |
| Sonnet 5.5 (20) | lint v2 (10-29) | 7.27 | 7.12 | -0.14 | 9 / 3 | 5 |
| Sonnet 5.5 (20) | plain (10-29) | 7.69 | 6.27 | -1.42 | 20 / 0 | 0 |
| Sonnet 5.5 (20) | plain + line profile | 7.69 | 6.24 | -1.46 | 19 / 0 | 0 |
| Opus 5.5 (10) | lint v2 (10-29) | 7.65 | 7.55 | -0.10 | 4 / 0 | 2 |
| Opus 5.5 (10) | plain (10-29) | 7.38 | 5.55 | -1.83 | 10 / 0 | 0 |
| Opus 5.5 (10) | plain + line profile | 7.38 | 5.45 | -1.93 | 10 / 0 | 2 |
| Haiku 5.5 (10) | lint v2 (10-29) | 11.18 | 8.56 | -2.63 | 4 / 0 | 0 |
| Haiku 5.5 (10) | plain (10-29) | 11.18 | 13.91 | +2.73 | 7 / 1 | 880 |
| Haiku 5.5 (10) | plain + line profile | 11.18 | 6.93 | -4.26 | 8 / 1 | 0 |

The lint v2 round skipped programs with no findings (Sonnet 18 of 20, Opus 9 of 10). Haiku's difference is one
program: its `plain` scroller rewrote the column upload and went to 75% load with 880 dropped frames; without that
program the two rounds are equal (-4.77 vs -4.63). Opus's 2 dropped frames are one-off redraws right after the Start
presses in the pad script (frames 71 and 301 of the platformer). Same picture at frame 600 as the original
(`results/same-shot.txt`, a rough check): plain + profile 30 of 40, plain 28 of 40, lint v2 32 of 37.

## Run it

Needs Docker, Python 3, the Genesis Plus GX core from the 2026-10-28 project, and the SGDK Docker image.

```sh
WORK=~/.cache/mdprof ../2026-10-28-sgdk-frame-profiler/profiler/build.sh   # the core (once)
profiler/build.sh                                                        # mdlines
python3 tools/sgdkhot.py path/to/sgdk/project --input 100-400:R --input 120-124:A --json hot.json
# the tests
python3 tools/sgdkhot.py validate --frames 600 --from 60
python3 tools/corpus.py --stages v0,lint,lint2,plain     # re-profile the 2026-10-29 programs
python3 tools/fix_hot.py                                 # the new fix round (claude CLI, logged in)
python3 tools/summary.py && python3 tools/same_shot.py   # same_shot needs the .ppm frames: rerun corpus.py first
```

`sgdkhot.py` builds in the project folder (like SGDK's own Docker build) and writes `out/lines.csv`,
`out/prof.json`, `out/frames.csv`. `--no-build` reuses an existing `out/` (it needs `rom.out` from a `-g` build).

## Files

- `results/corpus/<program>/<stage>.txt` and `.json`: the report and the data (every line, every finding with its
  verdict) for v0, lint, lint2, plain and hot. `hot-prompt.md` is exactly what the model saw, `hot-reply.md` its
  reply, `hot.c` the code, `hot-result.json` cost and build rounds. Frame screenshots were left out (47 MB); rerun
  `corpus.py` to get them.
- `results/summary.md` (all tables), `results/validate.txt`, `results/same-shot.txt`.

## Limits

- Emulator numbers from Genesis Plus GX; nothing was checked on a real console.
- One scripted pad run per program: a line the script never reached shows as `not run`, not as cheap.
- Light games: the 40 programs average 7 to 11% CPU load, and 3 of the 40 originals dropped frames (16 in all). A
  game near its frame budget may rank its lines differently.
- One reply per program in the fix round, and the `plain` / `lint v2` rounds ran four weeks earlier on Claude Code
  2.1.312: read the small differences between rounds as noise.
- Code that runs from RAM, and calls made through `JMP` tail calls, are charged to the frame that was current; the
  stack check assumes the game does not move the stack pointer by hand.

## Versions

Genesis Plus GX (libretro) `60bee4c724859710ce05672239e5b601ad01382e`; SGDK Docker image
`ghcr.io/stephane-d/sgdk:latest` (digest `sha256:b0bb8a71...cf31`, SGDK 2.09, m68k-elf-gcc 13.2.0), the same image as 2026-10-29; Docker `gcc:14`
for the frontend; Claude Code 2.1.332 with `claude-sonnet-5-5`, `claude-opus-5-5`, `claude-haiku-5-5`.

MIT license. Genesis Plus GX has its own non-commercial license; it is not included here. The games in
`results/corpus/` were written by Claude models for the 2026-10-29 test and this one.
