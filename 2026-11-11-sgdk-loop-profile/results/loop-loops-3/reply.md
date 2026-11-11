`check.sh` now reports **60.0 game loops per second**, with 0 of 800 frames dropped. The slowest loop uses about 17% of a frame, and the DMA checks are clean. It started at 36 loops per second.

**Cause:** each fighter re-rendered its whole 96×112 sprite with the software renderer every 3 frames, about 100k+ cycles per render. Combined with the DMA queue flush, that overran the 128k-cycle frame.

**Fix, all in `src/main.c`:**
- **Cached images:** the art has only 7 distinct images: idle bob 0, 1 and 2, punch frames 0, 1, 2/3 and 4, and punch frame 5, which is identical to idle bob 0. They are rendered once at startup while the display is off. Both fighters share them, and DMA reads straight from the cache. This replaces the two per-fighter render buffers with a single 37.6 KB cache, and it removed the render cost from the game loop entirely.
- **Faster renderer:** `fillRect` was rewritten with per-column clipping and cheaper address math, plus inline 68000 assembly for the fill and mask loops. This only affects the startup render, since nothing is rendered per frame now.

Animation timing, the one-tick offset between fighters, DMA chunking and the punch latency are unchanged. I did not compare screenshots or pixels against the original. The cached images come from the same drawing code, with the 7-image merge reasoned from the pose tables. I did not look at the rendered output.

**Startup:** the scene now renders those images before the display turns on, so the black screen at boot is slightly longer. I did not measure how much longer.