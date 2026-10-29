# sgdklint: a performance lint for SGDK code, with measured 68000 costs

**Finding:** in SGDK code, the slow line is often one that looks harmless: `s16 / s16` calls libgcc and costs 613
cycles while `u16 / u16` is a 117-cycle `divu`, `u16 % FIX16(2.7)` costs 1,062 cycles against 168 for `u16 % 20`,
and SGDK 2.09's own `muls()` became a 270-cycle `__mulsi3` call when GCC knew one operand was positive. sgdklint
flags these patterns in the code that runs every frame, with the cost of each measured on a cost-table ROM. On 40
small games written by Claude (Sonnet 5.5, Opus 5.5, Haiku 5.5) it pointed at a cause in all 3 games that dropped
frames (the version tuned on them; v1 ranked two low and missed one), but handing its output back to the model helped less than a plain "make it faster" (load -0.1 to -0.2
points vs -1.4 to -1.8 on Sonnet and Opus), except on Haiku, whose slow spots were exactly what the lint flags
(HUD 19.0% -> 3.7% load).

Write-up: https://aitoollabs.dev/posts/sgdk-performance-lint/

## Measured costs (cost-table ROM, Genesis Plus GX, NTSC frame = 128,005 cycles)

Cycles per operation on top of a loop that does an add instead, from `bench/` profiled with
[mdprof](../2026-10-28-sgdk-frame-profiler/). Full table: `results/costs.txt`; what GCC emitted is in its last column.

| Code | Cycles | GCC emitted |
| --- | --- | --- |
| `s32 / s32` | 617 | `__divsi3` |
| `u32 / u32` | 453 | `__udivsi3` |
| `s16 / s16` | 613 | `__divsi3` |
| `u16 / u16` | 117 | `divu` |
| `s32 % s32` | 1,041 | `__modsi3` |
| `u16 % 20` | 168 | `divu` |
| `u16 % FIX16(2.7)` | 1,062 | `__modsi3` (the macro casts to signed `fix16`) |
| `s32 / 10` | 623 | `__divsi3` |
| `(u16 - 240) / 30` | 640 | `__divsi3` (u16 promoted to int) |
| `u16 / 10` | 77 | `mulu` (reciprocal) |
| `s32 / 8`, `u16 / 8` | 23, 24 | shifts |
| SGDK `divu()`, `divs()` | 133, 139 | `divu`, `divs` |
| `s32 * s32`, `s16 * s32` | 268, 268 | `__mulsi3` |
| `s16 * s16`, SGDK `muls()` | 35, 35 | `muls` |
| SGDK `muls(s16, 256 + u16 % 768)` | 270 | `__mulsi3` |
| `float` add / divide / multiply | 1,147 / 1,221 / 2,285 | `__addsf3` / `__divsf3` / `__mulsf3`, plus 1,046 for `(s32)` |
| `F16_mul`, `F16_div`, `F32_mul` | 59, 155, 305 | `muls`, `divs`, `__mulsi3` |
| 40 x `VDP_setTileMapXY` (one row) | 12,966 | |
| `VDP_fillTileMapRect` 40x1 | 732 | |
| `VDP_setTileMapDataRect` 40x1, CPU / DMA_QUEUE | 1,470 / 1,553 | |
| `VDP_drawText`, 19 characters | 3,673 | |
| `sprintf(buf, "SCORE %ld", v)` | 12,582 | |
| `intToStr(v, buf, 7)` | 2,881 | |
| `SPR_addSprite` + `SPR_releaseSprite` | 1,865 | |
| `VDP_loadTileData` 64 tiles, DMA_QUEUE / CPU | 6,046 / 17,604 | |
| `VDP_drawImageEx` 128x64, NONE / FAST (LZ4W) / APLIB | 22,630 / 138,263 / 549,823 | |

The 128x64 test image compressed to 1% with LZ4W and 31% with APLIB, so real art will unpack at different speeds;
the point is the order of magnitude: a compressed image drawn every frame takes one to four frames here.

## What sgdklint flags

`lint/sgdklint.py` (Python 3, no dependencies) reads the C files of an SGDK project, finds the game loops
(`while (TRUE)`, `while (1)`, `for (;;)`) and the VInt/HInt callbacks, follows direct calls from there, and reports
in that code:

| Rule | What |
| --- | --- |
| `div` | `/` and `%` that compile to libgcc calls: 32-bit or signed operands, `FIX16()` divisors, promoted expressions |
| `mul32` | multiplies with a 32-bit operand and a non-constant other side |
| `float` | `float`/`double` (software floating point) |
| `tile-loop` | `VDP_setTileMapXY` in a loop |
| `redraw` | `VDP_drawImageEx`, `VDP_loadTileSet`, `VDP_loadTileData`, `MAP_create`, `unpack`... on the frame path |
| `text` | `VDP_drawText`, `sprintf`, `intToStr`... on the frame path |
| `sprite-alloc` | `SPR_addSprite`/`SPR_releaseSprite` on the frame path |
| `blocking` | `waitMs`, `JOY_waitPress`, `PAL_fade*(..., FALSE)`, two frame waits in one loop pass |

Each finding has a level: `heavy` (constant loop bounds put one line at half a frame or more), `hot` (in a loop),
`frame` (once per frame), `guarded` (inside an `if`/`switch`, so maybe not every frame), `init` (only with `--all`).

