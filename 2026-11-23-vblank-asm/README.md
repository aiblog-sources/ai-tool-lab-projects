# Can Claude fit NES and Master System updates in VBlank? 96 routines timed

**Finding:** Claude Sonnet 5.5 and Opus 5.5 wrote 96 VBlank update routines in 6502 (NES, called from NMI) and Z80
(Master System, called from the VBlank interrupt): palette, a nametable row, a column, and a big copy at the
limit (OAM DMA + 160 bytes on the NES, 768 bytes on the Master System). All 96 were right on the first try, in
every frame, and finished inside vertical blank, with or without the prompt saying "finish inside vertical blank".
On the big copies they unrolled fully: the last NES write landed about 400 cycles before VBlank ended (1,851-1,861
of 2,260). Their own cycle counts were within 1% of the measurement in 60 of 60 first tries. Given a copy that cannot
fit (OAM DMA + 256 bytes, or 1,024 bytes), all 12 first replies said so. Sonnet copied only what fits; Opus mostly
shipped the full copy, knowingly late, and switched to splitting it over two frames after one report.

Write-up: https://aitoollabs.dev/posts/claude-vblank-nes-master-system/

| Prompt | Model | NES first try | Master System first try |
| --- | --- | --- | --- |
| warned ("finish inside vertical blank", scroll rule) | Sonnet 5.5 | 12/12 | 12/12 |
| warned | Opus 5.5 | 12/12 | 12/12 |
| plain (same facts, no timing or scroll rule) | Sonnet 5.5 | 12/12 | 12/12 |
| plain | Opus 5.5 | 12/12 | 12/12 |

The copy that cannot fit (`huge`, warned prompt, 3 reps per model and CPU):

| Model | First reply said it cannot fit | First try strategy | After up to 2 fix rounds |
| --- | --- | --- | --- |
| Sonnet 5.5 | 6/6 | 5 copied only what fits (200 of 256 bytes, 928-960 of 1,024), 1 overran by 88 cycles | 3 truncated, 1 split over two frames, 1 split over two frames but 117 cycles late, 1 turned rendering off (and said so) |
| Opus 5.5 | 6/6 | 5 full copy, late by about 360 (NES) or 910 (SMS) cycles, 1 split over two frames | 5 split over two frames, 1 truncated to 210 bytes |

Full tables: [results/summary.md](results/summary.md) and [results/huge.md](results/huge.md).

## What it does

- `runner/nesvb.c` runs an NES ROM headless on the FCEUmm libretro core with the cycle counter and per-instruction hook
  from [2026-11-17-nes-cc65-profiler](../2026-11-17-nes-cc65-profiler). For the store instructions that can reach a PPU
  register it works out the target address and logs every write to `$2000-$2007` (and mirrors) and `$4014` made
  inside the NMI handler, with its cycle from the handler's first instruction. An OAM DMA counts 513 more cycles. The
  end of vertical blank comes from the emulator itself: FCEUmm sets `scanline` to 0 one scanline after the pre-render
  line starts. It measures 2,260 to 2,263 cycles from the handler's first instruction (2,273 minus the NMI entry).
- `runner/smsvb.c` does the same on Genesis Plus GX in Master System mode with no emulator patch: it wraps the core's
  exported `z80_writeport` (as smsvdp did in [2026-11-19-sms-vdp-timing-checker](../2026-11-19-sms-vdp-timing-checker))
  and the handler marks its start and end with `OUT ($DE)` / `OUT ($DF)`. The core starts each frame at line 192, so
  vertical blank ends at 70 x 228 = 15,960 cycles. The interrupt handler starts at cycle 252.
