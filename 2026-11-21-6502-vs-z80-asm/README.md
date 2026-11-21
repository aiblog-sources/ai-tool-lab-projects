# Can Claude write 6502 and Z80 assembly? 120 routines tested on emulators

**Finding:** I asked Claude Sonnet 5.5 and Opus 5.5 for 10 small routines in 6502 assembly (NES, ca65) and Z80
assembly (Master System, sjasmplus), 3 times each: 120 routines. 117 of them passed every test vector on the first
try. The three misses were one broken multiply loop (Sonnet, 6502) and two Z80 files where a `JR` jump was out of
range (Opus). One fix round with the emulator's report fixed all three. The cycle counts Claude claimed matched the
measured worst case exactly in 37 of 48 routines, and all 48 were within 10%. Twice the test was wrong, not the
routine. Given a report from a wrong checker, Claude "fixed" 12 of 12 correct routines to match it.

Write-up: https://aitoollabs.dev/posts/claude-6502-z80-assembly/

| Model | CPU | First try | After 1 fix | Assembled first try |
| --- | --- | --- | --- | --- |
| Opus 5.5 | 6502 (NES) | 30/30 | 30/30 | 30/30 |
| Opus 5.5 | Z80 (Master System) | 28/30 | 30/30 | 28/30 |
| Sonnet 5.5 | 6502 (NES) | 29/30 | 30/30 | 30/30 |
| Sonnet 5.5 | Z80 (Master System) | 30/30 | 30/30 | 30/30 |

Full tables (per task, cycle claims, speed, cost): [results/summary.md](results/summary.md).

## What it does

- `runner/nesrun.c` loads the FCEUmm libretro core (with the cycle counter and per-instruction hook from
  [2026-11-17-nes-cc65-profiler](../2026-11-17-nes-cc65-profiler)) and runs a harness ROM whose main loop is
  `JSR routine`. Before each call it loads one test vector into A, X, Y, P, zero page `$00-$0F` and a 64-byte buffer at
  `$0300`; after the call it records the registers, that memory, the cycles (JSR and RTS included), the stack pointer,
  and any write to other RAM.
- `runner/smsrun.c` does the same for the Z80 on Genesis Plus GX in Master System mode, with no emulator patch: it wraps
  the core's exported `z80_writeport` (as smsvdp did in
  [2026-11-19-sms-vdp-timing-checker](../2026-11-19-sms-vdp-timing-checker)) and uses two `OUT` instructions around the
  `CALL` as markers. Inputs go in A F B C D E H L, `$C000-$C00F` and a buffer at `$C100`.
- `tools/tasks.py` holds the 10 specs, the seeded test vectors (every input for 8- and 16-bit single-input tasks,
  thousands of random and edge cases for the rest; 1,024 to 65,536 per task) and the expected results.
- `tools/asmrun.py` assembles one routine into its harness, runs it, and checks every vector.
- `tools/ai_test.py` asks `claude -p` (tools off, no CLAUDE.md, no settings) for each routine, runs it, and gives it up to
  two fix rounds with the report.
- `tools/selftest.py` checks the checker; `tools/recheck.py` re-checks saved routines after a checker fix, without new
  Claude calls; `tools/summarize.py` writes `results/summary.md`.

The ten routines: popcount, 8x8 multiply, 16/8 divide, 16-bit to 5 decimal digits, 16-bit integer square root, adding
a signed 8-bit velocity to a 16-bit position, signed 16-bit compare (carry out), memmove with overlap, byte sort, and a
sprite box overlap test (carry out).

## Validation

- Branch-free routines with hand-counted costs (`validate/`): 75 cycles on the 6502, 155 on the Z80. Both runners
  measured exactly that on all 1,024 vectors.
- A routine that hangs on one input: 4 timeouts, the core reset, and the run went on. A routine that writes outside its
  RAM: flagged. RAM the routine must not touch is filled with a pattern that changes per vector, so a write of the same
  value still shows; in the Z80 test, 4 of 1,024 writes happened to store the fill value and were missed as stray writes
  (they still failed as wrong answers).
- `tools/selftest.py`: for every task and CPU, an ideal output record passes and one with a flipped output bit fails.

## My checker was wrong twice

1. **box_overlap.** The spec says "a box with width or height 0 overlaps nothing" and gives four inequalities. My first
   expected function used only the inequalities, which call a 0-width box inside a wider one overlapping. All 12 first
   tries (both models, both CPUs) failed on exactly the same 63 vectors, all with a 0 size. They followed the sentence;
   my test followed the formula. Given the report, all 12 fix rounds removed the zero-size rule to match the test. Two replies
   (Sonnet, Z80) said the tests disagreed with the task text and followed the tests anyway; the other ten called their
   own zero-size check the bug. Against the corrected checker, all
   12 "fixed" versions fail those 63 vectors ([results/box-overlap-superseded-fixes.txt](results/box-overlap-superseded-fixes.txt)).
   The first tries pass. The superseded rounds are kept under `ai-test/*/box_overlap-*/superseded/`.
2. **sort_u8 on the Z80.** All three Opus first tries built a 256-byte counting table on the stack, below `$DF00`. The
   prompt allowed the stack, but my runner treated only `$DF00-$DFEF` as stack and flagged the writes. The window is now
   `$D000-$DFEF`, and those routines pass. Opus's fix rounds (an in-place insertion sort) moved to `superseded/`.

`results/recheck.log` is the re-check of all 120 routines with the final checker.

## How to run it

Needs Docker (the `gcc:14` image), Python 3 and, for the AI test, Claude Code. Everything compiles and runs inside the
container with no network.

```sh
../2026-11-17-nes-cc65-profiler/checker/build.sh       # cc65 + FCEUmm with the nesprof patch
../2026-11-19-sms-vdp-timing-checker/checker/build.sh  # Genesis Plus GX
runner/build.sh                                        # nesrun, smsrun, sjasmplus v1.24.1
./run.sh                                               # validation + checker self-test
python3 tools/asmrun.py 6502 mul8 my_mul8.s            # test your own routine
python3 tools/ai_test.py --model claude-sonnet-5-5     # the Claude runs (3 reps per task and CPU)
python3 tools/summarize.py
```

Versions: FCEUmm e0f3086, Genesis Plus GX 60bee4c, cc65 eebf54a, sjasmplus v1.24.1 (ccc7c17), Claude Code 2.1.330,
claude-sonnet-5-5 and claude-opus-5-5. Cost of the Claude runs at API prices (paid by subscription): USD 6.50 for 138
calls, superseded rounds included.

## Limits

- Ten textbook routines with exact specs, not game code. Earlier posts on this site found Claude's mistakes in
  hardware-facing code (DMA, sprites, VDP timing); this test does not touch hardware.
- 3 reps per model, task and CPU. With 117 of 120 passing, it cannot rank the models or the CPUs.
- Emulators only (FCEUmm and Genesis Plus GX), no real console. Cycle counts are the emulators' (both count CPU cycles
  per instruction; neither run has DMA or interrupts during the calls).
- For tasks with sampled vectors (divide, compare, memmove, sort, box test, velocity), the measured "worst case" is the
  worst case seen, so the cycle-claim comparison uses only the four tasks where every input was tested.

MIT.
