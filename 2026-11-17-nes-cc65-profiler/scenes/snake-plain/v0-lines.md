| line | function | cycles/frame | busy | source |
| --- | --- | --- | --- | --- |
| v0.c:93 | `_fill_oam` | 2,793 | 26.2% | `idx = (h - (unsigned char)(seg * DELAY)) & HMASK;` |
| v0.c:97 | `_fill_oam` | 1,501 | 14.1% | `OAM_X(j)    = hist_x[idx];` |
| v0.c:94 | `_fill_oam` | 1,374 | 12.9% | `OAM_Y(j)    = hist_y[idx];` |
| v0.c:95 | `_fill_oam` | 1,292 | 12.1% | `OAM_TILE(j) = TILE_BODY;` |
| v0.c:96 | `_fill_oam` | 1,292 | 12.1% | `OAM_ATTR(j) = 0;` |
| (asm) | `nmi` | 621 | 5.8% | `` |
| v0.c:92 | `_fill_oam` | 492 | 4.6% | `for (j = 1; j < SEGS; ++j) {` |
| nes.c:24 | `_pad_read` | 421 | 4.0% | `` |
| v0.c:98 | `_fill_oam` | 323 | 3.0% | `++seg;` |
| v0.c:99 | `_fill_oam` | 102 | 1.0% | `if (seg >= SEGS) seg = 1;` |
| v0.c:53 | `_update` | 56 | 0.5% | `unsigned char pad = pad_read();` |
| nes.c:21 | `_pad_read` | 55 | 0.5% | `` |
| v0.c:114 | `_main` | 37 | 0.3% | `fill_oam();` |
| v0.c:83 | `_fill_oam` | 36 | 0.3% | `OAM_Y(0)    = hist_y[h];` |
| v0.c:113 | `_main` | 20 | 0.2% | `update();` |
