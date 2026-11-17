| line | function | cycles/frame | busy | source |
| --- | --- | --- | --- | --- |
| v1.c:58 | `_put` | 2,230 | 13.8% | `unsigned char *o = OAM + ((unsigned int)idx << 2);` |
| v1.c:141 | `_draw` | 2,227 | 13.8% | `OAM_Y(idx) = 0xFF;` |
| v1.c:134 | `_draw` | 1,025 | 6.3% | `unsigned char x = form_x + i * SPACING;` |
| v1.c:59 | `_put` | 945 | 5.8% | `o[0] = y - 1;                   /* OAM Y is top line - 1 */` |
| v1.c:60 | `_put` | 848 | 5.2% | `o[1] = tile;` |
| v1.c:62 | `_put` | 848 | 5.2% | `o[3] = x;` |
| v1.c:61 | `_put` | 751 | 4.6% | `o[2] = attr;` |
| v1.c:73 | `_put_big` | 738 | 4.6% | `put(x + 8, y + 8, tile, pal_no \| 0xC0);` |
| v1.c:57 | `_put` | 727 | 4.5% | `{` |
| v1.c:71 | `_put_big` | 714 | 4.4% | `put(x + 8, y,     tile, pal_no \| 0x40);` |
| v1.c:72 | `_put_big` | 714 | 4.4% | `put(x,     y + 8, tile, pal_no \| 0x80);` |
| v1.c:70 | `_put_big` | 678 | 4.2% | `put(x,     y,     tile, pal_no);` |
| (asm) | `nmi` | 621 | 3.8% | `` |
| v1.c:140 | `_draw` | 445 | 2.8% | `while (idx < 64) {` |
| nes.c:24 | `_pad_read` | 421 | 2.6% | `` |
