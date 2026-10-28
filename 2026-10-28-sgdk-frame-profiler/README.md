# mdprof: a headless frame-budget profiler for SGDK games

**Finding:** run a Mega Drive ROM headless in Genesis Plus GX with its CPU hook on, charge every 68000 cycle to a
function from SGDK's `symbol.txt`, and you get per-frame load, dropped frames and DMA overflow, with the function
that caused each one, at about 1,550 frames per second (26 times real time) on a 2-core laptop. On a test ROM with
known costs it counted the same dropped frames as the ROM's own counters (60 of 60 in the slowdown phase) and
measured a fixed-cost loop to within 3 cycles of its hand count.

Write-up: https://aitoollabs.dev/posts/sgdk-frame-profiler/

## Results (test ROM, 1,500 frames, NTSC)

| Workload in the test ROM | What mdprof measured |
| --- | --- |
| `burn(500)`: `move.w`, 500 `dbra` loops, `rts` (5,038 cycles by hand, about 5,117 with the emulator's DRAM refresh delay) | 5,114.0 cycles per call |
| One 32-bit division in a per-sprite loop (`__divsi3` + `__udivsi3`) | 661 cycles each (slope over 1 to 8 divisions per sprite) |
| The same step with a reciprocal table and a 16-bit multiply | 139 cycles each |
| 24 sprites, 7 divisions each per frame | loop cost 103% of a frame: 30 fps, 30 of 60 frames dropped |
| Redraw a 40x28 plane with 1,120 `VDP_setTileMapXY()` calls | 334,924 cycles (2.6 frames, 2 frames dropped per redraw) |
| The same redraw as one `VDP_fillTileMapRect()` | 54,806 cycles (43% of a frame, no drop) |
| DMA queue of 4,096 to 6,656 bytes per frame | all of it in VBlank |
| DMA queue of 7,168 to 9,728 bytes per frame | 7,135 to 7,143 bytes in VBlank, the rest during active display, ending as late as line 154 |
| Dropped frames per phase, profiler vs the ROM's own `vtimer` and loop counter | 60 vs 60 (divisions), 12 vs 11 (per-tile redraw), 1 vs 0 (light phase, first frame), 0 vs 0 elsewhere |

SGDK's own `platformer` sample (MIT, SGDK v2.10), driven by a scripted pad for 1,200 frames: mean load 12.5%, no
dropped frames, busiest frame 19% when the camera decodes a new map row (`prepareMapDataRowEx_MTI8_BI8`).

These are emulator numbers. Genesis Plus GX models 68000 timings, DMA rates and the VDP FIFO closely, but nothing
here was checked on a real console.

## How it works

- `profiler/mdprof.c` is a small libretro frontend. It loads the Genesis Plus GX core built with `HOOK_CPU=1` (an
  option the core already has) and installs a hook that runs before every 68000 instruction. The hook reads the
  core's master-cycle counter and charges the time since the previous instruction to the function that contains the
  previous program counter.
- Time inside `VDP_waitVBlank` (SGDK's wait for the next frame) is idle; everything else is busy, including the
  vertical interrupt and the DMA flush, where the 68000 is stalled.
- A video frame in which the game never left `VDP_waitVBlank` is a dropped frame: no new game loop started, so the
  screen showed the old frame again.
- Writes to VRAM, CRAM and VSRAM that happen while the core holds the 68000 for a DMA are counted as DMA bytes, split
  into VBlank and active display by the scanline at the time.

No emulator source is changed or redistributed: `profiler/build.sh` clones Genesis Plus GX at a pinned commit and
builds it locally. Genesis Plus GX has its own non-commercial license; this folder (mdprof, the test ROM, the
scripts) is MIT.

## Run it

Needs Docker, git and Python 3. Everything compiles inside containers; nothing is installed on the host.

```bash
WORK=~/.cache/mdprof profiler/build.sh       # Genesis Plus GX core + mdprof (gcc:14 image)
WORK=~/.cache/mdprof ./run.sh                # builds testrom/ with SGDK, profiles 1,500 frames, writes results/
WORK=~/.cache/mdprof ./run-platformer.sh     # SGDK's platformer sample with a scripted pad
```

On your own SGDK project, after `make`:

```bash
docker run --rm -v "$WORK:/w" -v "$PWD/out:/rom" -w /w gcc:14 ./mdprof gpgx/genesis_plus_gx_libretro.so \
  /rom/rom.bin /rom/symbol.txt 1800 --from 60 --input 60-600:R --input 200-210:C --csv /rom/frames.csv
```

Options: `--idle` (wait functions, default `VDP_waitVBlank`), `--from N` (skip boot frames in the summary),
`--input F-T:BUTTONS` (hold pad 1 buttons `UDLRABCS`), `--csv`, `--json`, `--shot N:file.ppm`, `--ram ADDR:name`
(log a RAM word per frame, address from `symbol.txt`), `--nohook` (time the emulator alone).

## Versions

- Genesis Plus GX (libretro), commit `60bee4c724859710ce05672239e5b601ad01382e` (2026-10-23)
- SGDK Docker image `ghcr.io/stephane-d/sgdk:latest`, digest `sha256:b0bb8a71...cf31`; its `genesis.h` says
  `SGDK_VERSION 2.09`; m68k-elf-gcc 13.2.0
- Host compiler: Docker `gcc:14` (GCC 14.4.0)
- Test ROM SHA-256 in `results/rom.sha256`

## Files

- `profiler/mdprof.c`, `profiler/build.sh`: the profiler and its build
- `testrom/src/main.c`: the test ROM, phases A to E, all code and art written for this project
- `run.sh`, `run-platformer.sh`: the two runs; `tools/analyze.py` per-phase tables; `tools/ppm2png.py` screenshots
- `results/`: `report.txt` (mdprof output), `frames.csv` (one row per frame), `summary.json`, `analysis.txt`,
  `nohook.txt`, screenshots of the test ROM, and `platformer/`

Related tool: [md-profiler](https://github.com/Tails8521/md-profiler) (MIT) is a tracing profiler that records
call traces from a modified BlastEm in its debugger and shows them in Perfetto. mdprof is narrower: no debugger, no
window, a frame-budget summary you can run in a script or CI.
