`check.sh` now reports **60.0 game loops per second**, with 0 of 800 frames dropped. Every loop fits in one video frame, using 31% of a frame at most. It was 36.0 loops per second at the start. The DMA report is unchanged and clean: largest queue 3,776 bytes, no DMA during active display.

The slowdown came from the CPU software renderer. Each fighter drew its whole 96×112 frame in a single game loop, and every third loop ran over a frame. I changed `src/main.c` in three ways, so the scene should be pixel-identical:

- **One column per tick:** the renderer works on a single 32-pixel column per video frame. That is the same column the DMA sends that tick, so the DMA schedule is unchanged.
- **Static legs:** the legs and feet never change, so they are drawn once at start-up and left in the RAM buffer. Only rows 0–71 (the upper body) get replaced afterwards.
- **Pre-rendered frames:** there are only 9 distinct upper-body images (3 idle bob heights and 6 punch frames), and both fighters share them. `buildTemplates()` renders them once at start-up, with the display off. At run time "rendering" a column is just a copy from that cache. This is what got it to 60. The cache costs about 31 KB of RAM, and it linked and ran fine under the checker.

I also swapped the 32-bit multiply in `fillRect` for a colour lookup table and made its inner loops cheaper.

I only checked timing and DMA, since `check.sh` doesn't compare pixels. The header comment about rendering at the start of each step is now slightly out of date. I left it as is.