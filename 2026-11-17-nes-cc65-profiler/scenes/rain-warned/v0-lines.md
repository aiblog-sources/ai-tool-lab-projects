| line | function | cycles/frame | busy | source |
| --- | --- | --- | --- | --- |
| v0.c:115 | `_fill_oam` | 3,080 | 14.6% | `OAM_X(n)    = drop_x[i];` |
| v0.c:112 | `_fill_oam` | 2,840 | 13.5% | `OAM_Y(n)    = drop_y[i];` |
| v0.c:113 | `_fill_oam` | 2,560 | 12.1% | `OAM_TILE(n) = TILE_DROP;` |
| v0.c:114 | `_fill_oam` | 2,560 | 12.1% | `OAM_ATTR(n) = 0;` |
| v0.c:67 | `_update_drops` | 2,120 | 10.0% | `y = drop_y[i] + layer_speed[i & 3];   /* max 239 + 6 < 256 */` |
| v0.c:66 | `_update_drops` | 999 | 4.7% | `for (i = 0; i < NUM_DROPS; ++i) {` |
| v0.c:111 | `_fill_oam` | 974 | 4.6% | `for (i = 0; i < NUM_DROPS; ++i) {` |
| v0.c:72 | `_update_drops` | 840 | 4.0% | `drop_y[i] = y;` |
| v0.c:106 | `_fill_oam` | 680 | 3.2% | `OAM_X(n)    = umb_x + ((i & 3) << 3);` |
| v0.c:103 | `_fill_oam` | 648 | 3.1% | `OAM_Y(n)    = umb_y + ((i >> 2) << 3);` |
| (asm) | `nmi` | 621 | 2.9% | `` |
| v0.c:104 | `_fill_oam` | 616 | 2.9% | `OAM_TILE(n) = umb_tiles[i];` |
| v0.c:116 | `_fill_oam` | 600 | 2.8% | `++n;` |
| v0.c:105 | `_fill_oam` | 512 | 2.4% | `OAM_ATTR(n) = 1;` |
| nes.c:24 | `_pad_read` | 421 | 2.0% | `` |
