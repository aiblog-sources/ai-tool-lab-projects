| line | function | cycles/frame | busy | source |
| --- | --- | --- | --- | --- |
| v0.c:81 | `_main` | 3,320 | 30.7% | `idx = (h - seg * DELAY) & HMASK;` |
| v0.c:85 | `_main` | 1,420 | 13.1% | `OAM_X(k)    = hx[idx];` |
| v0.c:84 | `_main` | 1,402 | 12.9% | `OAM_ATTR(k) = (seg == 0) ? 1 : 0;` |
| v0.c:82 | `_main` | 1,286 | 11.9% | `OAM_Y(k)    = hy[idx];` |
| v0.c:83 | `_main` | 1,260 | 11.6% | `OAM_TILE(k) = 0x04;` |
| (asm) | `nmi` | 621 | 5.7% | `` |
| nes.c:24 | `_pad_read` | 421 | 3.9% | `` |
| v0.c:78 | `_main` | 377 | 3.5% | `for (k = 0; k < SEGS; ++k) {` |
| v0.c:80 | `_main` | 204 | 1.9% | `if (seg >= SEGS) seg -= SEGS;` |
| v0.c:79 | `_main` | 200 | 1.8% | `seg = k + rot;` |
| nes.c:21 | `_pad_read` | 55 | 0.5% | `` |
| v0.c:52 | `_main` | 30 | 0.3% | `pad = pad_read();` |
| v0.c:91 | `_main` | 20 | 0.2% | `wait_frame();` |
| v0.c:66 | `_main` | 17 | 0.2% | `else if (dy > 0) { if (y < 222) ++y; }` |
| v0.c:64 | `_main` | 16 | 0.2% | `else if (dx > 0) { if (x < 248) ++x; }` |
