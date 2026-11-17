| line | function | cycles/frame | busy | source |
| --- | --- | --- | --- | --- |
| v0.c:90 | `_draw` | 10,656 | 36.3% | `put_sprite(i++, bx + (c << 3), by + (r << 3), tile, 0);` |
| v0.c:59 | `_put_sprite` | 1,824 | 6.2% | `OAM_Y(i)    = (unsigned char)(y - 1);` |
| v0.c:60 | `_put_sprite` | 1,728 | 5.9% | `OAM_TILE(i) = tile;` |
| v0.c:62 | `_put_sprite` | 1,728 | 5.9% | `OAM_X(i)    = (unsigned char)x;` |
| v0.c:61 | `_put_sprite` | 1,632 | 5.6% | `OAM_ATTR(i) = attr;` |
| v0.c:99 | `_draw` | 1,464 | 5.0% | `cx + cos64(a) - 4,` |
| v0.c:58 | `_put_sprite` | 1,440 | 4.9% | `{` |
| v0.c:86 | `_draw` | 1,002 | 3.4% | `for (c = 0; c < 6; ++c) {` |
| (asm) | `nmi` | 621 | 2.1% | `` |
| v0.c:26 | `_sin64` | 547 | 1.9% | `return -qsin[64 - i];` |
| v0.c:88 | `_draw` | 521 | 1.8% | `if (r == 2 && (c == 1 \|\| c == 4)) tile = 0x03;      /* eyes */` |
| v0.c:21 | `_sin64` | 510 | 1.7% | `{` |
| v0.c:100 | `_draw` | 476 | 1.6% | `BOSS_CY + sin64(a) - 4,` |
| v0.c:89 | `_draw` | 473 | 1.6% | `else if (r == 4 && (c == 2 \|\| c == 3)) tile = 0x02; /* mouth */` |
| nes.c:24 | `_pad_read` | 421 | 1.4% | `` |
