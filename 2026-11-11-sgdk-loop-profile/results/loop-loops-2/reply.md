`./check.sh` now reports 60.0 game loops per second, with 0 of 800 frames dropped and mean CPU load around 17% (it was 36.0 loops per second). The `check.sh` DMA summary still shows no DMA during active display. I haven't compared the output pixels against the old renderer. The only checks were `check.sh` and reading the fill code against the tile layout.

**Cause.** Each fighter's 96x112 frame was redrawn in software on the one frame per animation step when the fighter's timer ticked over. That cost about 100,000 cycles, almost a whole video frame, and with the 20,000 cycles of normal work the loop missed the next VBlank every time.

**Fix, all in `src/main.c`:**
- **Pose cache.** The animations only use 7 distinct poses (3 idle bob heights and 4 distinct punch poses). Some repeat: punch frames 2 and 3 are identical, and punch frame 5 matches idle at bob 0. Each pose is rendered once at start-up, while the display is off, into a shared RAM cache of about 37 KB. Both fighters use it.
- **Per-frame work.** The running loop only picks the next pose and queues the same 1,792-byte DMA chunks as before. Nothing is redrawn during play.
- **Faster renderer.** `fillRect` now fills each sprite's 32-pixel-high column as one run of longs, which cut the start-up render cost.

Objects, sizes, animations and timing are unchanged, including the 3-to-6-frame punch delay. The cost is RAM: the cache replaces the two 5,376-byte render buffers, so use goes up by roughly 27 KB. It built and ran without a memory overflow.