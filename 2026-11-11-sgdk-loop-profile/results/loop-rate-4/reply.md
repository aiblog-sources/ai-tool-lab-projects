`./check.sh` now reports 60.0 game loops per second, with 0 of 800 frames dropped. It started at 36.0. The frames are the same as before. I haven't compared the output pixel by pixel, only the frame rate.

All the work was in `src/main.c`. Rendering the fighters on the CPU was the bottleneck: with the render stubbed out, the loop ran at 60.

- **Rectangle fill:** `fillRect` now works one tile at a time with tight inner loops, using small lookup tables for tile layout. The old version did the layout math for every row, which was slow on the 68000. This alone took it from about 26 to 45.
- **Spreading the render:** each fighter's next frame is now drawn one 32-pixel column per video frame, right before that column is sent to VRAM. The DMA timing is unchanged. On its own this didn't help, but it spreads the cost evenly.
- **Static legs and feet:** these are drawn once into a template and copied in, rather than redrawn every frame.
- **Idle pose cache:** the idle pose only depends on a bob offset of 0 to 2, so those 3 images are rendered once at startup and copied from RAM. This was the change that got it to 60.

The extra RAM is about 5 KB for the leg template and 16 KB for the idle cache.

Punch frames are still drawn live. This scripted run has no dropped frames, but I haven't tested other pad input.