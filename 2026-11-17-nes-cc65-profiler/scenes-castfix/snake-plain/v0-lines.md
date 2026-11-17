| line | function | cycles/frame | busy | source |
| --- | --- | --- | --- | --- |
| v0.c:93 | `_fill_oam` | 2,793 | 35.2% | `idx = (h - (unsigned char)(seg * DELAY)) & HMASK;` |
| v0.c:97 | `_fill_oam` | 798 | 10.1% | `OAM_X(j)    = hist_x[idx];` |
| v0.c:94 | `_fill_oam` | 766 | 9.6% | `OAM_Y(j)    = hist_y[idx];` |
| (asm) | `nmi` | 621 | 7.8% | `` |
| v0.c:95 | `_fill_oam` | 589 | 7.4% | `OAM_TILE(j) = TILE_BODY;` |
| v0.c:96 | `_fill_oam` | 589 | 7.4% | `OAM_ATTR(j) = 0;` |
| v0.c:92 | `_fill_oam` | 492 | 6.2% | `for (j = 1; j < SEGS; ++j) {` |
| nes.c:24 | `_pad_read` | 421 | 5.3% | `` |
| v0.c:98 | `_fill_oam` | 323 | 4.1% | `++seg;` |
| v0.c:99 | `_fill_oam` | 102 | 1.3% | `if (seg >= SEGS) seg = 1;` |
| v0.c:53 | `_update` | 56 | 0.7% | `unsigned char pad = pad_read();` |
| nes.c:21 | `_pad_read` | 55 | 0.7% | `` |
| v0.c:114 | `_main` | 37 | 0.5% | `fill_oam();` |
| v0.c:83 | `_fill_oam` | 36 | 0.5% | `OAM_Y(0)    = hist_y[h];` |
| v0.c:113 | `_main` | 20 | 0.3% | `update();` |
