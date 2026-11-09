`check.sh` now reports 60.0 game loops per second, with 0 of 800 frames dropped. Every loop fits in one video frame, and the busiest uses about 17% of one. DMA is unchanged: no DMA during active display, and the largest vblank transfer is still 3776 bytes.

**Cause:** `renderFighter` redrew a full 96×112 fighter in software every 3 frames. With the two fighters staggered, that rendering landed on 2 of every 3 frames. Each render cost far more than a frame's cycle budget, so most loops took 2 video frames.

**Fix, all in `src/main.c`:**
- **Pose cache:** both fighters use the same art, and many animation frames are pixel-identical. The idle bob goes 0,1,2,2,1,0, and punch frame 5 equals idle frame 0. That leaves 8 distinct images, which `renderAllPoses()` draws once at start-up while the display is off. The per-fighter render buffers are gone. Each fighter now streams its chunks from the shared cache.
- **Renderer tweaks:** `fillRect` uses a table for the colour fill instead of a 32-bit multiply, and it works tile by tile.
- **Unchanged:** the VRAM double-buffering, the 3-chunk DMA, the timer offsets, the sprite layout and the input behaviour. I did not compare rendered frames pixel by pixel. I only checked `check.sh`'s numbers.

The pose cache uses 43,008 bytes of RAM, which is about 32 KB more than before. It built and ran under `check.sh`, so it fits, but that is the main memory cost.