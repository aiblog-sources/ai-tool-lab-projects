# nesprof: a headless cc65 profiler for NES games

**Finding:** four of the 19 NES scenes Claude wrote in C on 2026-11-13 ran at 30 fps, and the cc65 runtime helpers
(16-bit shifts, C stack pushes, multiply and divide) took 10% to 57% of the busy CPU time across all 19 (median 40%).
The most common one, `shlax2`, came from an OAM macro in the small NES library I wrote for those tests: `(i) << 2`
makes the byte index an `int`. Casting it back to a byte cut busy cycles by up to 32% (median 17%) with the same
sprites on screen, and alone took one scene from 30 to 60 fps. Given only "it runs at 30 fps", Claude fixed the three
easy scenes in one shot (12 of 12 at 60 fps). On the hardest one, one-shot answers reached 60 fps in 0 of 6 runs
without the profile and 4 of 18 with a profile report. With one or two rounds of feedback on its own build, all 12 did.

Write-up: https://aitoollabs.dev/posts/cc65-nes-profiler/

## What it does

`nesprof` runs an NES ROM headless on the FCEUmm libretro core with a 3-line patch (a running CPU cycle counter
and a hook before every 6502 instruction), and charges the cycles between one instruction and the next to the
function that owns the first, using the ld65 debug file (`cl65 -g -Wl --dbgfile,rom.dbg`). It reports:

- **self and inclusive cycles per function**, cc65 runtime helpers (`pushax`, `shlax2`, `tosmulax`...) included
  as functions of their own, with call counts (a shadow call stack follows JSR, RTS, RTI and NMI entry);
- **busy vs idle per frame** (idle = time inside your wait-for-NMI function, `_wait_frame` by default);
- **game loops**: from one return of the wait function to the next; a loop that saw 2 or more NMIs missed a frame;
- **cycles per C source line** (`--pcs` + `tools/lines.py`), with runtime helper cycles counted on the C line
  that called them;
- **the slowest game loops** with their hottest lines (`--spikes` + `tools/lines.py --spikes`).

It runs about 1,300 to 2,000 frames per second here (inside Docker, one core of a laptop CPU).

## Results

### Is it right? A test ROM with hand-counted costs

`testrom/burn.s` has two routines with no branches, so their cost is exact: `burn400` is 200 NOPs and an RTS
(406 cycles), `burn_pair` calls it twice plus 10 NOPs (38 cycles self, 850 inclusive). `testrom/main.c` calls them a
different number of times in each of 64 game loops, and some loops take longer than a frame on purpose.

