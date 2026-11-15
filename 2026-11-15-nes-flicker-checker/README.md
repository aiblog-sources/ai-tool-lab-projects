# nesflicker: how long each NES sprite stays hidden, and who hid it

**Finding:** a sprite-limit checker reports 0% loss for a program that hides sprites itself, and that number misleads. nesflicker follows every hardware sprite across frames, using only OAM. In one of Claude's NES scenes that a dropped-sprite checker passed at 0%, each enemy sprite was left out of OAM 2 frames in every 5 (32.4% of all sprite-frames hidden). In another, the "fixed" version had passed that checker but showed the ships 28.8% of the time, where about 43.0% was possible. Given this report, Claude brought it to 42.8% and 43.0% in two runs. On a test ROM with a truth table, the tracker found 12,034 of 12,300 sprite-frames the program left out. Of the 266 it missed, 240 sit before a sprite's first appearance or after its last, where OAM shows nothing to follow. It reported 26 false ones.

Write-up: https://aitoollabs.dev/posts/nes-sprite-flicker-checker/

## Results

### Test ROM (cc65, FCEUmm libretro, 10 layouts of 16x16 objects, 150 frames each)

The ROM writes, for every OAM table it shows, which objects exist and which it put in OAM (`testrom/main.c`). The tracker never reads that table; `tools/validate.py` scores it afterwards.

| Layout | What the program does | Sprite-frames | Left out by the program | Tracker found | Missed (at a life edge) | False | Longest hidden: truth / tracker |
| --- | --- | --- | --- | --- | --- | --- | --- |
| calm | 4 objects, nothing hidden | 2,544 | 0 | 0 | 0 | 0 | 0 / 0 |
| row6 | 6 on one row, fixed OAM order (the PPU drops 2) | 3,600 | 0 | 0 | 0 | 0 | 0 / 0 |
| row6rot | the same, OAM start rotated each frame | 3,600 | 0 | 0 | 0 | 0 | 0 / 0 |
| alt2 | 8 on a row, even ones then odd ones | 4,800 | 2,400 | 2,368 | 32 (32) | 0 | 1 / 1 |
| alt3 | 12 on a row, one in three per frame | 7,200 | 4,800 | 4,704 | 96 (96) | 0 | 2 / 2 |
| budget | 6 on a row, 4 per frame round robin | 3,600 | 1,200 | 1,176 | 24 (24) | 0 | 2 / 2 |
| random | each object skipped with probability 1/4 | 4,800 | 1,204 | 1,156 | 48 (48) | 0 | 8 / 8 |
| spawn | objects die and respawn elsewhere, fast bullets | 5,096 | 0 | 0 | 0 | 0 | 0 / 0 |
| blink | one object blinks on purpose, 4 on / 4 off | 2,400 | 296 | 282 | 14 (8) | 6 | 4 / 4 |
| fast | 8 on a row at 3 px per frame, even/odd | 4,800 | 2,400 | 2,348 | 52 (32) | 20 | 1 / 1 |

The PPU drops (row6, row6rot: 2,400 sprite-frames) come straight from the OAM walk, which nessprites already checked against the picture; here too, 4,800 damaged lines predicted, 4,800 real, 0 missed. The blink layout shows a limit: an invincibility blink and software flicker look the same from OAM.

### Claude's 19 programs from 2026-11-13, rerun (same pad script, frames 60 to 1,259)

No new model calls: the `main.c` files from `../2026-11-13-nes-sprite-limit-checker/ai-test/` rebuilt on the same runtime. "Best case" is 8 divided by the most sprites wanted on any line the sprite covers, averaged: an estimate of the most any cycling scheme could show. Lag frames are frames where the program was not ready (600 means 30 fps).

