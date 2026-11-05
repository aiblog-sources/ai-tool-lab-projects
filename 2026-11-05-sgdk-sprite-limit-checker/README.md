# mdsprites: a Mega Drive sprite limit checker for SGDK ROMs

**Finding:** the Sega Genesis / Mega Drive sprite limit is two limits per line, 20 sprites and 320 pixels of sprites, and the second one is invisible to the VDP's overflow flag: eleven 32x32 sprites in a row lost one sprite in all 116 frames measured with the flag never set. mdsprites runs a ROM headless and names every sprite lost on every line. Against the picture the emulator really drew, it missed 0 of 40,998 damaged lines on its test ROM and 0 of 48,606 in 19 ROMs written by Claude.

Write-up: https://aitoollabs.dev/posts/sgdk-sprite-limit-checker/

## Results (test ROM, SGDK 2.11, H40, NTSC, 116 frames per layout)

| Layout on one row | Most on a line | Sprites lost per frame | Overflow flag |
| --- | --- | --- | --- |
| 20 x 8x8 | 20 | 0 | off |
| 21 x 8x8 | 20 | 1 | on |
| 20 x 16x16 | 20 | 0 | off |
| 21 x 16x16 | 20 | 1 | on |
| 10 x 32x32 | 10 | 0 | off |
| 11 x 32x32 | 10 | 1 | **off** |
| 8 x 32x32 on screen, 4 off screen first in the list, `SPR_addSprite` flags | 10 | 2 | **off** |
| The same with `SPR_FLAG_AUTO_VISIBILITY` | 8 | 0 | off |
| The same 12 with the off-screen sprites last in the list | 10 | 0 | off |
| 24 x 16x16, fixed order | 20 | 4 (the same 4, in 100% of frames) | on |
| 24 x 16x16, `SPR_setDepth` rotated each frame | 20 | 4 (each sprite in 13.8% to 17.2% of frames) | on |
| 24 x 16x16, two rows 16 pixels apart | 12 | 0 | off |

Other measurements:

- Rotating order costs CPU when the engine does it: median busy cycles per frame went from 14,469 (11.3% of a frame) to 46,925 (36.7%) with 24 `SPR_setDepth` calls. With a hand-filled table, rotating added 192 cycles (34,927 to 35,119), though filling the table by hand every frame is itself dearer.
- In SGDK 2.09, which is what `ghcr.io/stephane-d/sgdk:latest` holds, `SPR_FLAG_AUTO_VISIBILITY` hides a sprite that is only partly over the left or top edge. 2.11 has `(u16)` casts in that test and shows it. Right edge fine in both.
- rescomp `SLOW` and `MAX` use a randomized search: 10 builds of the same source at those levels gave 2 to 7 lost sprites and 438 to 782 lost pixels in one layout. `FAST` and `MEDIUM` gave the same ROM hash in 10 of 10 builds.

## Claude test (Sonnet 5.5, 8 scenes x 2 prompts, one run each, scripted pad, 1,200 frames)

| Scene | Plain prompt: frames that lost sprites | After one fix round with the report | Warned prompt |
| --- | --- | --- | --- |
| rings (32-bullet rings) | 3.3%, worst 15 sprites | 4.5%, worst 1, but p95 CPU load 31.5% to 100% | 0%, spawns gated |
| brawler (11 x 32x48 on one ground line) | 100%, worst 2, flag never set | 66.4%, worst 1 | 0%, by making it a 1,280 px street with 4 enemies on screen |
| snake (30 x 16x16) | 73%, worst 10 | 12.2%, worst 1 | 0%, by refusing moves that would overflow a line |
| formation, convoy, boss, stars, rain | 0% | not needed | 0% (convoy moved to a tile plane) |

22 `claude -p` calls, USD 3.58 at API prices, on a subscription. One run per scene is a small sample. Everything is in `ai-test/`.

These are emulator numbers from Genesis Plus GX. Nothing here was checked on a real console or a second emulator.

## How it works

- `checker/mdsprites.c` is a small libretro frontend. After each frame it walks the sprite list for every line as the core's own `parse_satb_m5` and `render_obj_m5` do: the link chain from entry 0, the count limit, the dot limit, masking at X = 0. Off-screen sprites count.
- To check itself it runs the ROM twice: once with the core's "no sprite limit" option, saving every frame, and once with limits on. A line whose pixels differ really lost something. Lines it predicts with no visible difference are normal: the lost pixels were transparent or behind another sprite (2,202 lines on the test ROM, all in the layouts with overlapping discs).
- A hook counts writes to the sprite table during active display. In those frames (raster tricks, late DMA) the once-per-frame walk is an estimate, and the report says so.
- `tools/explain.py` turns the CSVs into a short report for a person or a model.

No emulator source is changed or redistributed: `checker/build.sh` clones Genesis Plus GX at a pinned commit and builds it locally. Genesis Plus GX has its own non-commercial license; this folder is MIT. The SGDK samples used in `run-samples.sh` are fetched from SGDK and are MIT.

## Run it

Needs Docker, git and Python 3. Everything compiles inside containers; nothing is installed on the host.

```sh
checker/build.sh                      # core + mdsprites, in ~/.cache/mdsprites
./run.sh                              # test ROM on SGDK 2.09 (the image), into results/

git clone --depth 1 --branch v2.11 https://github.com/Stephane-D/SGDK ~/.cache/mdsprites/sgdk-v2.11
SGDK_DIR=~/.cache/mdsprites/sgdk-v2.11 OUT=$PWD/results-v2.11 ./run.sh
./run-samples.sh                      # SGDK's sonic and platformer samples
python3 tools/ai_test.py              # the Claude test (needs the claude CLI)
OPT_LEVEL=MAX tools/rebuild_test.sh 5 # same source, five builds
```

To check your own ROM, copy its build folder (`out/rom.bin`, `out/symbol.txt`) under `~/.cache/mdsprites/<name>/`, then:

```sh
tools/rom_check.sh <name> 3600 --input 100-400:R --input 120-124:A
python3 tools/explain.py ~/.cache/mdsprites/<name>/frames.csv ~/.cache/mdsprites/<name>/events.csv
```

SGDK 2.11 runs here as its headers, library and rescomp mounted over the 2.09 image, so the compiler is the image's GCC 13.2.

## Limits

- It finds what your scripted input reaches. A clean run is not a proof that no line can overflow.
- H40 and H32 only; interlace mode 2 is not modeled.
- Sprite tiles are not read, so it cannot tell if lost pixels were transparent. The `--truth` comparison can.
