| line | function | cycles/frame | busy | source |
| --- | --- | --- | --- | --- |
| v1.c:100 | `_main` | 5,224 | 36.7% | `seg = 1 + (unsigned char)((rot + 3 * k) % (SEGS - 1));` |
| v1.c:31 | `_put_seg` | 3,320 | 23.3% | `idx = (h - seg * DELAY) & HMASK;` |
| v1.c:35 | `_put_seg` | 680 | 4.8% | `OAM_X(slot)    = hx[idx];` |
| v1.c:34 | `_put_seg` | 662 | 4.6% | `OAM_ATTR(slot) = (seg == 0) ? 1 : 0;` |
| v1.c:32 | `_put_seg` | 646 | 4.5% | `OAM_Y(slot)    = hy[idx];` |
| (asm) | `nmi` | 621 | 4.4% | `` |
| v1.c:93 | `_main` | 575 | 4.0% | `for (k = 0; k < SEGS; ++k) used[k] = 0;` |
| v1.c:33 | `_put_seg` | 520 | 3.7% | `OAM_TILE(slot) = 0x04;` |
| nes.c:24 | `_pad_read` | 421 | 3.0% | `` |
| v1.c:104 | `_main` | 338 | 2.4% | `for (seg = 1; seg < SEGS; ++seg) {` |
| v1.c:105 | `_main` | 269 | 1.9% | `if (!used[seg]) put_seg();` |
| v1.c:36 | `_put_seg` | 220 | 1.5% | `used[seg] = 1;` |
| v1.c:99 | `_main` | 134 | 0.9% | `for (k = 0; k < 7; ++k) {` |
| v1.c:37 | `_put_seg` | 120 | 0.8% | `++slot;` |
| v1.c:38 | `_put_seg` | 120 | 0.8% | `}` |
