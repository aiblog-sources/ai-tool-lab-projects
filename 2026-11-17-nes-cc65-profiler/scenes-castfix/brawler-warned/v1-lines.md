| line | function | cycles/frame | busy | source |
| --- | --- | --- | --- | --- |
| v1.c:93 | `_fill_oam` | 3,454 | 25.5% | `1 + (i % 3), enemies[i].dir);` |
| v1.c:46 | `_draw_char` | 906 | 6.7% | `OAM_Y(slot + 1) = ty; OAM_TILE(slot + 1) = 0x12; OAM_ATTR(slot + 1) = attr; OAM_X(slot + 1) = xr;` |
| v1.c:47 | `_draw_char` | 894 | 6.6% | `OAM_Y(slot + 2) = by; OAM_TILE(slot + 2) = 0x10; OAM_ATTR(slot + 2) = attr; OAM_X(slot + 2) = xl;` |
| v1.c:48 | `_draw_char` | 894 | 6.6% | `OAM_Y(slot + 3) = by; OAM_TILE(slot + 3) = 0x10; OAM_ATTR(slot + 3) = attr; OAM_X(slot + 3) = xr;` |
| v1.c:45 | `_draw_char` | 822 | 6.1% | `OAM_Y(slot)     = ty; OAM_TILE(slot)     = 0x12; OAM_ATTR(slot)     = attr; OAM_X(slot)     = xl;` |
| v1.c:71 | `_update` | 793 | 5.8% | `e->x -= e->spd;` |
| v1.c:92 | `_fill_oam` | 690 | 5.1% | `draw_char(slot, (unsigned char)(enemies[i].x >> 8), GROUND_Y - CHAR_H,` |
| (asm) | `nmi` | 621 | 4.6% | `` |
| v1.c:63 | `_update` | 545 | 4.0% | `e = &enemies[i];` |
| v1.c:68 | `_update` | 489 | 3.6% | `if (e->x < e->spd + ((unsigned int)MIN_X << 8)) {` |
| nes.c:24 | `_pad_read` | 421 | 3.1% | `` |
| v1.c:65 | `_update` | 313 | 2.3% | `e->x += e->spd;` |
| v1.c:41 | `_draw_char` | 303 | 2.2% | `unsigned char xr = flip ? x : x + 8;` |
| v1.c:40 | `_draw_char` | 300 | 2.2% | `unsigned char xl = flip ? x + 8 : x;` |
| v1.c:39 | `_draw_char` | 285 | 2.1% | `unsigned char attr = pal \| (flip ? 0x40 : 0);` |
