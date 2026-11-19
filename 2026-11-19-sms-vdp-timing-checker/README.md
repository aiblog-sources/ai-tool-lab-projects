# smsvdp: a headless checker for Master System VDP writes that come too fast

**Finding:** on a hand-counted test ROM, smsvdp reported every VDP data-port write during active display, with its gap
in Z80 cycles, exactly (240 per frame; 108, 139 and 201 under 23, 26 and 29 cycles). devkitSMS's "safe" SMSlib calls space
their writes 27 to 28 cycles apart, which is safe by the 26-cycle figure from SMS Power testing and too fast by the
29 cycles Sega's manual gives. The `UNSAFE_` calls write every 16 cycles. In 32 devkitSMS programs written by Claude
Sonnet 5.5 and Haiku 5.5, no write came under 26 cycles. 9 of them wrote during active display at SMSlib's 27-28 cycle
pace. One switched the display off to copy its tile map and blanked the top 124 lines of the picture.

| | Sonnet 5.5 | Haiku 5.5 |
| --- | --- | --- |
| Programs that built (first try) | 16/16 | 16/16 |
| Writes under 23 or 26 cycles during active display | 0 | 0 |
| Programs with active-display writes | 3 | 6 |
| ... with writes under 29 cycles (all from SMSlib's 27-28 pace) | 2 | 6 |
| Programs blanking visible lines with display off/on | 1 | 0 |

Validation ROM and SMSlib call timings are in [Results](#results).

Write-up: https://aitoollabs.dev/posts/master-system-vdp-write-timing/

## What it does

While the Master System VDP draws the picture, the CPU only gets into video memory between the VDP's own reads.
Data-port writes (`out ($BE)`, VRAM or CRAM) that come too close together are lost or corrupted on the console, but most
emulators accept them. Published figures for the minimum gap during active display differ: about 22.8 Z80 cycles
(Maxim, SMS Power forum), 26 (sverx's 2014 hardware test, same thread) and 29 (Sega's Mark III manual, quoted by
Bumbershoot Software). In VBlank, in the border and with the display off there is no limit.

`smsvdp` loads the Genesis Plus GX libretro core, runs an `.sms` ROM for N frames with no window and wraps the core's
exported Z80 port handlers (no emulator patch). For every VDP data-port access it records:

- the Z80 cycles since the previous data-port access (the core charges an instruction before it runs, so each write is
  stamped at the end of its instruction; every Z80 `OUT` form ends with its I/O cycle, so gaps are exact),
- the scanline and whether the display was on,
- VRAM or CRAM, and the instruction (`PC - 2`: every OUT to the VDP is a 2-byte instruction), named from an SDCC `.noi`
  symbol file.

It reports active-display writes under 23, 26 and 29 cycles (`--min` picks the headline threshold, default 26), per
frame, per function and per instruction, a gap histogram, VRAM reads that come too soon after the address setup, and
visible lines blanked by switching the display off and on within one frame. A display that stays off for whole frames
(a loading screen) is counted separately as black-screen frames. Exit code 3 means writes under `--min`, 4 means blanked
lines only.

It runs about 6,000 frames in 1.6 s here, container start included.

## Results

### Test ROM with hand-counted gaps (`testrom/main.c`)

Bursts started at fixed V counter lines every frame, interrupts off: OTIR (21 cycles per byte), unrolled OUTI (16),
unrolled `OUT (n),A` (11), OUTI + 2 NOP (24), OUTI + JP NZ (26), OUTI + JR NZ (28), OUTI + NOP + JP NZ (30), OTIR to
CRAM, an OTIR with the display switched off, and an OTIR in VBlank.

| Per frame | Expected | smsvdp (NTSC and PAL) |
| --- | --- | --- |
| Data writes, all | 336 | 336 |
| Active-display writes | 240 | 240 |
| Under 23 / 26 / 29 cycles | 108 / 139 / 201 | 108 / 139 / 201 |
| Lines blanked by the display-off burst | 4 (170-173) | 4.0 |
| Line of the first OTIR burst | 40 | 40-43 |

### SMSlib calls during active display (`libtest/main.c`, `results/smslib-table.md`)

One call per frame at line 64, display on, SMSlib built from source (devkitSMS 561bc2c, SDCC 4.5.0).

| SMSlib call | Writes per call | Smallest gap | Under 26 | Under 29 |
| --- | --- | --- | --- | --- |
| SMS_loadTiles / SMS_VRAMmemcpy / _brief / SMS_VRAMmemset(W) (128 B) | 128 | 28 | 0 | 127 |
| SMS_loadTileMap (32 entries) | 64 | 28 | 0 | 63 |
| SMS_loadTileMapArea (8x4) | 64 | 27 | 0 | 60 |
| SMS_loadTileMapColumn (24) | 48 | 30 | 0 | 0 |
| SMS_setTileatXY x32, SMS_setTile x32, SMS_print | 64 | 27 | 0 | 32 |
| SMS_loadBGPalette / SpritePalette / zeroBGPalette | 16 | 28 | 0 | 15 |
| SMS_setBGPaletteColor x16 | 16 | 208 | 0 | 0 |
| SMS_loadBGPaletteHalfBrightness | 16 | 48 | 0 | 0 |
| SMS_load1bppTiles (4 tiles) | 128 | 128 | 0 | 0 |
| SMS_load2bppTiles (4 tiles) | 128 | 27 | 0 | 127 |
| SMS_copySpritestoSAT (16 sprites) | 49 | 28 | 0 | 46 |
| UNSAFE_SMS_copySpritestoSAT | 49 | 16 | 46 | 46 |
| UNSAFE_SMS_VRAMmemcpy128, UNSAFE_SMS_VRAMmemcpy (128 B) | 128 | 16 | 127 | 127 |

devkitSMS's own `hello_sms` example made 12 writes 27 cycles apart during active display in 300 frames
(`results/hello_sms.txt`).

### Claude's devkitSMS programs (`tools/ai_test.py`, `ai-test/`, `ai-test-haiku/`)

8 scenes that update VRAM while running (score HUD, animated water tile, scroller, palette fade, typewriter dialog,
bullets + star field, a boss animated through 24 tiles every frame, Game of Life redrawing the whole tile map every
frame) x 2 prompts (plain, and "It must work correctly on a real Master System console, not only in emulators"),
SMSlib.h in the prompt, `claude -p` with tools off. 600 frames each, no input, frames 60-599 checked.

| | Sonnet 5.5 | Haiku 5.5 |
| --- | --- | --- |
| Programs that built (first try) | 16/16 | 16/16 |
| Writes under 23 or 26 cycles during active display | 0 | 0 |
| Programs with active-display writes | 3 | 6 |
| ... with writes under 29 cycles (all from SMSlib's 27-28 pace) | 2 | 6 |
| Programs using UNSAFE_ calls (all in VBlank) | 2 | 0 |
| Programs blanking visible lines with display off/on | 1 (life-plain: 124 lines, 32 frames) | 0 |
| Cost at API prices | USD 2.52 (20 calls) | USD 0.33 (16 calls) |

Given the smsvdp report, Sonnet fixed the blanking in one round: it copies 3 rows per VBlank into the hidden name table
and flips. Three other fix rounds were triggered by my first blank-line metric, which counted the loading screen; they
are set aside in `ai-test/*/discarded/` (their cost is in the Sonnet total).

## How to run it

Docker and git are needed; everything else builds in containers or under `~/.cache/smsvdp`.

```sh
sms/setup.sh          # SDCC image, devkitSMS (pinned), SMSlib + crt0 + makesms from source
checker/build.sh      # Genesis Plus GX libretro core (pinned 60bee4c) + smsvdp
./run.sh              # test ROM (NTSC, PAL), SMSlib table, hello_sms -> results/
python3 tools/ai_test.py [--model claude-haiku-5-5 --out ai-test-haiku]   # needs Claude Code logged in
```

Check your own ROM (name it `.sms`; pass the SDCC `.noi` for function names):

```sh
docker run --rm --network none -v ~/.cache/smsvdp:/w -w /w gcc:14 ./smsvdp gpgx/genesis_plus_gx_libretro.so \
  game.sms 1800 --from 60 --sym game.noi --csv frames.csv --sites sites.csv --events events.csv
```

## Limits

- Gaps are measured in an emulator; nothing here was run on a real console, and the checker does not model which
  writes the VDP drops. It flags gaps against the published figures, which disagree.
- Active display is lines 0 to viewport height - 1 with display enabled. Horizontal blanking inside a visible line is
  not treated as free.
- Master System (and SMS2) only; Game Gear ROMs use a different port handler and are refused.
- Bank-switched code is named from the `.noi` by address only.

## Licenses

MIT for this folder's code. Genesis Plus GX (cloned at build time) has its own non-commercial license; devkitSMS's
SMSlib and crt0 carry their own licenses (see its LICENSES.txt).
