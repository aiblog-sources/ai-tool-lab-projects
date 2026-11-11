`./check.sh` now reports 60.0 game loops per second, with 0 of 800 frames dropped.

**What was wrong:** each fighter re-rendered its whole 96×112 frame in software (about 107k cycles) on a single video frame. That is nearly a full 128k-cycle NTSC frame, so those loops missed the next VBlank and the frame was shown twice.

**What I changed in `src/main.c`:**
- **Column-by-column rendering:** `renderFighter` now draws one 32-pixel column per tick, in the same tick that column is DMA'd. Chunk timing and the 3-tick animation step are unchanged.
- **Faster `fillRect`:** it now writes the contiguous runs inside each sprite row, which removes the per-row table lookups and multiplies.
- **Idle-frame cache:** the 6 idle frames are rendered once at startup (display off) and DMA'd straight from RAM. Both fighters' idle art is identical, so only P1's punch frames are rendered at runtime.

Objects, sizes, animations, input handling and sprite layout are unchanged. The peak DMA queue is still 3776 bytes, with no DMA during active display.

**Result:** mean CPU load is 19.6%. The worst loop is 104,522 cycles, 82% of a frame. The cheapest loop is about 20k.

The cache costs about 32 KB of RAM (6 × 5376 bytes). The build and run worked, so it fit. I didn't compare the pixels with the original renderer's output, so I haven't confirmed the art is pixel-identical.