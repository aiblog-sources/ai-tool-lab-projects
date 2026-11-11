`./check.sh` now reports 60.0 game loops per second, with 0 of 800 frames dropped. Every loop fits in one video frame, the heaviest at 87,298 busy cycles against the 128,005 budget. It started at roughly 25 loops per second. I haven't compared the rendered output against the original pixel for pixel, so "same scene" rests on how I made the changes below, not on a test.

The cost was the software renderer in `src/main.c`. Each fighter re-rendered a whole 96x112 frame every three ticks, and the two fighters' renders landed on the same loops.

- **Column-by-column rendering:** each tick now renders and DMAs one 32-px column per fighter. Streaming and swap timing are unchanged.
- **Faster `fillRect`:** it now writes down the contiguous tile runs in each sprite column, and its per-tile setup and per-row address maths are gone.
- **Pre-drawn legs:** the static legs and feet are drawn once into a template and copied in, instead of being redrawn every frame.
- **Cached idle poses:** the three distinct idle poses (bob 0, 1 and 2) are rendered once at startup into a cache, and idle frames DMA straight from it. Only punches are rendered at run time.
- **Repeated idle frames:** when two consecutive idle frames are identical (bob 2 to 2, and 0 to 0), the fighter keeps its shown slot and skips both the render and the DMA.

The DMA checks are still clean. The largest queue is 3,776 bytes (the documented worst case), with no DMA during active display and no bytes lost. The caches use about 22 KB of extra RAM, and the link still succeeded.