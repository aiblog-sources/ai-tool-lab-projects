`check.sh` now reports **60.0 game loops per second**, with 0 of 800 frames dropped. It was 36.0 before. I haven't compared the output pixels against the original build, so identical visuals rest on the logic, not a pixel diff.

The slowdown was the software renderer. Re-drawing a whole 96×112 fighter every 3 frames took about 200k CPU cycles, more than one frame, so each re-render cost an extra frame. Only the renderer and the frame-selection code in `src/main.c` changed. Objects, sizes, animations, input handling and the DMA/VRAM double-buffer scheme are unchanged, including the punch start latency.

- **Idle frames:** there are only 3 distinct idle images, since they differ only by the bob offset. Those 3 plus punch frame 0 are rendered once at boot with the display off, then streamed straight from RAM with no re-render.
- **Punch frames 1–5:** each is rendered one column per video frame, during the step before it is shown. Only P1 can punch, so only P1 gets the two-buffer pool for this.
- **Static legs and feet:** these never change, so the per-frame render skips them and clears and redraws only the top 72 rows.
- **`fillRect`:** it now works per tile instead of per pixel row. It was the second-biggest cost.

Caching all 9 images in RAM didn't boot, so I used the smaller scheme above. The sprite-table update in the main loop now runs only when a fighter's x position or displayed slot changes. It made no measurable difference on its own.

Along the way I read the check tool's call log and a few build files outside the work folder to understand the failures. I did not modify anything there.