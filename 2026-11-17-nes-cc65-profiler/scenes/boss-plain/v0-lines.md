| line | function | cycles/frame | busy | source |
| --- | --- | --- | --- | --- |
| v0.c:90 | `_draw` | 5,328 | 29.4% | `put_sprite(i++, bx + (c << 3), by + (r << 3), tile, 0);` |
| v0.c:60 | `_put_sprite` | 1,752 | 9.7% | `OAM_TILE(i) = tile;` |
| v0.c:62 | `_put_sprite` | 1,752 | 9.7% | `OAM_X(i)    = (unsigned char)x;` |
| v0.c:61 | `_put_sprite` | 1,704 | 9.4% | `OAM_ATTR(i) = attr;` |
| v0.c:59 | `_put_sprite` | 1,680 | 9.3% | `OAM_Y(i)    = (unsigned char)(y - 1);` |
| v0.c:99 | `_draw` | 732 | 4.0% | `cx + cos64(a) - 4,` |
| v0.c:58 | `_put_sprite` | 720 | 4.0% | `{` |
| v0.c:86 | `_draw` | 501 | 2.8% | `for (c = 0; c < 6; ++c) {` |
| (asm) | `nmi` | 356 | 2.0% | `` |
| v0.c:26 | `_sin64` | 271 | 1.5% | `return -qsin[64 - i];` |
| v0.c:88 | `_draw` | 260 | 1.4% | `if (r == 2 && (c == 1 \|\| c == 4)) tile = 0x03;      /* eyes */` |
| v0.c:21 | `_sin64` | 255 | 1.4% | `{` |
| v0.c:100 | `_draw` | 238 | 1.3% | `BOSS_CY + sin64(a) - 4,` |
| v0.c:89 | `_draw` | 236 | 1.3% | `else if (r == 4 && (c == 2 \|\| c == 3)) tile = 0x02; /* mouth */` |
| nes.c:24 | `_pad_read` | 210 | 1.2% | `` |
