| line | function | cycles/frame | busy | source |
| --- | --- | --- | --- | --- |
| v0.c:81 | `_main` | 3,320 | 41.7% | `idx = (h - seg * DELAY) & HMASK;` |
| v0.c:85 | `_main` | 680 | 8.5% | `OAM_X(k)    = hx[idx];` |
| v0.c:84 | `_main` | 662 | 8.3% | `OAM_ATTR(k) = (seg == 0) ? 1 : 0;` |
| v0.c:82 | `_main` | 646 | 8.1% | `OAM_Y(k)    = hy[idx];` |
| (asm) | `nmi` | 621 | 7.8% | `` |
| v0.c:83 | `_main` | 520 | 6.5% | `OAM_TILE(k) = 0x04;` |
| nes.c:24 | `_pad_read` | 421 | 5.3% | `` |
| v0.c:78 | `_main` | 377 | 4.7% | `for (k = 0; k < SEGS; ++k) {` |
| v0.c:80 | `_main` | 204 | 2.6% | `if (seg >= SEGS) seg -= SEGS;` |
| v0.c:79 | `_main` | 200 | 2.5% | `seg = k + rot;` |
| nes.c:21 | `_pad_read` | 55 | 0.7% | `` |
| v0.c:52 | `_main` | 30 | 0.4% | `pad = pad_read();` |
| v0.c:91 | `_main` | 20 | 0.3% | `wait_frame();` |
| v0.c:66 | `_main` | 17 | 0.2% | `else if (dy > 0) { if (y < 222) ++y; }` |
| v0.c:64 | `_main` | 16 | 0.2% | `else if (dx > 0) { if (x < 248) ++x; }` |
