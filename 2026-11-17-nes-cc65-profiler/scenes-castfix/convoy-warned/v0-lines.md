| line | function | cycles/frame | busy | source |
| --- | --- | --- | --- | --- |
| v0.c:64 | `_draw` | 4,180 | 34.6% | `p = base + (int)i * CAR_SPACING;` |
| v0.c:83 | `_draw` | 1,479 | 12.2% | `OAM_Y(idx) = 0xFF;` |
| v0.c:84 | `_draw` | 931 | 7.7% | `++idx;` |
| v0.c:82 | `_draw` | 668 | 5.5% | `while (idx < 64) {` |
| (asm) | `nmi` | 621 | 5.1% | `` |
| v0.c:66 | `_draw` | 601 | 5.0% | `x = p - 16;                      /* left edge, -16 .. 943 */` |
| v0.c:73 | `_draw` | 550 | 4.6% | `x += 8;` |
| nes.c:24 | `_pad_read` | 421 | 3.5% | `` |
| v0.c:65 | `_draw` | 375 | 3.1% | `if (p >= ROAD_LEN) p -= ROAD_LEN;` |
| v0.c:74 | `_draw` | 373 | 3.1% | `if (x >= 0 && x <= 248) {` |
| v0.c:68 | `_draw` | 313 | 2.6% | `if (x >= 0 && x <= 248) {` |
| v0.c:63 | `_draw` | 267 | 2.2% | `for (i = 0; i < NUM_CARS; ++i) {` |
| v0.c:70 | `_draw` | 188 | 1.6% | `OAM_ATTR(idx) = i & 1; OAM_X(idx) = (unsigned char)x;` |
| v0.c:76 | `_draw` | 188 | 1.6% | `OAM_ATTR(idx) = i & 1; OAM_X(idx) = (unsigned char)x;` |
| v0.c:69 | `_draw` | 156 | 1.3% | `OAM_Y(idx) = CAR_Y; OAM_TILE(idx) = 0x01;` |