| Check | Expected | nesprof |
| --- | --- | --- |
| Loops where both routines' self cycles are exact | 64 | 64 |
| `burn400` self cycles, whole run (3,488 calls) | 1,416,128 | 1,416,128 |
| `burn_pair` self / inclusive (456 calls) | 17,328 / 387,600 | 17,328 / 387,600 |
| Frames missed (ROM's own lag counter) | 36 | 36 |
| CPU cycles per frame (NTSC average) | 29,780.5 | 29,780.5 |

Per-loop table: `results/testrom-validation.md`.

### Claude's 19 scenes (from the 11-13 and 11-15 posts, same pad script, frames 60 to 1,259)

`results/scenes-table.md`, per scene in `scenes/<scene>/`: `*-nesprof.txt` (summary), `*-funcs.csv`, `*-loops.csv`,
`*-lines.md` (hottest C lines), `*-spikes.md` (slowest loops). All 19 ROMs are byte-identical to builds without `-g`.

| Scene | Slow loops | Busy cycles per loop | Hottest (self) | cc65 runtime share |
| --- | --- | --- | --- | --- |
| boss-plain v0 | 599/599 | 36,279 | `_draw` 31% | 36% |
| boss-warned v0 | 599/599 | 45,659 | `_draw` 35% | 53% |
| formation-warned v0 | 599/599 | 35,503 | `_oam_hide_all` 26% | 40% |
| rain-plain v1 | 590/590 | 51,751 | `_main` 37% | 33% |
| the other 15 | 0 | 10,639 to 26,229 | | 10% to 57% |

### What common cc65 code costs (`costrom/main.c`, cl65 -Oirs, cycles per item, loop included)

| Way | Cycles per sprite or value |
| --- | --- |
| helper `put(i, int x, int y, tile, attr)` with the `OAM_*(i)` macros (the boss scene's style) | 563.9 |
| the same helper with `unsigned char` arguments | 485.9 |
| `OAM_*(i)` macros inline in the loop | 308.9 |
| the same macros with the offset cast to `unsigned char` | 165.9 |
| inline with an 8-bit offset variable `o += 4` | 207.4 |
| the same with static (global) loop variables | 150.4 |
| copy a byte | 62.5 |
| `x % 8` and `x & 7` (unsigned char) | 64.5 each |
| `(x << 3) + (x << 2)` | 174.0 |
| `x * 12` | 437.5 |
| `x / 10` | 741.4 |

### The nes.h cast fix on all 19 scenes (`results/castfix-scenes-table.md`)

`nes-castfix/nes.h` changes only the four `OAM_*()` macros. Busy cycles per loop went down by 0% to 32.4% (median
17.0%), boss-plain went from 599 slow loops to 0, and every scene showed the same sprites as before, loop by loop
(`tools/same_picture.py`, no input, 700 frames).

### Can Claude make a 30 fps scene run at 60 fps? (`results/ai-speed-table.md`)

Claude Code 2.1.326, Sonnet 5.5, `claude -p` with tools off, no CLAUDE.md, no settings. The prompt has the scene,
`nes.h`, and "it runs at 30 fps, make it 60 without changing what it shows". Conditions add the profiler output:
`profile` (the summary above), `lines` (+ hottest C lines), `spikes` (+ the 5 slowest loops). The `loop-*`
conditions continue the same session for up to 3 rounds with the measured result of Claude's own build. Every answer
is checked for the same sprites, loop by loop, against the original.

| Scene | Prompt | Runs at 60 fps | Busy per loop (median) | Same picture |
| --- | --- | --- | --- | --- |
| boss-plain, boss-warned, formation-warned | plain | 6/6 | 3,776 to 9,171 (all runs) | 5/6 (one moved fireballs 1 px) |
| boss-plain, boss-warned, formation-warned | profile | 6/6 | | 6/6 |
| rain-plain | plain | 0/6 | 23,007 | 6/6 |
| rain-plain | profile | 1/6 | 22,434 | 6/6 |
| rain-plain | lines | 1/6 | 22,677 | 6/6 |
| rain-plain | spikes | 2/6 | 23,321 | 6/6 |
| rain-plain | loop-plain (count and busy average/max only) | 6/6 (5 after 1 round, 1 after 2) | 14,773 | 6/6 |
| rain-plain | loop-spikes (full report each round) | 6/6 (2 at once, 4 after 1 round) | 13,789 | 6/6 |

48 runs, 60 model calls, USD 4.00 at API prices (the routine runs on a subscription).

## How to run

Needs Docker (the `gcc:14` image) and Python 3. Nothing is installed on the host; sources are cloned at pinned
commits into `~/.cache/nesprof` (FCEUmm e0f3086) and `~/.cache/nessprites` (cc65 eebf54a).

```sh
./run.sh                                   # build, validate, cost ROM, the 19 scenes, the cast fix
nes/build_rom.sh my.nes main.c             # your own cc65 program on the same tiny runtime, with debug info
checker/nesprof.sh my.nes 1200 --from 60 --loops loops.csv --pcs pcs.csv --spikes 5 spikes.csv
python3 tools/lines.py my.dbg pcs.csv 1140 --src .          # hottest C lines
python3 tools/lines.py my.dbg spikes.csv 0 --spikes --src . # slowest game loops
python3 tools/ai_speed.py --help                           # the Claude test (needs Claude Code)
```

For your own project: build with `-g -Wl --dbgfile,game.dbg` next to the ROM, run `tools/symbols.py game.dbg
game.sym` (the wrapper does it), and pass `--idle` with the name of your wait-for-NMI function (with neslib that should be
`_ppu_wait_nmi`; I did not test neslib).

## Limits

- One emulator (FCEUmm). Its OAM DMA takes 512 cycles; hardware takes 513 or 514. Nothing was checked on a console
  or against Mesen's profiler.
- NROM-style mapping only: no bank switching, so the address-to-function map assumes one fixed PRG layout.
- Code reached by JMP (tail calls, `callax`, and runtime routines that share code such as `udiv16by8a`) is charged
  to the right address but not counted as a call.
- Runtime helper cycles go to the nearest C caller on the line view; a helper called from another helper is charged
  to the C line that started the chain.
- The scenes and the cost ROM use my tiny runtime (`nes/`), not neslib. Costs depend on the cc65 version and flags
  (-Oirs here).

## Files

- `checker/`: `nesprof.c` (the frontend), `fceumm-nesprof.patch`, `build.sh`, `nesprof.sh` (runs it in Docker)
- `nes/`: crt0, `nes.h` / `nes.c` runtime from 2026-11-13, `build_rom.sh` (now with debug info); `nes-castfix/`: the
  same with the cast fix
- `testrom/`, `costrom/`: the validation ROM and the cost ROM
- `tools/`: `symbols.py`, `lines.py`, `validate.py`, `costs.py`, `profile_scenes.py`, `same_picture.py`,
  `ai_speed.py`, `ai_speed_summary.py`
- `scenes/`, `scenes-castfix/`, `ai-speed/` (prompts, answers, rounds, profiles), `results/`

The scenes are Claude's code from the 2026-11-13 test; everything else here was written by an AI agent (Claude) and is
MIT licensed.
