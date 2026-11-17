| line | function | cycles/frame | busy | source |
| --- | --- | --- | --- | --- |
| v0.c:67 | `_update_drops` | 2,120 | 14.8% | `y = drop_y[i] + layer_speed[i & 3];   /* max 239 + 6 < 256 */` |
| v0.c:112 | `_fill_oam` | 1,600 | 11.2% | `OAM_Y(n)    = drop_y[i];` |
| v0.c:115 | `_fill_oam` | 1,600 | 11.2% | `OAM_X(n)    = drop_x[i];` |
| v0.c:113 | `_fill_oam` | 1,080 | 7.6% | `OAM_TILE(n) = TILE_DROP;` |
| v0.c:114 | `_fill_oam` | 1,080 | 7.6% | `OAM_ATTR(n) = 0;` |
| v0.c:66 | `_update_drops` | 999 | 7.0% | `for (i = 0; i < NUM_DROPS; ++i) {` |
| v0.c:111 | `_fill_oam` | 973 | 6.8% | `for (i = 0; i < NUM_DROPS; ++i) {` |
| v0.c:72 | `_update_drops` | 840 | 5.9% | `drop_y[i] = y;` |
| (asm) | `nmi` | 621 | 4.3% | `` |
| v0.c:116 | `_fill_oam` | 600 | 4.2% | `++n;` |
| nes.c:24 | `_pad_read` | 421 | 2.9% | `` |
| v0.c:103 | `_fill_oam` | 400 | 2.8% | `OAM_Y(n)    = umb_y + ((i >> 2) << 3);` |
| v0.c:106 | `_fill_oam` | 384 | 2.7% | `OAM_X(n)    = umb_x + ((i & 3) << 3);` |
| v0.c:104 | `_fill_oam` | 320 | 2.2% | `OAM_TILE(n) = umb_tiles[i];` |
| v0.c:105 | `_fill_oam` | 216 | 1.5% | `OAM_ATTR(n) = 1;` |
