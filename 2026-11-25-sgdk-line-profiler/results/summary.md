# sgdkhot on the 40 games of the 2026-10-29 test

Programs: 40. Verdicts: matters = at least 1% of a frame (1,280 cycles) per frame on average, small = 0.2 to 1%, negligible = under 0.2%, not run = no cycles on that line in the 1,200 counted frames.

## Lint findings on the original code (v0), by rule

| Rule | Findings | matters | small | negligible | not run |
| --- | --- | --- | --- | --- | --- |
| text | 152 | 2 | 3 | 126 | 21 |
| div | 68 | 4 | 7 | 45 | 12 |
| tile-loop | 21 | 2 | 3 | 12 | 4 |
| mul32 | 11 | 1 | 0 | 8 | 2 |
| all | 252 | 9 | 13 | 191 | 39 |

## Same, by lint level

| Level | Findings | matters | small | negligible | not run |
| --- | --- | --- | --- | --- | --- |
| heavy | 5 | 0 | 1 | 2 | 2 |
| hot | 13 | 5 | 1 | 5 | 2 |
| frame | 20 | 2 | 1 | 17 | 0 |
| guarded | 214 | 2 | 10 | 167 | 35 |

## Lint estimates vs measurement (div and mul32 findings whose line called libgcc)

Lint per op = its estimate divided by the loop passes it assumed; lint per frame = its estimate (passes x op). Measured per frame = everything on that line.

