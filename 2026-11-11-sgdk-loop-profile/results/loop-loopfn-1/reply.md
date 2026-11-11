`./check.sh` now reports 60.0 game loops per second, with 0 of 800 frames dropped. The worst loop takes 69% of a frame's cycle budget.

I changed three things in `src/main.c`. The scene's objects, sizes, animations and controls are unchanged.

- **Rendering:**
  - `fillRect` now fills tile by tile instead of row by row.
  - Fill colours come from a lookup table instead of a 32-bit multiply.
  - The legs and feet never change, so they are drawn once into a static layer that each frame starts from, instead of being redrawn every frame.
- **Spreading the work:** a fighter's next frame is now rendered one 32-pixel column per video frame, just before that column is DMA'd. Before, the whole frame was rendered in one go. The DMA queue peak is still 3776 bytes.
- **Idle poses:** the three distinct idle images are rendered once at startup into `idleCache` and streamed from there. Only punch frames are rendered at runtime. This is what got it to 60. It costs about 16 KB more RAM (the cache) plus 5 KB for the static layer, and the build and run were fine with it.

I haven't compared screenshots or VRAM contents against the original. The checker only measures timing and DMA. I kept the draw order and the fill logic the same, so the output should be identical, but that is by reasoning rather than a pixel diff.