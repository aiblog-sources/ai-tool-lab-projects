| line | function | cycles/frame | busy | source |
| --- | --- | --- | --- | --- |
| nes.c:47 | `_oam_hide_all` | 9,217 | 36.8% | `` |
| v1.c:120 | `_draw` | 2,340 | 9.3% | `put_ship(n, x, y, 1 + r);` |
| v1.c:41 | `_put_ship` | 1,323 | 5.3% | `unsigned char *p = &OAM[(unsigned char)(i << 2)];` |
| v1.c:50 | `_put_ship` | 1,008 | 4.0% | `p[7] = x + 8;` |
| v1.c:42 | `_put_ship` | 819 | 3.3% | `unsigned char flip = attr \| 0x40;   /* horizontal flip for the right half */` |
| v1.c:118 | `_draw` | 768 | 3.1% | `x = form_x + rot * ENEMY_PITCH;` |
| v1.c:43 | `_put_ship` | 735 | 2.9% | `p[0] = y;` |
| v1.c:45 | `_put_ship` | 735 | 2.9% | `p[2] = attr;` |
| v1.c:49 | `_put_ship` | 735 | 2.9% | `p[6] = flip;` |
| v1.c:46 | `_put_ship` | 693 | 2.8% | `p[3] = x;` |
| v1.c:47 | `_put_ship` | 693 | 2.8% | `p[4] = y;` |
| v1.c:40 | `_put_ship` | 630 | 2.5% | `{` |
| v1.c:48 | `_put_ship` | 630 | 2.5% | `p[5] = 0x10;` |
| (asm) | `nmi` | 621 | 2.5% | `` |
| v1.c:44 | `_put_ship` | 588 | 2.3% | `p[1] = 0x10;` |
