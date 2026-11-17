| line | function | cycles/frame | busy | source |
| --- | --- | --- | --- | --- |
| v1.c:101 | `_fill_oam` | 2,793 | 35.2% | `idx = (h - (unsigned char)(seg * DELAY)) & HMASK;` |
| v1.c:105 | `_fill_oam` | 798 | 10.0% | `OAM_X(j)    = hist_x[idx];` |
| v1.c:102 | `_fill_oam` | 766 | 9.6% | `OAM_Y(j)    = hist_y[idx];` |
| (asm) | `nmi` | 621 | 7.8% | `` |
| v1.c:103 | `_fill_oam` | 589 | 7.4% | `OAM_TILE(j) = TILE_BODY;` |
| v1.c:104 | `_fill_oam` | 589 | 7.4% | `OAM_ATTR(j) = 0;` |
| v1.c:100 | `_fill_oam` | 492 | 6.2% | `for (j = 1; j < SEGS; ++j) {` |
| nes.c:24 | `_pad_read` | 421 | 5.3% | `` |
| v1.c:106 | `_fill_oam` | 323 | 4.1% | `++seg;` |
| v1.c:107 | `_fill_oam` | 102 | 1.3% | `if (seg >= SEGS) seg = 1;` |
| v1.c:59 | `_update` | 56 | 0.7% | `unsigned char pad = pad_read();` |
| nes.c:21 | `_pad_read` | 55 | 0.7% | `` |
| v1.c:122 | `_main` | 37 | 0.5% | `fill_oam();` |
| v1.c:89 | `_fill_oam` | 36 | 0.5% | `OAM_Y(0)    = hist_y[h];` |
| v1.c:121 | `_main` | 20 | 0.3% | `update();` |
