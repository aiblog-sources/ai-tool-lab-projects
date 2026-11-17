| line | function | cycles/frame | busy | source |
| --- | --- | --- | --- | --- |
| v1.c:100 | `_main` | 5,224 | 30.6% | `seg = 1 + (unsigned char)((rot + 3 * k) % (SEGS - 1));` |
| v1.c:31 | `_put_seg` | 3,320 | 19.4% | `idx = (h - seg * DELAY) & HMASK;` |
| v1.c:35 | `_put_seg` | 1,420 | 8.3% | `OAM_X(slot)    = hx[idx];` |
| v1.c:34 | `_put_seg` | 1,402 | 8.2% | `OAM_ATTR(slot) = (seg == 0) ? 1 : 0;` |
| v1.c:32 | `_put_seg` | 1,286 | 7.5% | `OAM_Y(slot)    = hy[idx];` |
| v1.c:33 | `_put_seg` | 1,260 | 7.4% | `OAM_TILE(slot) = 0x04;` |
| (asm) | `nmi` | 621 | 3.6% | `` |
| v1.c:93 | `_main` | 575 | 3.4% | `for (k = 0; k < SEGS; ++k) used[k] = 0;` |
| nes.c:24 | `_pad_read` | 421 | 2.5% | `` |
| v1.c:104 | `_main` | 338 | 2.0% | `for (seg = 1; seg < SEGS; ++seg) {` |
| v1.c:105 | `_main` | 269 | 1.6% | `if (!used[seg]) put_seg();` |
| v1.c:36 | `_put_seg` | 220 | 1.3% | `used[seg] = 1;` |
| v1.c:99 | `_main` | 134 | 0.8% | `for (k = 0; k < 7; ++k) {` |
| v1.c:37 | `_put_seg` | 120 | 0.7% | `++slot;` |
| v1.c:38 | `_put_seg` | 120 | 0.7% | `}` |
