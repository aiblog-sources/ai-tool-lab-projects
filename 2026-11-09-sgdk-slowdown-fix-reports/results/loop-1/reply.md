`./check.sh` now reports 60.0 game loops per second, with 0 of 800 frames dropped. The slowest loop uses 115,747 of the 128,005 cycles in a frame (90%). The DMA checker shows no DMA during active display and a peak of 3,776 bytes per frame, the same as the original design's worst case.

The original rendered a fighter's whole next frame in one game loop, about 100k cycles each. That pushed those loops past one video frame, so the frame was shown twice. I made three changes in `src/main.c`:

- **Rendering:** the next frame is now drawn one 32-pixel column at a time, in the same tick that column is streamed to VRAM, instead of all at once. The anim/frame choice still happens at the same cycle boundary, so punch timing is unchanged.
- **`fillRect`:** it now loops over tile columns and runs down each tile's contiguous rows, rather than stepping scanline by scanline.
- **Idle poses:** the six idle frames only differ by a bob offset of 0, 1 or 2, so three are pre-rendered at startup into `idleCache`. That costs about 16 KB of RAM, and the build still links. Idle frames now go straight to DMA with no drawing. Only punch frames are drawn at runtime.

The scene should look the same, with the same objects, sizes, animations and punch latency, but I did not compare rendered output pixel by pixel. I also only looked at the end of `check.sh`'s output, so I can't say whether it flags any visual difference.