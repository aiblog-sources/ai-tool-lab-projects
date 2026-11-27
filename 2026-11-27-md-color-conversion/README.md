# mdcolor: SNES to Mega Drive color conversion, measured

**Finding:** SGDK's rescomp turns colors into Mega Drive colors by keeping the top 3 bits of each channel
(`v >> 5`), confirmed on all 256 grey values in a built ROM. Over all 32,768 SNES colors that costs a mean
CIEDE2000 error of 4.13; picking the nearest of the 512 Mega Drive colors in CIELAB instead gives 3.58 and halves
the worst case (20.6 to 10.7). On 18 SNES images the CIELAB pick had the lowest error on every one (median 6.36 vs
7.75), but it merges more shades of a ramp, and the right answer depends on which Mega Drive levels your display
shows. SGDK's other converter, the `RGB24_TO_VDPCOLOR` macro for colors in code, rounds instead (`(v + 16) >> 5`),
disagrees with rescomp on 112 of 256 values and does worst of all (mean 6.69, every color pushed lighter by 6.4 L*
on average). Claude Sonnet, Opus and Haiku 5.5 wrote 18 converters with the CRAM bit layout right every time, all
per-channel nearest-level, none perceptual.

Write-up: https://aitoollabs.dev/posts/snes-to-mega-drive-colors/

## Results

All 32,768 SNES colors (`results/all-snes.txt`), SNES 5-bit channels expanded as `(c << 3) | (c >> 2)`, error =
CIEDE2000 between the SNES color and the Mega Drive color as shown:

| Method | Shown with linear levels: mean / p95 / max | Shown with the non-linear table: mean / p95 / max |
| --- | --- | --- |
| trunc (rescomp) | 4.13 / 8.46 / 20.61 | 4.31 / 9.12 / 21.72 |
| macro (`RGB24_TO_VDPCOLOR`) | 6.69 / 11.56 / 24.00 | 6.80 / 12.11 / 25.65 |
| round (nearest, linear levels) | 4.11 / 9.07 / 23.28 | 5.05 / 10.46 / 24.03 |
| round-table (nearest, table levels) | 4.48 / 9.50 / 24.01 | 4.13 / 8.91 / 23.70 |
| lab, tuned for linear | **3.58 / 6.74 / 10.73** | 4.42 / 8.27 / 15.36 |
| lab, tuned for the table | 4.43 / 8.47 / 14.45 | **3.58 / 6.57 / 13.81** |

Linear levels = `k * 255 / 7` (0, 36, 73, 109, 146, 182, 219, 255). Table = 0, 52, 87, 116, 144, 172, 206, 255,
the non-linear levels credited to hardware measurements by TmEE and Eke (cited, not measured here).

18 SNES images (the SNES backgrounds and sprite sheets Claude drew for the
[2026-10-30 pixcheck test](../2026-10-30-pixel-art-limits-checker/), snapped to SNES colors), `results/corpus-summary.md`:

| Shown with | Method | Median of mean dE00 | Worst pixel | Lowest error on | Shades merged (images under 60 colors) |
| --- | --- | --- | --- | --- | --- |
| linear | trunc | 7.75 | 19.2 | 0 of 18 | 20 of 210 |
| linear | macro | 9.65 | 21.6 | 0 of 18 | 26 of 210 |
| linear | round | 8.25 | 21.0 | 0 of 18 | 27 of 210 |
| linear | lab | **6.36** | 10.6 | 18 of 18 | 32 of 210 |
| linear | distinct | 7.33 | 33.4 | 0 (ties lab where nothing merges) | 0 of 210 |
| table | trunc | 7.92 | 18.2 | 0 of 18 | 20 of 210 |
| table | lab | **5.90** | 11.8 | 18 of 18 | 35 of 210 |

Fit after conversion (pixcheck: 4 palettes of 15 + backdrop): trunc 17 of 18, round 18, lab 17, distinct 11. One
4-palette landscape stops packing after trunc and lab. `distinct` (never merge two source colors, over the whole
image) breaks packing on 7 images.

