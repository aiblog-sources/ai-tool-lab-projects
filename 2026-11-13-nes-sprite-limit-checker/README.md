# nessprites: an NES sprites-per-scanline checker

**Finding:** the NES draws 8 sprites per scanline and drops the rest in OAM order, and its sprite overflow flag cannot be trusted to tell you: on a test ROM, Mesen's flag stayed clear while a ninth sprite really vanished (119 of 119 frames) and came on with nothing lost (119 of 119). nessprites runs a ROM headless, names every sprite lost on every line, and models the flag with its hardware bug; that model matched Mesen in 1,360 of 1,360 frames. Against the picture FCEUmm really drew, it missed 0 of 7,923 damaged lines on the test ROM and 0 of 190,788 in 19 ROMs written by Claude.

Write-up: https://aitoollabs.dev/posts/nes-8-sprites-per-scanline-checker/

## Results (test ROM, cc65, FCEUmm and Mesen libretro cores, 120 frames per layout)

| Layout | Sprites lost per frame | Exact overflow | Real PPU flag (Mesen, and the model) |
| --- | --- | --- | --- |
| 8 sprites on one row | 0 | no | clear |
| 9 on one row | 1 (the last in OAM, every frame) | yes | set |
| 12 on one row | 4 (the same 4, every frame) | yes | set |
| 12, OAM start rotated by 1 each frame | 4, each object gone 33.3% of frames, **4 frames in a row** | yes | set |
| 12, OAM start rotated by 5 each frame | 4, each object gone 33.3% of frames, **never 2 in a row** | yes | set |
| 10 x 8x16 on one row | 2 | yes | set |
| 56 unused entries left at Y = 0 (cleared OAM), 8 real sprites near the top | all 8 real sprites, on 5 of their 8 lines | yes | set |
| 8 on a row + a 10th entry on the row behind one that is off it | 1 | yes | **clear** (false negative) |
| 8 on a row, a later entry's tile number equals the row's Y | 0 | no | **set** (false positive) |
| 24 in 3 rows of 8, 16 lines apart | 0 | no | clear |

Full table with line counts: `results/testrom-table.md`. Per-frame data: `results/testrom-frames.csv` (FCEUmm) and `results/testrom-mesen-frames.csv`. Screenshots: `results/shots/`.

Side finding while building the test ROM: the false negative layout first came out as a *true* positive, because hidden entries still held an X byte of 100 from an earlier layout, and after the eighth hit the PPU compares X bytes as if they were Y. Hiding sprites by writing only Y = $FF leaves the flag's answer to whatever is in the other three bytes. `oam_hide_all()` now writes $FF to all 256 bytes.

## Claude test (Sonnet 5.5, 6 scenes x 2 prompts, one run each, scripted pad, 1,200 frames)

The plain prompt describes the scene; the warned prompt adds "It must look right on a real console, with no sprites dropping out or flickering." Scenes that lost sprites got one fix round with the checker's report.

| Scene | Prompt | What Claude did | Frames really losing pixels | Most on a line | Lag frames (of 1,200) |
| --- | --- | --- | --- | --- | --- |
| formation (2 x 10 ships, 16x16) | plain | software flicker, 3 groups (20 Hz) | 1.7%, then 0% after the fix (4 groups) | 9, then 7 | 0 |
| formation | warned | kept 10 per row, noted "fits only 4 per row" | 100%, still 100% after the fix (now rotated) | 21 | 600, then 0 |
| brawler (6 x 16x32 on one line) | plain | software flicker, 3 of 5 enemies per frame | 0% | 8 | 0 |
| brawler | warned | OAM rotation | 100%, 99.8% after the fix | 12 | 0 |
| convoy (10 x 16x8 on one row) | plain | OAM rotation | 100%, 100% after the fix | 22 | 0 |
| convoy | warned | moved the cars 96 px apart on a 960 px road | 0% | 8 | 0 |
| boss (48x48 + 8 orbiting) | plain | budgeted 6 + 2 per line | 0% | 8 | **600** |
| boss | warned | budgeted 6 + 2 per line | 0% | 8 | **600** |
| rain (40 drops + umbrella) | plain | OAM rotation | 3.2%, then 0% after the fix | 12, then 9 | 0, then **609** |
| rain | warned | 4 layers of 10 drops, 24 lines apart | 0% | 8 | 0 |
| snake (20 x 8x8) | plain | OAM rotation | 68.2%, 68.2% after the fix | 20 | 0 |
| snake | warned | OAM rotation | 55.8%, 56.5% after the fix | 20 | 0 |