| Program | Line | Level | Lint per op | Measured per call | Callee | Calls per frame | Lint per frame | Measured per frame |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| breakout-sonnet-5-5-r2 | 191 | guarded | 617 | 578 | __divsi3 | 0.14 | 617 | 94 |
| bullets-haiku-5-5-r1 | 126 | guarded | 1,041 | 835 | __umodsi3 | 0.19 | 1,041 | 172 |
| bullets-haiku-5-5-r1 | 128 | guarded | 623 | 415 | __udivsi3 | 0.15 | 623 | 73 |
| bullets-haiku-5-5-r1 | 207 | guarded | 453 | 581 | __divsi3 | 0.10 | 18,120 | 75 |
| bullets-haiku-5-5-r1 | 208 | guarded | 453 | 575 | __divsi3 | 0.10 | 18,120 | 73 |
| bullets-opus-5-5-r1 | 129 | guarded | 617 | 579 | __divsi3 | 0.10 | 617 | 69 |
| bullets-opus-5-5-r1 | 130 | guarded | 617 | 570 | __divsi3 | 0.10 | 617 | 68 |
| bullets-sonnet-5-5-r1 | 128 | guarded | 617 | 580 | __divsi3 | 0.10 | 24,680 | 70 |
| bullets-sonnet-5-5-r1 | 129 | guarded | 617 | 571 | __divsi3 | 0.10 | 24,680 | 70 |
| bullets-sonnet-5-5-r2 | 143 | guarded | 617 | 580 | __divsi3 | 0.10 | 24,680 | 71 |
| bullets-sonnet-5-5-r2 | 144 | guarded | 617 | 571 | __divsi3 | 0.10 | 24,680 | 68 |
| homing-haiku-5-5-r1 | 99 | hot | 268 | 262 | __mulsi3 | 15.87 | 2,144 | 5,106 |
| homing-haiku-5-5-r1 | 107 | guarded | 453 | 582 | __divsi3 | 7.93 | 3,624 | 5,502 |
| homing-haiku-5-5-r1 | 108 | guarded | 453 | 581 | __divsi3 | 7.93 | 3,624 | 5,461 |
| homing-haiku-5-5-r1 | 151 | guarded | 453 | 833 | __umodsi3 | 0.63 | 453 | 576 |
| homing-haiku-5-5-r1 | 152 | guarded | 453 | 413 | __udivsi3 | 0.63 | 453 | 298 |
| homing-haiku-5-5-r1 | 163 | guarded | 1,041 | 1,334 | __umodsi3 | 0.06 | 1,041 | 90 |
| homing-haiku-5-5-r1 | 168 | guarded | 1,041 | 847 | __umodsi3 | 0.06 | 1,041 | 58 |
| homing-haiku-5-5-r1 | 236 | frame | 617 | 411 | __udivsi3 | 0.99 | 617 | 488 |
| homing-sonnet-5-5-r1 | 298 | guarded | 617 | 577 | __divsi3 | 0.94 | 4,936 | 649 |
| homing-sonnet-5-5-r1 | 299 | guarded | 617 | 578 | __divsi3 | 0.94 | 4,936 | 650 |
| homing-sonnet-5-5-r2 | 237 | guarded | 617 | 577 | __divsi3 | 0.79 | 4,936 | 559 |
| homing-sonnet-5-5-r2 | 238 | guarded | 617 | 578 | __divsi3 | 0.79 | 4,936 | 565 |
| hud-haiku-5-5-r1 | 57 | hot | 1,041 | 834 | __umodsi3 | 8.00 | 1,041 | 7,377 |
| hud-haiku-5-5-r1 | 58 | hot | 623 | 413 | __udivsi3 | 6.00 | 623 | 2,811 |
| particles-haiku-5-5-r1 | 62 | guarded | 268 | 259 | __mulsi3 | 0.24 | 12,864 | 89 |
| particles-haiku-5-5-r1 | 63 | guarded | 268 | 259 | __mulsi3 | 0.24 | 12,864 | 88 |
| particles-opus-5-5-r1 | 114 | guarded | 268 | 234 | __mulsi3 | 0.24 | 12,864 | 78 |
| particles-opus-5-5-r1 | 115 | guarded | 268 | 234 | __mulsi3 | 0.24 | 12,864 | 82 |
| particles-sonnet-5-5-r1 | 66 | guarded | 1,062 | 1,000 | __modsi3 | 0.20 | 50,976 | 232 |
| particles-sonnet-5-5-r2 | 63 | guarded | 268 | 235 | __mulsi3 | 0.23 | 268 | 139 |
| particles-sonnet-5-5-r2 | 64 | guarded | 268 | 235 | __mulsi3 | 0.23 | 268 | 100 |
| pong-haiku-5-5-r1 | 91 | guarded | 453 | 575 | __divsi3 | 0.00 | 453 | 2 |
| pong-haiku-5-5-r1 | 92 | guarded | 453 | 586 | __divsi3 | 0.00 | 453 | 2 |
| pong-sonnet-5-5-r1 | 124 | guarded | 623 | 576 | __divsi3 | 0.00 | 623 | 1 |
| pong-sonnet-5-5-r1 | 130 | guarded | 268 | 230 | __mulsi3 | 0.00 | 268 | 0 |
| snake-opus-5-5-r1 | 192 | guarded | 453 | 579 | __divsi3 | 0.38 | 453 | 255 |
| snake-opus-5-5-r1 | 333 | guarded | 453 | 1,002 | __modsi3 | 0.03 | 453 | 33 |
| snake-opus-5-5-r1 | 341 | guarded | 453 | 1,004 | __modsi3 | 0.03 | 453 | 34 |
| snake-sonnet-5-5-r1 | 121 | guarded | 453 | 1,004 | __modsi3 | 0.02 | 1,359 | 20 |
| snake-sonnet-5-5-r1 | 134 | guarded | 453 | 1,004 | __modsi3 | 0.02 | 453 | 18 |
| snake-sonnet-5-5-r1 | 153 | guarded | 453 | 580 | __divsi3 | 0.20 | 453 | 132 |
| snake-sonnet-5-5-r2 | 147 | frame | 623 | 580 | __divsi3 | 0.38 | 623 | 252 |

per call, measured / lint: median 0.94, range 0.66 to 2.22; per frame, measured / lint: median 0.110, range 0.0010 to 7.09 (43 findings)

## Per program: busy cycles, share on lint-flagged lines, hottest game line

