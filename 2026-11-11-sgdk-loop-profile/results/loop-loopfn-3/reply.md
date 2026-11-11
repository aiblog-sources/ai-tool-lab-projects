`./check.sh` now reports 60.0 game loops per second, with 0 of 800 frames dropped. It was 25.9 per second after my first attempt and about 30 on the original. The busiest frame now uses 88% of a frame's cycles.

I changed three things, all in `src/main.c`:

1. **Idle frames pre-rendered at startup.** The idle bob has only 3 distinct poses (offsets 0, 1 and 2), so I render them once into RAM before the display turns on. Both fighters then DMA them straight from there. P2 only ever idles, so it no longer renders at runtime, and neither does P1 while idle.
2. **Punch rendered one column per video frame.** `renderColumn` draws the column that is about to be streamed, in the same frame as that column's DMA. The DMA timing and sizes are unchanged: still 1792 bytes per chunk, at most 3776 per frame.
3. **Faster `fillRect`.** It now fills tile by tile with a pointer loop instead of recomputing the tile lookup for every pixel row. This took the worst column render from about 114k cycles to well under a frame.

The check reports no DMA during active display, no frames ending with a DMA still running, and no bytes missing from the VDP. I didn't compare the output pixel for pixel with the original. The pixels are drawn by the same rectangles with the same clipping and masks, so I expect the scene to look identical, but I haven't verified that.

The comment block at the top of the file still describes the old render-everything-at-once structure.