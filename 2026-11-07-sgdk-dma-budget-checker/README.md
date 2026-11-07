# mddma: a DMA budget checker for SGDK ROMs (Mega Drive / Genesis)

**Finding:** on an emulated NTSC Mega Drive in H40 mode, SGDK 2.11 got 6,907 to 6,932 bytes of DMA out in VBlank with 1 KB queue entries (7,149 to 7,168 with one big entry), less than its own 7,200-byte default limit. A 7 KB queue that SGDK accepts without a warning spilled 305 bytes into lines 0 to 17, and the sprite table landed at line 13. At 8 KB the game fell to 35.6 fps. With `DMA_setIgnoreOverCapacity(TRUE)` the frame rate held at 60, but SGDK dropped everything past 7,168 bytes, including the sprite table queued last, so the sprites froze in 87 of 87 frames. mddma runs a ROM headless and reports all of this per frame.

Write-up: https://aitoollabs.dev/posts/sgdk-dma-queue-vblank-limit/

## Results (test ROM, SGDK 2.11, Genesis Plus GX 60bee4c, CPU work about 45% of a frame)

NTSC (60 Hz), tiles queued in 1 KB entries, then the sprite table (64 bytes):

| Queued per frame | Mode | Game loops/s | Sent in VBlank | Sent during display | Last DMA line | Sprite table |
| --- | --- | --- | --- | --- | --- | --- |
| 6 KB | H40, default | 60.0 | 6,208 | 0 | - | in VBlank |
| 7 KB | H40, default | 60.0 | 6,927 | 305 | 17 | line 13 |
| 8 KB | H40, default | 35.6 | 6,912 | 1,344 | 83 | line 71 |
| 10 KB | H40, default | 30.2 | 6,908 | 3,396 | 189 | line 186 |
| 12 KB | H40, default | 30.2 | 6,915 | 4,016 | 223 (runs into the next frame) | next VBlank |
| 8 to 16 KB | H40, ignore over capacity | 60.0 | 6,922 to 6,930 | 238 to 246 | 13 | **never sent** (0 of 87 or 88 frames) |
| 12 KB, sprite table queued first | H40, ignore | 60.0 | 6,208 | 0 | - | in VBlank, 6,144 bytes of tiles dropped per frame |
| 12 KB, half each frame | H40, default | 60.0 | 6,208 | 0 | - | in VBlank |
| 6 KB | H32, default | 60.0 | 5,675 | 533 | 33 | line 29 |
| 8 KB | H32, default | 30.2 | 5,656 | 2,600 | 163 | line 159 |