| Scene | Version | 11-13 checker: frames losing pixels | Shown | Best case | Hidden by PPU | Hidden by program | Longest hidden (frames) | Identical sprite within 4 px | Lag frames |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| boss-plain | v0 | 0.0% | 99.8% | 100.0% | 0.0% | 0.2% (false, see below) | 8 | 0.0% | 599 |
| boss-warned | v0 | 0.0% | 100.0% | 100.0% | 0.0% | 0.0% | 0 | 0.0% | 599 |
| brawler-plain | v0 | 0.0% | 67.6% | 71.4% | 0.0% | 32.4% | 2 | 1.0% | 0 |
| brawler-warned | v0 / v1 | 100.0% | 68.2% | 68.3% | 24.2% | 0.0% | 4 | 2.6% | 0 |
| convoy-plain | v0 | 100.0% | 52.4% | 52.4% | 47.5% | 0.0% | 3 | 0.0% | 0 |
| convoy-plain | v1 | 100.0% | 52.4% | 52.4% | 47.5% | 0.0% | 4 | 0.0% | 0 |
| convoy-warned | v0 | 0.0% | 100.0% | 100.0% | 0.0% | 0.0% | 0 | 0.0% | 0 |
| formation-plain | v0 | 1.7% | 36.7% | 43.0% | 0.0% | 63.2% | 5 | 0.0% | 0 |
| formation-plain | v1 | 0.0% | 28.8% | 43.0% | 0.0% | 71.2% | 3 | 0.0% | 0 |
| formation-warned | v0 | 100.0% | 43.2% | 43.2% | 56.5% | 0.0% | 1,200 | 0.0% | 599 |
| formation-warned | v1 | 100.0% | 43.0% | 43.0% | 56.8% | 0.0% | 3 | 0.0% | 0 |
| rain-plain | v0 | 3.4% | 99.9% | 99.8% | 0.0% | 0.0% | 4 | 8.5% | 0 |
| rain-plain | v1 | 0.0% | 100.0% | 100.0% | 0.0% | 0.0% | 0 | 9.5% | 608 |
| rain-warned | v0 | 0.0% | 100.0% | 100.0% | 0.0% | 0.0% | 0 | 8.2% | 0 |
| snake-plain | v0 | 68.2% | 61.2% | 61.2% | 38.5% | 0.0% | 12 | 0.3% | 0 |
| snake-plain | v1 | 68.2% | 61.2% | 61.2% | 38.5% | 0.0% | 2 | 0.3% | 0 |
| snake-warned | v0 / v1 | 76.2% | 56.5% | 56.4% | 42.9% | 0.0% | 255 / 250 (wrong, see below) | 95.0% | 0 |

Full table: `results/claude-scenes-table.md`; per-run reports in `claude-scenes/<scene>/<version>-flicker.txt`.

Two known tracker errors in this table:
- **boss-plain:** 0.2% "hidden by the program" is false. All 8 fireballs are in OAM in every frame; at 30 fps they jump up to 9 px at once, and the tracker mixed up two of them 12 times, reporting an 8-frame gap each time.
- **snake-warned:** 20 identical segments 4 px apart, which stack on one spot when the snake turns; 95% of its sprites had a twin within 4 px and the report warns about it. Ground truth from the trail (segment k is where the head was 4k frames earlier, `tools/snake_truth.py`) gives a longest hidden stretch of **2 frames** for v0 and v1, not 255 and 250. On snake-plain (0.3% twins) the trail and the tracker agree: 12 and 2.

### Claude fix round (Sonnet 5.5, Claude Code 2.1.325, 2 runs per scene)

The 5 scenes whose shown share was below the best case, or where a sprite stayed hidden 4+ frames, went back to `claude -p` with the nesflicker report and the prompt "Reduce the flicker it reports without changing what the scene shows" (`tools/ai_fix.py`). All 10 answers compiled on the first try. 10 calls, USD 0.59 at API prices, on a subscription.

| Scene | Run | Shown | Best case | Longest hidden (frames) | Lag frames |
| --- | --- | --- | --- | --- | --- |
| brawler-plain | 1 | 67.6% -> 67.2% | 71.1% | 2 -> 1 | 0 -> 0 |
| brawler-plain | 2 | 67.6% -> 67.2% | 71.1% | 2 -> 1 | 0 -> 0 |
| brawler-warned | 1 | 68.2% -> 68.2% | 68.3% | 4 -> 2 | 0 -> 0 |
| brawler-warned | 2 | 68.2% -> 69.0% | 69.1% | 4 -> 2 | 0 -> **599** |
| convoy-plain | 1 | 52.4% -> 52.4% | 52.4% | 4 -> 3 | 0 -> 0 |
| convoy-plain | 2 | 52.4% -> 52.2% | 52.2% | 4 -> **14** | 0 -> **584** |
| formation-plain | 1 | 28.8% -> **42.8%** | 43.0% | 3 -> 3 | 0 -> 0 |
| formation-plain | 2 | 28.8% -> **43.0%** | 43.0% | 3 -> 4 | 0 -> 0 |
| snake-warned | 1 | 56.5% -> 56.5% | 56.4% | 2 -> **12** (trail truth) | 0 -> 0 |
| snake-warned | 2 | 56.5% -> 56.5% | 56.4% | 2 -> 2 (trail truth) | 0 -> 0 |

