| line | function | cycles/frame | busy | source |
| --- | --- | --- | --- | --- |
| v0.c:90 | `_fill_oam` | 3,459 | 20.0% | `1 + (i % 3), enemies[i].dir);` |
| v0.c:46 | `_draw_char` | 1,848 | 10.7% | `OAM_Y(slot + 1) = ty; OAM_TILE(slot + 1) = 0x12; OAM_ATTR(slot + 1) = attr; OAM_X(slot + 1) = xr;` |
| v0.c:47 | `_draw_char` | 1,836 | 10.6% | `OAM_Y(slot + 2) = by; OAM_TILE(slot + 2) = 0x10; OAM_ATTR(slot + 2) = attr; OAM_X(slot + 2) = xl;` |
| v0.c:48 | `_draw_char` | 1,836 | 10.6% | `OAM_Y(slot + 3) = by; OAM_TILE(slot + 3) = 0x10; OAM_ATTR(slot + 3) = attr; OAM_X(slot + 3) = xr;` |
| v0.c:45 | `_draw_char` | 1,668 | 9.7% | `OAM_Y(slot)     = ty; OAM_TILE(slot)     = 0x12; OAM_ATTR(slot)     = attr; OAM_X(slot)     = xl;` |
| v0.c:71 | `_update` | 793 | 4.6% | `e->x -= e->spd;` |
| v0.c:89 | `_fill_oam` | 690 | 4.0% | `draw_char(slot, (unsigned char)(enemies[i].x >> 8), GROUND_Y - CHAR_H,` |
| (asm) | `nmi` | 621 | 3.6% | `` |
| v0.c:63 | `_update` | 545 | 3.2% | `e = &enemies[i];` |
| v0.c:68 | `_update` | 489 | 2.8% | `if (e->x < e->spd + ((unsigned int)MIN_X << 8)) {` |
| nes.c:24 | `_pad_read` | 421 | 2.4% | `` |
| v0.c:65 | `_update` | 313 | 1.8% | `e->x += e->spd;` |
| v0.c:39 | `_draw_char` | 306 | 1.8% | `unsigned char attr = pal \| (flip ? 0x40 : 0);` |
| v0.c:40 | `_draw_char` | 300 | 1.7% | `unsigned char xl = flip ? x + 8 : x;` |
| v0.c:41 | `_draw_char` | 300 | 1.7% | `unsigned char xr = flip ? x : x + 8;` |
