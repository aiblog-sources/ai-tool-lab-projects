# pixcheck: does this pixel art fit the NES, Master System, SNES or Mega Drive?

**Finding:** asked for pixel art as Python code for a named console, without being told to respect its limits,
Claude Sonnet 5.5, Opus 5.5 and Haiku 5.5 broke a hardware limit in 11 of 36 images, all on the NES (6 of 9:
16x16 attribute areas, 3-color sprite tiles, the 256-tile budget) and the SNES (5 of 9: colors that cannot be
packed into 8 palettes of 15). All 18 Master System and Mega Drive images fit. Told to respect the limits, they broke
one in 36. pixcheck finds these in a fraction of a second, and its Mega Drive export went through SGDK's rescomp
and showed up pixel-identical in an emulator for all 12 backgrounds.

Write-up: https://aitoollabs.dev/posts/pixel-art-console-limits-checker/

## Results

72 images (3 tasks x 4 consoles x 3 models x 2 prompt variants), drawn by Python scripts that `claude -p` wrote with
no tools, run in a container with no network, checked with the final pixcheck (`tools/summarize.py`):

| Prompt | Model | First try passes | Fails by console |
| --- | --- | --- | --- |
| Names the console only ("naive") | Sonnet 5.5 | 7 of 12 | NES 3, SNES 2 |
| | Opus 5.5 | 10 of 12 | SNES 2 |
| | Haiku 5.5 | 8 of 12 | NES 3, SNES 1 |
| Adds "respect the console's color, palette and tile limits" | Sonnet 5.5 | 12 of 12 | |
| | Opus 5.5 | 12 of 12 | |
| | Haiku 5.5 | 11 of 12 | NES 1 (258 unique tiles, budget 256) |

Fix rounds on the 12 failures: one plain round ("this breaks the hardware limits, fix it") passed 11; pixcheck's
report as feedback passed 12 (one needed a second round). Median mean-RGB change from the first image: 5.3 (plain)
and 7.7 (pixcheck), out of 255.

Mega Drive checks with SGDK 2.09 (Docker image):
- The 12 Mega Drive backgrounds as the models saved them, dropped into `res/` for rescomp: 4 built (indexed PNGs of
  15 or 16 colors), 8 failed (`expected a plain tile for palette data at position [0,0]`: rescomp reads a palette
  from the top-left tiles of an RGB image).
- The same 12 through `pixcheck md --export` (an indexed PNG with the palettes packed): 12 built, and the emulator
  screenshot matched the source on all 71,680 pixels of each (`results/md-roundtrip.txt`).
- SGDK 2.09's `TILE_USER_LENGTH`, read from the running ROM: 1,425 tiles. pixcheck uses that as the Mega Drive tile
  budget.

Mistakes found in pixcheck while testing it, both fixed: the greedy palette packing called one SNES image
unpackable when an exact search finds a packing (now an exact search runs when the greedy one fails; 16.8 s on that
image, median 0.02 s over all 72), and an NES palette decoded from the NTSC signal flagged the models' standard NES
colors as off-palette (NES colors are now taken as given, or snapped to a `.pal` file you pass).

## What it checks

| | NES | Master System | SNES (mode 1, 4bpp) | Mega Drive |
| --- | --- | --- | --- | --- |
| Color depth | taken as given, or `--nes-palette file.pal` | 2 bits per channel | 5 bits | 3 bits |
| Background palette area | 16x16 (attribute) | 8x8 tile | 8x8 tile | 8x8 tile |
| Colors per area | 3 + shared backdrop | 16 | 15 + backdrop | 15 + backdrop |
| Background palettes | 4 | 2 | 8 | 4 |
| Tile budget (unique 8x8) | 256, no flips | 448, flips | 1024, flips | 1,425 (SGDK default), flips |
| Sprites | 3 colors + transparent per 8x8 tile, 4 palettes | 15 per frame, 1 palette | 15 per frame, 8 palettes | 15 per frame, 4 palettes |
| Sprites on one line | 8 | 8 | 32 | 20 |

It also reports source colors that collapse into one console color, the palettes it packed, and the busiest line of
a sprite frame (8-pixel columns). Palette packing is greedy with restarts, then an exact search (120 s cap) when the
greedy one leaves tiles out, so a FAIL on packing says whether it was proven.

## Run it

Needs Python 3 with Pillow and numpy.

```bash
python3 pixcheck/pixcheck.py nes title.png --overlay title-check.png     # background
python3 pixcheck/pixcheck.py md hero.png --sprite 32x32                  # sprite sheet of 32x32 frames
python3 pixcheck/pixcheck.py md stage.png --export res/stage.png         # indexed PNG for SGDK rescomp
python3 tests/test_pixcheck.py                                           # 14 synthetic cases
```

Exit code 0 when every limit holds, 1 when one breaks. `--json` writes the full report; `--overlay` draws red boxes
on areas with too many colors and orange ones on areas that got no palette.

To repeat the AI test (needs Claude Code logged in, Docker, and a `pixcheck-runner` image built from
`python:3.12-slim` with `pillow==12.0.0 numpy==2.3.4`):

```bash
python3 tools/ai_test.py --variant limits && python3 tools/ai_test.py --variant naive
python3 tools/summarize.py && python3 tools/fix_drift.py
tools/rescomp_raw.sh                 # SGDK image
MDPROF=<folder with mdprof> tools/md_roundtrip_all.sh   # mdprof from ../2026-10-28-sgdk-frame-profiler
```

## Versions

- Claude Code 2.1.314, models `claude-sonnet-5-5`, `claude-opus-5-5`, `claude-haiku-5-5`; 100 calls, USD 7.24 at API
  prices (run on a subscription)
- Python 3.12, Pillow 10.2 on the host for pixcheck; the runner container has Pillow 12.0.0 and numpy 2.3.4
- SGDK Docker image `ghcr.io/stephane-d/sgdk:latest` (`SGDK_VERSION 2.09`), Genesis Plus GX through mdprof

## Files

- `pixcheck/pixcheck.py`: the checker (MIT)
- `tests/test_pixcheck.py`, `tests/cases/`: synthetic images with a known number of violations
- `mdrom/`: the SGDK ROM that shows an exported background, for the round trip
- `tools/`: the AI test, re-check, fix drift, exact packing check, rescomp and round-trip scripts
- `ai-test/<variant>-<task>-<console>-<model>/<stage>/`: every script the models wrote, the image it drew and
  pixcheck's report (`v0` first try, `plain1` plain fix, `check1`/`check2` pixcheck fixes)
- `results/`: `ai-test.json`, `ai-test-summary.md`, `fix-drift.md`, `rescomp-raw.txt`, `md-roundtrip.txt` and
  `md-roundtrip/` (exports, screenshots, comparisons), `timing.txt`, run logs

All art in this folder was drawn by code the models wrote for this test. No commercial art, ROMs or game assets.