```
$ python3 lint/sgdklint.py ../2026-10-28-sgdk-frame-profiler/testrom/src
.../main.c:99: heavy in 2 loops [tile-loop] VDP_setTileMapXY in a loop: 324 cycles per tile; VDP_fillTileMapRect is
18 per tile, VDP_setTileMapDataRect 37 (one 40-tile row: 12,966 vs 732 vs 1,470). Here: 1,120 passes x 324 =
362,880 cycles (283% of a frame)
.../main.c:73: guarded in 2 loops [div] '/' on 32-bit / 32-bit: 617 cycles (s32 / s32); SGDK divu()/divs() is
133-139, a shift is 23. Here: 24 passes x 617 = 14,808 cycles (12% of a frame)
```

mdprof measured that redraw at 334,924 cycles (2.6 frames), so the estimate was 8% high.

Limits: no real C parser (types come from declarations in the project and a list of SGDK return types; an unknown
type gives a softer "unknown type" finding), calls through function pointers are not followed, early exits from a
loop are not seen (a retry loop of 64 that usually stops at 1 is reported as 64), and GCC's code generation can
still surprise it (the `muls()` case above). Use it to find candidates, and a profiler to confirm.

## On SGDK's own samples

On the 24 sample projects of the SGDK v2.10 tag, the two game samples (`game/platformer`, `game/sonic`) get no
findings. The first version of the lint flagged 12 lines in `game/sonic`, all float literals inside `FIX32(1.5)`
and friends, which the compiler folds into integers; the lint now skips them. Outside SGDK's `benchmark` sample
(which runs slow operations on purpose), the 17 non-guarded findings are all text drawn every frame by demos
(`VDP_drawText`, `sprintf`, `BMP_drawText`): real per-frame costs, but deliberate ones. Output:
`results/sgdk-samples.txt`.

## The AI test (40 games, `tools/ai_test.py`)

10 tasks (starfield, bullets, platformer, snake, breakout, particles, homing enemies, scroller, HUD, pong), one
prompt each asking for a complete SGDK 2.09 `main.c` with tiles as arrays in code. Sonnet 5.5 twice, Opus 5.5 and
Haiku 5.5 once, through `claude -p` with tools off and no CLAUDE.md. Each program was built (compiler errors sent
back, up to 3 rounds), linted, and profiled for 1,260 frames with a scripted pad. Then three fix rounds from the same
code: with lint v1's output, with lint v2's output, and with only "make it run faster on the real hardware".
185 calls, USD 15.97 at API prices (run on a subscription).

| Model | Programs | Built first try | Mean load | Programs with dropped frames | Lint v2 heavy | Lint v2 hot + frame |
| --- | --- | --- | --- | --- | --- | --- |
| Sonnet 5.5 | 20 | 18 | 7.7% | 2 (10 frames) | 1 | 13 |
| Opus 5.5 | 10 | 10 | 7.4% | 1 (6 frames) | 2 | 1 |
| Haiku 5.5 | 10 | 0 | 11.2% | 0 | 2 | 19 |

| Model | Fix round | Mean load change | Dropped frames after |
| --- | --- | --- | --- |
| Sonnet 5.5 | lint v1 / lint v2 / plain | -0.13 / -0.14 / -1.42 points | 5 / 5 / 0 |
| Opus 5.5 | lint v1 / lint v2 / plain | -0.09 / -0.10 / -1.83 points | 2 / 2 / 0 |
| Haiku 5.5 | lint v1 / lint v2 / plain | -2.55 / -2.63 / +2.73 points | 0 / 0 / 880 |

Haiku's "make it faster" scroller rewrote the column upload with raw VDP port writes and then spent 74% of all
cycles in one function, 880 dropped frames. Haiku's build errors were API drift: 9 of 10 programs used deprecated
names (`TILE_USERINDEX` 25 times) and 7 of 10 the old `VDP_setSprite` signature. Sonnet's two errors were
`SYS_doVSync` (does not exist) and `fix16Mul` (removed in favour of `F16_mul`).

Per-program table: `results/ai-test-summary.md`. Everything per program (prompt reply, code of each round, build
errors, lint output, mdprof report): `ai-test/<task>-<model>-r<rep>/`. Screenshots of the first versions at frame
600: `results/ai-test-v0-screens.png`. Lint v1 (used in the first fix round) is kept in `lint/history/`.

## Run it

Needs Docker, git, Python 3 (Pillow only for `tools/make_art.py`), and mdprof from the previous project:

```bash
WORK=~/.cache/mdprof ../2026-10-28-sgdk-frame-profiler/profiler/build.sh   # Genesis Plus GX core + mdprof
./run-bench.sh                                   # cost-table ROM -> results/costs.txt
python3 lint/sgdklint.py path/to/your/project/src
python3 tools/ai_test.py --models claude-sonnet-5-5:1 --tasks hud    # needs the claude CLI
python3 tools/ai_test.py --lint2 && python3 tools/summarize.py
```

## Versions

- SGDK Docker image `ghcr.io/stephane-d/sgdk:latest`, digest `sha256:b0bb8a71...cf31` (`genesis.h`: 2.09),
  m68k-elf-gcc 13.2.0; samples from the SGDK v2.10 tag (MIT), lint results in `results/sgdk-samples.txt`
- Genesis Plus GX (libretro) commit `60bee4c724859710ce05672239e5b601ad01382e`, via mdprof
- Claude Code 2.1.312; models `claude-sonnet-5-5`, `claude-opus-5-5`, `claude-haiku-5-5`
- Cost-table ROM SHA-256 in `results/bench-rom.sha256`

These are emulator numbers. Genesis Plus GX models 68000 timings closely, but nothing here was checked on a real
console. MIT license; code in `ai-test/` was written by Claude models for this test.