| Program | Load | Findings | Busy on flagged lines | Hottest game line (cycles/frame) | Flagged? | What it is |
| --- | --- | --- | --- | --- | --- | --- |
| breakout-haiku-5-5-r1 | 4.7% | 7 | 1.2% | main.c:405 (755) | no | DMA_doCPUCopyDirect |
| breakout-opus-5-5-r1 | 4.9% | 21 | 0.6% | main.c:337 (1,427) | no | DMA_transfer.isra.0 |
| breakout-sonnet-5-5-r1 | 4.8% | 10 | 0.9% | main.c:338 (950) | no | DMA_queueDmaFast |
| breakout-sonnet-5-5-r2 | 5.7% | 8 | 2.3% | main.c:441 (950) | no | DMA_queueDmaFast |
| bullets-haiku-5-5-r1 | 12.2% | 6 | 3.0% | main.c:261 (2,483) | no | own code (incl. inlined SGDK) |
| bullets-opus-5-5-r1 | 12.3% | 4 | 1.4% | main.c:234 (1,872) | no | own code (incl. inlined SGDK) |
| bullets-sonnet-5-5-r1 | 11.9% | 5 | 1.1% | main.c:207 (1,316) | no | own code (incl. inlined SGDK) |
| bullets-sonnet-5-5-r2 | 18.9% | 5 | 0.7% | main.c:271 (3,119) | no | own code (incl. inlined SGDK) |
| homing-haiku-5-5-r1 | 28.3% | 12 | 49.6% | main.c:107 (5,502) | yes | __divsi3 |
| homing-opus-5-5-r1 | 6.3% | 7 | 0.3% | main.c:437 (1,430) | no | DMA_transfer.isra.0 |
| homing-sonnet-5-5-r1 | 8.2% | 8 | 12.6% | main.c:252 (1,430) | no | own code (incl. inlined SGDK) |
| homing-sonnet-5-5-r2 | 7.1% | 11 | 12.7% | main.c:176 (1,430) | no | own code (incl. inlined SGDK) |
| hud-haiku-5-5-r1 | 19.0% | 5 | 76.5% | main.c:57 (7,377) | yes | __umodsi3 |
| hud-opus-5-5-r1 | 3.6% | 10 | 13.0% | main.c:129 (398) | yes | VDP_drawText |
| hud-sonnet-5-5-r1 | 3.7% | 15 | 15.7% | main.c:116 (337) | yes | VDP_drawText |
| hud-sonnet-5-5-r2 | 3.8% | 7 | 15.6% | main.c:101 (432) | yes | VDP_drawText |
| particles-haiku-5-5-r1 | 13.4% | 2 | 1.0% | main.c:96 (2,755) | no | own code (incl. inlined SGDK) |
| particles-opus-5-5-r1 | 12.8% | 4 | 2.0% | main.c:156 (1,313) | no | own code (incl. inlined SGDK) |
| particles-sonnet-5-5-r1 | 12.7% | 1 | 1.4% | main.c:49 (3,868) | no | own code (incl. inlined SGDK) |
| particles-sonnet-5-5-r2 | 13.7% | 2 | 1.4% | main.c:115 (3,572) | no | DMA_transfer.isra.0 |
| platformer-haiku-5-5-r1 | 1.4% | 5 | 4.0% | main.c:279 (623) | no | DMA_doCPUCopyDirect |
| platformer-opus-5-5-r1 | 6.4% | 6 | 8.0% | main.c:403 (1,422) | no | DMA_transfer.isra.0 |
| platformer-sonnet-5-5-r1 | 5.3% | 4 | 0.1% | main.c:331 (908) | no | DMA_queueDmaFast |
| platformer-sonnet-5-5-r2 | 7.5% | 5 | 0.1% | main.c:222 (1,428) | no | DMA_transfer.isra.0 |
| pong-haiku-5-5-r1 | 4.8% | 7 | 1.0% | main.c:212 (1,416) | no | DMA_transfer.isra.0 |
| pong-opus-5-5-r1 | 4.8% | 7 | 1.0% | main.c:299 (1,428) | no | DMA_transfer.isra.0 |
| pong-sonnet-5-5-r1 | 4.9% | 4 | 0.7% | main.c:245 (1,430) | no | DMA_transfer.isra.0 |
| pong-sonnet-5-5-r2 | 4.8% | 8 | 0.6% | main.c:282 (1,429) | no | DMA_transfer.isra.0 |
| scroller-haiku-5-5-r1 | 5.0% | 1 | 14.7% | main.c:138 (933) | yes | own code (incl. inlined SGDK) |
| scroller-opus-5-5-r1 | 4.9% | 0 | 0.0% | main.c:194 (905) | no | DMA_queueDmaFast |
| scroller-sonnet-5-5-r1 | 6.0% | 1 | 24.4% | main.c:178 (1,864) | yes | own code (incl. inlined SGDK) |
| scroller-sonnet-5-5-r2 | 5.2% | 1 | 14.8% | main.c:184 (991) | yes | own code (incl. inlined SGDK) |
| snake-haiku-5-5-r1 | 3.1% | 8 | 0.6% | main.c:98 (39) | no | own code (incl. inlined SGDK) |
| snake-opus-5-5-r1 | 3.4% | 13 | 8.7% | main.c:192 (255) | yes | __divsi3 |
| snake-sonnet-5-5-r1 | 3.4% | 17 | 9.6% | main.c:153 (132) | yes | __divsi3 |
| snake-sonnet-5-5-r2 | 3.4% | 13 | 8.2% | main.c:147 (252) | yes | __divsi3 |
| starfield-haiku-5-5-r1 | 20.0% | 1 | 0.3% | main.c:187 (7,548) | no | own code (incl. inlined SGDK) |
| starfield-opus-5-5-r1 | 14.3% | 1 | 0.4% | main.c:228 (4,172) | no | own code (incl. inlined SGDK) |
| starfield-sonnet-5-5-r1 | 11.1% | 0 | 0.0% | main.c:177 (2,863) | no | own code (incl. inlined SGDK) |
| starfield-sonnet-5-5-r2 | 11.9% | 0 | 0.0% | main.c:93 (3,087) | no | own code (incl. inlined SGDK) |

