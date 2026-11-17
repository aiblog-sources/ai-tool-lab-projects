| line | function | cycles/frame | busy | source |
| --- | --- | --- | --- | --- |
| v1.c:101 | `_fill_oam` | 2,793 | 26.2% | `idx = (h - (unsigned char)(seg * DELAY)) & HMASK;` |
| v1.c:105 | `_fill_oam` | 1,501 | 14.1% | `OAM_X(j)    = hist_x[idx];` |
| v1.c:102 | `_fill_oam` | 1,374 | 12.9% | `OAM_Y(j)    = hist_y[idx];` |
| v1.c:103 | `_fill_oam` | 1,292 | 12.1% | `OAM_TILE(j) = TILE_BODY;` |
| v1.c:104 | `_fill_oam` | 1,292 | 12.1% | `OAM_ATTR(j) = 0;` |
| (asm) | `nmi` | 621 | 5.8% | `` |
| v1.c:100 | `_fill_oam` | 492 | 4.6% | `for (j = 1; j < SEGS; ++j) {` |
| nes.c:24 | `_pad_read` | 421 | 3.9% | `` |
| v1.c:106 | `_fill_oam` | 323 | 3.0% | `++seg;` |
| v1.c:107 | `_fill_oam` | 102 | 1.0% | `if (seg >= SEGS) seg = 1;` |
| v1.c:59 | `_update` | 56 | 0.5% | `unsigned char pad = pad_read();` |
| nes.c:21 | `_pad_read` | 55 | 0.5% | `` |
| v1.c:122 | `_main` | 37 | 0.3% | `fill_oam();` |
| v1.c:89 | `_fill_oam` | 36 | 0.3% | `OAM_Y(0)    = hist_y[h];` |
| v1.c:121 | `_main` | 20 | 0.2% | `update();` |