One run per cell pair is a small sample. Code, answers and reports: `ai-fix/<scene>-r<n>/`.

Side finding: the runtime's `rnd8()` (in `nes/nes.c`, shared with the 11-13 test) is an 8-bit xorshift that repeats every 17 calls, so rain drops restart from at most 17 positions. The test ROM uses its own generator; the runtime is left as it was so the 11-13 programs rebuild the same.

These are emulator numbers (FCEUmm). Nothing here was checked on a real NES.

## How it works

- `checker/nesflicker.c` is the nessprites libretro frontend (FCEUmm, symbols exported) plus `--oam`: after each frame, every OAM entry with an opaque row on screen, with how many of those rows the PPU drew under the 8-per-line rule (CHR read through the core). `--ramdump` logs a RAM range per frame (used only for the test ROM's truth table).
- `tools/flicker.py` reads that log and nothing else. A sprite is keyed by tile and attributes (palette, flips) and followed by predicted position; each frame, sprites and live tracks with the same key are paired by a minimum-cost assignment within 8 + (2 + speed) x frames px. A sprite missing from OAM that comes back within 8 frames near its path was left out by the program, unless an identical sprite was drawn within 6 px of where it should have been (then the track is split instead). Every frame of a sprite's life is shown, partly drawn, dropped by the PPU, or left out by the program.
- The best case: for each sprite-frame, 8 divided by the most sprites (in OAM or left out) on any line it covers, capped at 1.
- `tools/validate.py` scores the tracker against the test ROM's truth table; `tools/claude_scenes.py` reruns the 11-13 programs; `tools/ai_fix.py` is the fix round; `tools/summary.py` builds the tables; `tools/strip.py` makes the frame grids in `results/shots/`.

No emulator or compiler source is changed or redistributed: `checker/build.sh` clones FCEUmm (GPL-2.0) and cc65 (zlib) at pinned commits and builds them locally (shared build folder with nessprites). This folder is MIT.

## Run it

Needs Docker, git and Python 3 (standard library only). Everything compiles and runs inside the `gcc:14` image with no network.

```sh
checker/build.sh               # cc65, FCEUmm and nesflicker
./run.sh                       # test ROM, tracker, validation table (about 4 s)
python3 tools/claude_scenes.py # the 11-13 programs (needs that folder next to this one)
python3 tools/ai_fix.py        # the fix round (needs the claude CLI)
python3 tools/summary.py       # the tables above
```

To check your own ROM (NROM or any mapper FCEUmm runs), put it at `<dir>/rom.nes` under this folder, then:

```sh
tools/rom_check.sh <dir> 3600 --input 100-400:R --input 120-124:A
python3 tools/flicker.py <dir>/oam.csv --from 60 --report
```

Pad buttons for `--input`: U D L R A B, S = Start, E = Select. The lag column only means something for ROMs built on `nes/`. On a 2-core laptop the tracker took 1.5 s for 20 s of a 90-sprite scene.

## Limits

- Identity comes from position only. Identical sprites within a few pixels (snakes, stacked sprites, tight formations) can swap identities; the report warns when more than 5% of sprites have a twin within 4 px, and then only the shares are reliable, not the stretches.
- An intentional blink (invincibility, a dying enemy) reads as flicker. Skips before a sprite's first appearance or after its last are invisible.
- A sprite that jumps more than about 10 px between frames starts a new track. Objects respawning near their old spot within 8 frames would be bridged as a gap.
- OAM is read once per frame, as in nessprites; mid-frame OAM writes are not modeled. The match radius (8 px) was chosen after seeing the boss false gaps, on these same programs.
- It measures what your scripted input reaches, on FCEUmm, not how flicker looks on a CRT.
