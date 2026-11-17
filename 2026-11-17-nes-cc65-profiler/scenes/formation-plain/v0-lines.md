| line | function | cycles/frame | busy | source |
| --- | --- | --- | --- | --- |
| v0.c:56 | `_put` | 2,845 | 14.6% | `unsigned char *o = OAM + ((unsigned int)idx << 2);` |
| v0.c:139 | `_draw` | 1,852 | 9.5% | `OAM_Y(idx) = 0xFF;` |
| v0.c:132 | `_draw` | 1,368 | 7.0% | `unsigned char x = form_x + i * SPACING;` |
| v0.c:57 | `_put` | 1,206 | 6.2% | `o[0] = y - 1;                   /* OAM Y is top line - 1 */` |
| v0.c:58 | `_put` | 1,082 | 5.6% | `o[1] = tile;` |
| v0.c:60 | `_put` | 1,082 | 5.6% | `o[3] = x;` |
| v0.c:59 | `_put` | 959 | 4.9% | `o[2] = attr;` |
| v0.c:71 | `_put_big` | 944 | 4.8% | `put(x + 8, y + 8, tile, pal_no \| 0xC0);` |
| v0.c:55 | `_put` | 928 | 4.8% | `{` |
| v0.c:69 | `_put_big` | 913 | 4.7% | `put(x + 8, y,     tile, pal_no \| 0x40);` |
| v0.c:70 | `_put_big` | 913 | 4.7% | `put(x,     y + 8, tile, pal_no \| 0x80);` |
| v0.c:68 | `_put_big` | 867 | 4.4% | `put(x,     y,     tile, pal_no);` |
| (asm) | `nmi` | 621 | 3.2% | `` |
| v0.c:130 | `_draw` | 589 | 3.0% | `phase = frame % 3;` |
| nes.c:24 | `_pad_read` | 421 | 2.2% | `` |