Test card in a ROM (four 15-shade SNES ramps, one per palette; SGDK 2.09, Genesis Plus GX 60bee4c; levels read
back from the ROM's palette data and from the emulator screenshot, 0 mismatches with the prediction in 240 swatches):

| Method | Shades left (grey / sky / skin / foliage, of 15) | Mean dE00 skin | Worst hue shift, skin | Grey shades turned black |
| --- | --- | --- | --- | --- |
| SNES colors straight into rescomp (trunc) | 8 / 11 / 12 / 10 | 6.43 | 34.2 deg | 2 |
| round | 8 / 10 / 12 / 10 | 7.36 | 44.0 deg | 1 |
| lab | 8 / 6 / 9 / 11 | 5.48 | 24.8 deg | 1 |
| distinct (per ramp) | 15 / 15 / 15 / 15 | 6.36 | 26.3 deg | 1 |

Full images through the ROM: two corpus images x (trunc, lab), exported with pixcheck, built with rescomp and
screenshotted in Genesis Plus GX: 0 of 57,344 pixels differ from the prediction in all four (`results/roundtrip/`).

Claude writing the converter (`ai-test/`, `results/ai-test.json`): 3 models x 2 tasks (SNES BGR555 word, 24-bit
RGB) x 3 reps = 18 `claude -p` calls, USD 0.38 at API prices, 255 s in all. Every function used only the CRAM bits
(`0x0EEE`) with red in bits 1-3. Rule found by calling each on every channel value: nearest level on the linear grid
15 of 18 (5 of them break the two exact ties at 164 and 237 downward), nearest on the non-linear table 3 of 18
(Sonnet SNES 1, Opus SNES 1, Opus RGB 1). None truncated like rescomp, none compared colors perceptually.

## Run it

Needs Python 3 with numpy, Pillow and scipy (scipy only for `distinct`).

```bash
python3 mdcolor.py convert art.png art-md.png --method lab                 # nearest in CIELAB, linear levels
python3 mdcolor.py convert art.png art-md.png --method lab --display table # if your target shows the table levels
python3 mdcolor.py report art.png --snap-snes                              # compare every method on your image
python3 mdcolor.py all-snes                                                # the 32,768-color table above
python3 tests/test_mdcolor.py                                              # CIEDE2000 test pairs, CRAM layout, rescomp match
```

`convert` writes each Mega Drive level as `k * 255 / 7`, which rescomp's truncation maps back to `k`, so the PNG
can go to rescomp unchanged (as an indexed PNG laid out for 4 palettes; the
[pixcheck](../2026-10-30-pixel-art-limits-checker/) `md --export` option does that packing). A 159-color 256x224
image converts in 0.3 s.

To repeat the ROM checks (Docker, the `ghcr.io/stephane-d/sgdk` 2.09 image, and mdprof from
[2026-10-28](../2026-10-28-sgdk-frame-profiler/) built under `~/.cache/mdprof`):

```bash
python3 tools/probe_rescomp.py   # 256 grey values through rescomp: results/rescomp-probe.csv
python3 tools/probe_macro.py     # 256 grey values through RGB24_TO_VDPCOLOR: results/macro-probe.csv
python3 tools/rom_card.py        # the ramp test card four ways: results/rom-card.json, results/shots/
tools/roundtrip.sh               # two corpus images x trunc/lab, pixel compare
python3 tools/fit_md.py          # pixcheck fit for every image and method
python3 tools/ai_test.py         # the Claude test (needs Claude Code logged in and the pixcheck-runner image)
```

## What rescomp does

`convertRGBA8888toRGBA4444` in SGDK's `tools/commons/src/sgdk/tool/ImageUtil.java` keeps the top 4 bits of each
channel and `Palette.java` masks them with `0x0EEE`, so each 3-bit level is `v >> 5`: 0-31 is level 0, 32-63 level
1, up to 224-255 level 7. Same code in the 2.09 Docker image and on SGDK master (last change to that file
2025-01-25). For SNES colors that means level = `c5 >> 2`, so SNES 1-3 become black.

The `RGB24_TO_VDPCOLOR(color)` macro in SGDK's `inc/pal.h` (same on master) is a different rule: it adds `0x10` to
each channel (capped at `0xFF`) before keeping the top 3 bits. `tools/probe_macro.py` compiles it for all 256 grey
values into a ROM and reads the words back: level = `min(v + 16, 255) >> 5` on 256 of 256, different from rescomp on
112. It rounds to a grid 32 apart while the levels sit 36.4 apart, so most colors come out lighter.

## Files

- `mdcolor.py`: the converter and the measurements (sRGB to CIELAB D65, CIEDE2000).
- `tools/testcard.py`: the four-ramp test card. `tools/probe_rescomp.py`, `tools/rom_card.py`, `tools/roundtrip.sh`:
  ROM checks. `tools/fit_md.py`: pixcheck fit. `tools/summarize.py`: corpus table. `tools/ai_test.py`,
  `tools/ai_eval.py`: the Claude test.
- `corpus/`: the 18 SNES images (code-drawn by Claude for the 2026-10-30 project, MIT like the rest of this repo).
- `results/`: every number above. `results/shots/`: emulator screenshots of the test card (Genesis Plus GX's RGB565
  output, so level 7 shows as 238, not 255) and the SNES source rendered directly.

## Limits

- The 18 images are code-drawn by an AI, not hand-made SNES art, and 877 of their 1,250 colors had to be snapped to
  the SNES grid first.
- CIEDE2000 on single colors is a proxy for how a picture looks; it does not see dithering or neighbors.
- The non-linear level table is quoted, not measured; I have no console or capture card.
- No dithering was tested.

MIT license.