Queued as **one entry per phase** instead of 1 KB entries (`ONE_ENTRY=1 ./run.sh`, `results-one-entry/`), VBlank carried 7,149 to 7,168 bytes in H40 and 5,822 to 5,844 in H32: each 1 KB entry the 68000 sets up during VBlank costs about 34 bytes. That run also shows an SGDK quirk: when the first queued entry is the one that crosses the limit, `queueIndexLimit` stays 0 (SGDK's "no limit" value), so with ignore over capacity the oversized entry is sent anyway (8 KB: 1,026 bytes into lines 0 to 57) and the sprite table queued after it is dropped in every frame. With the sprite table queued first, the whole 12 KB entry was dropped every frame. Same code in SGDK 2.09, 2.11 and master (`dma.c` last changed 2025-06-05).

PAL (50 Hz): every queue up to 16,448 bytes went out inside VBlank in H40 and H32. SGDK's default PAL limit is 8,000 bytes, so with ignore over capacity it still dropped everything past 7,168 bytes (sprites frozen in 87 of 87 frames) although the hardware time was there.

Other measurements:

- DMA during the display ran at about 17 to 18 bytes per line in H40 and 16 in H32. The 68000 is stopped while it waits, so 1 KB past VBlank costs about 57 lines, a fifth of an NTSC frame: CPU load went from 70.5% at 6 KB to 78.9% at 7 KB.
- SGDK's flush started at line 225 or 226 (VBlank starts at 224), so every line the VBlank code spends before the flush is about 205 bytes of budget gone.

## Claude test (Sonnet 5.5, 5 scenes x 2 prompts, one run each, scripted pad, 1,500 frames)

Steady play is frames 700 to 1,499 (every scene had finished generating its art by then).

| Scene | Prompt | Boot (s) | Steady fps | Largest queue | Spill frames | Load (mean) |
| --- | --- | --- | --- | --- | --- | --- |
| boss 128x128, 8 frames | plain | 8.6 | 60.0 | 264 | 0 | 12.1 |
| | warned | 4.9 | 60.0 | 136 | 0 | 10.6 |
| two 96x112 fighters | plain | 4.7 | 60.0 | 5,568 | 0 | 6.2 |
| | warned | - | **36.0** | 3,776 | 0 | 70.5 |
| title, 640 unique tiles x 4 frames | plain | 10.6 | 60.0 | 0 (CPU writes) | 0 | 4.2 |
| | warned | - | **32.0** | 3,840 | 0 | 59.8 |
| brawler, 5 x 48x64 | plain | 4.4 | 60.0 | 160 | 0 | 8.7 |
| | warned | 1.9 | 60.0 | 1,696 | 0 | 11.1 |
| 2-axis scroller + line parallax | plain | 0.8 | 60.0 | 112 | 0 | 10.7 |
| | warned | 0.9 | 60.0 | 0 (CPU writes) | 0 | 17.3 |

None of the 10 scenes went over the DMA budget. The two that dropped frames during play were both "warned" (steady 60 fps on a real NTSC console): Claude planned the DMA around "~7 KB per NTSC vblank" and then drew art on the CPU every frame (mdprof: `fillRect` 40.7% of cycles in the fighters scene). One fix round with the mddma report: title back to 60 fps, fighters down to 26.0. 14 `claude -p` calls (Claude Code 2.1.319), USD 3.53 at API prices. Full table: `ai-test/summary.txt`.

## How to run it

Needs Docker. Everything compiles inside the `gcc:14` and SGDK images; the emulator source is cloned into `~/.cache/mddma` at a pinned commit.

```sh
./checker/build.sh                      # Genesis Plus GX (HOOK_CPU=1) + mddma
SGDK_DIR=path/to/SGDK-v2.11 ./run.sh    # test ROM, NTSC and PAL runs, results/
SGDK_DIR=... ONE_ENTRY=1 OUT=results-one-entry ./run.sh
python3 tools/ai_test.py                # the Claude test (needs Claude Code logged in)
python3 tools/ai_summary.py             # the table above
```

To check your own ROM (built with SGDK, so `out/symbol.txt` exists), from the folder that holds `mddma` and `gpgx/`:

```sh
./mddma gpgx/genesis_plus_gx_libretro.so out/rom.bin out/symbol.txt 1800 --from 300 --csv frames.csv
./mddma ... --pal          # 50 Hz console
./mddma ... --input 100-400:R --input 120-124:A    # hold pad buttons for frame ranges
```

The report lists the largest queue, frames with DMA during the display, frames whose sprite table or CRAM landed during the display, frames that got fewer bytes than SGDK queued, and the worst frames with where the bytes went (tiles, plane, sat, hscroll, cram, vsram). The CSV has one row per frame.

## Files

- `checker/mddma.c`: the checker (libretro frontend, MIT). `checker/build.sh` builds it.
- `testrom/src/main.c`: the test ROM with its 20 phases.
- `results/`: per-frame CSVs for NTSC and PAL, `analysis.txt`, screenshots, `rom.sha256` (1 KB entries). `results-one-entry/`: the same with one entry per phase. The ROM built with SGDK 2.09 (the Docker image's own) gave the same table as 2.11.
- `tools/ai_test.py`, `tools/ai_summary.py`, `ai-test/`: the Claude test, with every main.c, report and CSV.

## Limits

- Emulator results: Genesis Plus GX models the VDP's DMA slots per line, but none of this was checked on a real console.
- One SGDK version (2.11) and one way of queueing (1 KB entries). SGDK's flush timing decides the real budget, so other versions can differ by a few lines.
- DMA fills and VRAM copies are not counted, only 68000-to-VDP transfers.

MIT license. Genesis Plus GX has its own license (non-commercial); it is cloned at build time, not included here.
