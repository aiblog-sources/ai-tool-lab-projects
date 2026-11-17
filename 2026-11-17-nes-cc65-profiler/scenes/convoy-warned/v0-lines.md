| line | function | cycles/frame | busy | source |
| --- | --- | --- | --- | --- |
| v0.c:64 | `_draw` | 4,180 | 28.6% | `p = base + (int)i * CAR_SPACING;` |
| v0.c:83 | `_draw` | 3,123 | 21.4% | `OAM_Y(idx) = 0xFF;` |
| v0.c:84 | `_draw` | 931 | 6.4% | `++idx;` |
| v0.c:82 | `_draw` | 780 | 5.3% | `while (idx < 64) {` |
| (asm) | `nmi` | 621 | 4.3% | `` |
| v0.c:66 | `_draw` | 601 | 4.1% | `x = p - 16;                      /* left edge, -16 .. 943 */` |
| v0.c:73 | `_draw` | 550 | 3.8% | `x += 8;` |
| nes.c:24 | `_pad_read` | 421 | 2.9% | `` |
| v0.c:74 | `_draw` | 383 | 2.6% | `if (x >= 0 && x <= 248) {` |
| v0.c:70 | `_draw` | 380 | 2.6% | `OAM_ATTR(idx) = i & 1; OAM_X(idx) = (unsigned char)x;` |
| v0.c:76 | `_draw` | 380 | 2.6% | `OAM_ATTR(idx) = i & 1; OAM_X(idx) = (unsigned char)x;` |
| v0.c:65 | `_draw` | 375 | 2.6% | `if (p >= ROAD_LEN) p -= ROAD_LEN;` |
| v0.c:69 | `_draw` | 336 | 2.3% | `OAM_Y(idx) = CAR_Y; OAM_TILE(idx) = 0x01;` |
| v0.c:75 | `_draw` | 336 | 2.3% | `OAM_Y(idx) = CAR_Y; OAM_TILE(idx) = 0x01;` |
| v0.c:68 | `_draw` | 323 | 2.2% | `if (x >= 0 && x <= 248) {` |
