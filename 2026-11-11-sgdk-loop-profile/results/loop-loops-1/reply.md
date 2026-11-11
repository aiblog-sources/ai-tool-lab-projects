`./check.sh` now reports 60.0 game loops per second, with 0 of 800 frames dropped. Before, it reported 36.0 with 320 dropped. CPU load went from 70.5% to 19.3%, and the busiest loop is 91,112 cycles, 71% of a frame. DMA is clean too: no DMA during active display, no missing bytes, and the largest queue is still 3,776 bytes.

The cause was software rendering in `src/main.c`. Each fighter re-rendered its whole 96×112 frame in a single game loop, which cost about 128,000 cycles, a full frame.

I made four changes, none of which alter the sprites, sizes, animations or behavior:
- **Idle frames:** the idle animation only varies by a bob of 0, 1 or 2 pixels, so its three distinct images are rendered once at startup and DMA'd straight from RAM. This was the big win.
- **Punch frames:** these are still drawn live. They are now rendered one 32-pixel column per frame, immediately before that column's DMA chunk, so the timing and DMA schedule are the same as before.
- **Static legs and feet:** they are drawn once into a template and copied in, instead of being redrawn each time.
- **`fillRect`:** it now works tile by tile, with unrolled stores for whole tiles.

The idle cache and leg template cost about 21 KB more RAM.

One assumption: the savings rely on only fighter 1 ever punching, since the code only sets `punchReq` for `p1`. If fighter 2 were also given a punch, two fighters rendering punch frames in the same video frame would be much heavier.