busy cycles on flagged lines: median 1.4%, range 0.0 to 76.5%; hottest game line flagged in 11 of 40
what the hottest game line is: own code (incl. inlined SGDK) 17, DMA_transfer 9, DMA_queueDmaFast 4, __divsi3 4, VDP_drawText 3, DMA_doCPUCopyDirect 2, __umodsi3 1
SYS_doVBlankProcess line: 32 to 4,820 cycles per frame (42 lines)

## Fix rounds from the same v0 code (load in % of a frame, mean over programs)

`lint2` and `plain` are the 2026-10-29 rounds (re-profiled here, same numbers); `hot` is the new round: the `plain` prompt plus the sgdkhot report of v0.

| Model | Stage | Programs built | Mean load v0 | Mean load after | Mean change | Programs faster / slower | Dropped frames after (sum) | USD |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| haiku-5-5 | lint2 | 10 of 10 | 11.18 | 8.56 | -2.63 | 4 / 0 | 0 | see 2026-10-29 |
| haiku-5-5 | plain | 10 of 10 | 11.18 | 13.91 | +2.73 | 7 / 1 | 880 | see 2026-10-29 |
| haiku-5-5 | hot | 10 of 10 | 11.18 | 6.93 | -4.26 | 8 / 1 | 0 | 0.08 |
| opus-5-5 | lint2 | 9 of 10 | 7.65 | 7.55 | -0.10 | 4 / 0 | 2 | see 2026-10-29 |
| opus-5-5 | plain | 10 of 10 | 7.38 | 5.55 | -1.83 | 10 / 0 | 0 | see 2026-10-29 |
| opus-5-5 | hot | 10 of 10 | 7.38 | 5.45 | -1.93 | 10 / 0 | 2 | 2.34 |
| sonnet-5-5 | lint2 | 18 of 20 | 7.27 | 7.12 | -0.14 | 9 / 3 | 5 | see 2026-10-29 |
| sonnet-5-5 | plain | 20 of 20 | 7.69 | 6.27 | -1.42 | 20 / 0 | 0 | see 2026-10-29 |
| sonnet-5-5 | hot | 20 of 20 | 7.69 | 6.24 | -1.46 | 19 / 0 | 0 | 1.85 |

hot vs plain, program by program (0.05-point margin): hot lower in 14, higher in 16, same in 10
