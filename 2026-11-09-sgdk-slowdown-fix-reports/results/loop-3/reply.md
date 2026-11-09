`check.sh` now reports 60.0 game loops per second, with 0 of 800 frames dropped. The slowest game loop uses 84% of a frame (107,608 of 128,005 cycles). DMA is clean too: no DMA during active display, and every queued byte reached the VDP.

The cause was software rendering. Each fighter redrew its whole 96×112 frame in a single game loop. That took more than a full video frame of CPU time, so every loop that rendered missed the next VBlank.

I made three changes, all in `src/main.c`:

- **Faster `fillRect`:** it now fills a tile column as runs of consecutive words. The old version recomputed the tile address for every pixel row. It also uses a colour lookup table instead of a 32-bit multiply, which is slow on a 68000.
- **Rendering split by column:** each fighter now renders one 32-pixel sprite column per tick, right before the DMA chunk that streams it to VRAM. That is the same schedule the existing DMA chunking already used. The visible VRAM slot is still never written.
- **Idle frames cached:** the six idle frames are identical for both fighters, so they are rendered once at start-up into a 32 KB RAM cache, and DMA reads straight from it. Only punch frames are rendered live. This took the idle steady state from over budget to a tiny fraction.

The scene is unchanged: same objects, sizes, animations, DMA structure, timing and pad behaviour, and the pixels are identical. The only cost is about 32 KB of extra RAM, which fit without a linker error.

I didn't compare rendered output pixel by pixel; "pixels are identical" rests on the fill logic being a straight reorganisation of the same rectangles. I also tried a `#pragma GCC optimize("O3")` and an `optimize("O3")` attribute on `fillRect`, and neither changed the cycle counts, so I removed both.