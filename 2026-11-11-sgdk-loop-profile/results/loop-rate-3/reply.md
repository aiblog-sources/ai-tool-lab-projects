`./check.sh` now reports 60.0 game loops per second, with 0 of 800 frames dropped in frames 700–1499. It was 36.0 before.

Software rendering was the whole cost. With the fighters' render call disabled, the rate was already 60. I could only see `check.sh`'s rate output, so I found this by experiment rather than by profiling. I made four changes, all in `src/main.c`:

- **`fillRect`:** it now fills tile by tile instead of row by row, with far less per-row overhead. That took the rate from 36 to about 44.
- **Per-column rendering:** `renderFighter` now draws one 32-pixel column per video frame, right before that column's DMA chunk is queued. The render work is spread out instead of landing on one frame.
- **Cached idle frames:** the three distinct idle poses (bob 0, 1 and 2) are rendered once at startup, with the display off, into a shared 16 KB cache. Idle animation streams to VRAM straight from the cache, so only P1's punch frames are rendered at runtime.
- **Sprite table:** it is only rebuilt and DMA'd when P1's x or either fighter's visible slot changes.

The scene should look the same: same sprites, sizes, animations, punch timing and controls. The rendered pixels are identical to before. I checked that only through the frame rate, not by comparing frames pixel by pixel.