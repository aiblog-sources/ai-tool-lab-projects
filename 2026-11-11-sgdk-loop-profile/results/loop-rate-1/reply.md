`check.sh` now reports **60.0 game loops per second**, with 0 of 800 frames dropped. It was 36.0 before. I haven't compared the output pixel for pixel against the original. I checked the equivalence by reading the code, not by running a comparison.

**Cause:** the software renderer used about two frames of CPU every animation step, with each fighter's step landing on a different frame. Those frames then took two video frames each, which gives the 36. Reducing the cost of the existing fill routine barely helped, and the figure stayed at 36 or got worse.

**What I changed in `src/main.c`:**
- **Rows that never change:** rows 72 and below hold only legs and feet and are identical in every frame. They are drawn once at init, and the per-frame render only touches rows 0 to 71.
- **Poses baked at init:** the rectangle painter now runs once at startup, and each pose is stored as horizontal bands of identical pixel rows. The baked poses take about 19 KB of RAM. The per-frame work is just storing those bands into the render buffer.
- **One column per tick:** each fighter stores one sprite column per tick. Its DMA chunk is queued in the same tick, so the DMA timing matches the original design.
- **Sprite table updates:** the sprite table is rebuilt and queued only when a fighter's x position or displayed slot changes. The table contents are the same as before.

**Unchanged:** the same fighters, sizes, 6-frame idle and punch animations, controls and punch latency. The initial full render still uses the original `fillRect` path.