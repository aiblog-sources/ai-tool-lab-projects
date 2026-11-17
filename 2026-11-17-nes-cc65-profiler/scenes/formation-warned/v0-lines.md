| line | function | cycles/frame | busy | source |
| --- | --- | --- | --- | --- |
| nes.c:47 | `_oam_hide_all` | 4,608 | 26.0% | `` |
| v0.c:108 | `_draw` | 4,270 | 24.0% | `put_ship(n, form_x + c * ENEMY_PITCH,` |
| v0.c:109 | `_draw` | 3,330 | 18.8% | `ENEMY_ROW_Y0 + r * ENEMY_ROW_DY, 1 + r);` |
| v0.c:46 | `_put_ship` | 452 | 2.5% | `OAM[o + 7] = x + 8;` |
| v0.c:38 | `_put_ship` | 430 | 2.4% | `unsigned char o = i << 2;` |
| v0.c:45 | `_put_ship` | 430 | 2.4% | `OAM[o + 6] = attr \| 0x40;      /* horizontal flip for the right half */` |
| v0.c:41 | `_put_ship` | 410 | 2.3% | `OAM[o + 2] = attr;` |
| v0.c:42 | `_put_ship` | 410 | 2.3% | `OAM[o + 3] = x;` |
| v0.c:43 | `_put_ship` | 410 | 2.3% | `OAM[o + 4] = y;` |
| (asm) | `nmi` | 356 | 2.0% | `` |
| v0.c:40 | `_put_ship` | 336 | 1.9% | `OAM[o + 1] = 0x10;` |
| v0.c:44 | `_put_ship` | 336 | 1.9% | `OAM[o + 5] = 0x10;` |
| v0.c:37 | `_put_ship` | 315 | 1.8% | `{` |
| v0.c:39 | `_put_ship` | 315 | 1.8% | `OAM[o + 0] = y;` |
| v0.c:107 | `_draw` | 255 | 1.4% | `for (c = 0; c < ENEMY_COLS; ++c) {` |