- `harness/nes.s` and `harness/sms.asm`: init with rendering off (tiles, a nametable pattern, palette, sprites, the
  case's input data), then NMI / frame interrupt and rendering on. The handler saves the registers, calls
  `vblank_update` and returns.
- `tools/tasks.py`: the specs, the prompt context, 3 cases per task (rows 0, 13, 29; columns 0, 17, 31; three
  destination addresses; seeded random data).
- `tools/vbrun.py` builds a routine into the harness for each case, runs 40 frames and checks frames 15-39 (the NMI is
  on from frame 10): the destination bytes, the whole video memory and the last picture against my reference
  routine's run, and the timing of every write in every frame.
- `tools/ai_test.py` asks `claude -p` (tools off, no CLAUDE.md, no settings) for each routine, runs it, and gives it up
  to two fix rounds with the report. `--variant plain` drops the timing and scroll rules from the prompt.
- `tools/summarize.py` and `tools/huge_table.py` write `results/summary.md` and `results/huge.md`.

## Validation

`./run.sh` ([results/validation.log](results/validation.log)) stores my reference routines' runs and checks that the
checker fails routines that are wrong on purpose:

| Routine | Last write | VBlank end | Result |
| --- | --- | --- | --- |
| NES big, fully unrolled (8 cycles a byte) | 1,857 | 2,260 | pass |
| NES big, plain indexed loop (15 cycles a byte) | 2,978 | 2,261 | late; FCEUmm also put 47 of the 160 bytes in the wrong place and drew the frame shifted |
| NES row without the scroll reset | 530 | 2,262 | video memory right, picture wrong |
| SMS big, fully unrolled `OUTI` (16 cycles a byte) | 12,775 | 15,960 | pass |
| SMS big, three `OTIR` (21 cycles a byte) | 16,603 | 15,960 | late (Genesis Plus GX still stored every byte) |
| NES huge, fully unrolled | 2,625 | 2,263 | late: no plain copy fits |
| SMS huge, fully unrolled `OUTI` | 16,871 | 15,960 | late: no plain copy fits |

Cycle counts were checked against hand counts: my NES palette routine is 519 cycles from the JSR to its last write and
nesvb measured 532 from the handler start (13 for the handler's `PHA TXA PHA TYA PHA`). The Master System offset from
line 192 to the `CALL` (398 cycles) was calibrated the same way on the palette routine.

## How to run it

Needs Docker (the `gcc:14` image), Python 3 and, for the AI test, Claude Code. Everything compiles and runs inside the
container with no network.

```sh
../2026-11-17-nes-cc65-profiler/checker/build.sh       # cc65 + FCEUmm with the nesprof patch
../2026-11-19-sms-vdp-timing-checker/checker/build.sh  # Genesis Plus GX
../2026-11-21-6502-vs-z80-asm/runner/build.sh          # sjasmplus v1.24.1
./run.sh                                               # nesvb, smsvb, references, validation
python3 tools/vbrun.py 6502 big my_update.s            # check your own routine
python3 tools/ai_test.py --model claude-sonnet-5-5     # warned prompt, 4 tasks, 3 reps
python3 tools/ai_test.py --model claude-sonnet-5-5 --variant plain
python3 tools/ai_test.py --model claude-sonnet-5-5 --only huge
python3 tools/summarize.py && python3 tools/huge_table.py
```

Versions: FCEUmm e0f3086, Genesis Plus GX 60bee4c, cc65 eebf54a, sjasmplus v1.24.1, Claude Code 2.1.330,
claude-sonnet-5-5 and claude-opus-5-5. 125 Claude calls cost USD 4.20 at API prices (paid by subscription), median
15 s per call.

## Limits

- Emulators only. FCEUmm showed the damage of late NES writes; Genesis Plus GX stored late Master System writes
  correctly, so for the Master System the result is the timing alone (on the console, data-port writes during the
  active display closer than about 26 cycles get lost, see the smsvdp post).
- The "plain" prompt still says the routine runs from the NMI or VBlank interrupt with rendering on, which is itself a
  strong hint. A request from inside a C game loop, with no hardware context, is a different test.
- Single routines with exact specs. In a game the VBlank budget is shared with music, input and sprite updates.
- 3 reps per model, task, CPU and prompt. With 96 of 96 passing, the test cannot rank the models.
- The checker counts a routine that alternates halves over two frames as correct when the whole destination is right
  after consecutive frames. Whether that is acceptable depends on the game (the first frame shows half an update).

MIT.