19 `claude -p` calls (Claude Code 2.1.323), USD 1.11 at API prices, on a subscription. All 19 programs compiled on the first try. 600 lag frames means the game ran at 30 frames per second. One run per cell is a small sample. Everything is in `ai-test/` (code, reports, per-frame CSVs, screenshots, `table.json`).

The checker only sees sprites the PPU drops. When a program skips objects itself (brawler plain, formation plain), they flicker but the checker reports 0%: read the code column too. The "longest run invisible" heuristic in `tools/explain.py` chains lost sprites by position, so it overstates when objects overlap (snake).

These are emulator numbers. Nothing here was checked on a real NES.

## How it works

- `checker/nessprites.c` is a small libretro frontend. After each frame it reads FCEUmm's own OAM (`SPRAM`), PPUCTRL and PPUMASK and evaluates all 64 entries for every line: in range if `line - Y` is 0..7 (0..15 in 8x16), drawn on the next line, first 8 kept. A lost sprite counts as visible only if its pattern row (read through FCEUmm's CHR pages) has an opaque pixel on screen.
- It also runs the PPU's evaluation with the overflow bug as the [NESdev wiki](https://www.nesdev.org/wiki/PPU_sprite_evaluation) describes it: after 8 hits, every miss steps both the entry and the byte within it.
- To check itself it runs the ROM with FCEUmm's "no sprite limit" option, saves the 256x240 indexed picture of every frame, then compares. A line whose pixels differ really lost something.
- FCEUmm sets the overflow flag exactly (no bug). Mesen implements the bug, so `run.sh` also runs the test ROM on Mesen, where nessprites reads only RAM: the ROM stores bit 5 of $2002 at the start of each NMI. The two runs are aligned by the ROM's NMI counter.
- `nes/` is a tiny runtime for cc65 C programs (NROM, 8 KB CHR generated by `make_chr.py`): the NMI copies OAM by DMA only after `oam_ready()`, so a half-built table is never shown, and counts lag frames when the program was not ready.
- `tools/explain.py` turns the CSVs into the short report that was given to Claude.

No emulator or compiler source is changed or redistributed: `checker/build.sh` clones FCEUmm (GPL-2.0), Mesen (GPL-3.0) and cc65 (zlib) at pinned commits and builds them locally. This folder is MIT.

## Run it

Needs Docker, git and Python 3. Everything compiles and runs inside the `gcc:14` image with no network; nothing is installed on the host.

```sh
checker/build.sh          # cc65, FCEUmm, Mesen and nessprites in ~/.cache/nessprites
./run.sh                  # test ROM on FCEUmm (checked against the picture) and Mesen (flag)
python3 tools/ai_test.py  # the Claude test (needs the claude CLI)
python3 tools/summary.py  # the table above, from ai-test/
```

To check your own ROM (NROM or any mapper FCEUmm runs), put it at `<dir>/rom.nes` under this folder, then:

```sh
tools/rom_check.sh <dir> 3600 --input 100-400:R --input 120-124:A
python3 tools/explain.py <dir>/frames.csv <dir>/events.csv
```

Pad buttons for `--input`: U D L R A B, S = Start, E = Select. `--flagaddr` and the lag column only mean something for ROMs built on `nes/`.

## Limits

- It finds what your scripted input reaches. A clean run is not a proof that no line can overflow.
- OAM is read once per frame. A ROM that writes OAM during rendering (mid-frame DMA, $2004 writes) breaks that assumption; the picture comparison will show it as missed lines.
- PAL timing, the sprite 0 hit and per-dot OAM evaluation details are not modeled; only what decides which sprites are drawn.
