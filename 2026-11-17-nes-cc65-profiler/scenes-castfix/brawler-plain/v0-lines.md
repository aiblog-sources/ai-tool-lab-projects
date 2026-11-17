| line | function | cycles/frame | busy | source |
| --- | --- | --- | --- | --- |
| nes.c:47 | `_oam_hide_all` | 9,217 | 55.6% | `` |
| v0.c:78 | `_update_enemies` | 823 | 5.0% | `ex[i] -= espeed[i];` |
| v0.c:48 | `_draw_char` | 680 | 4.1% | `p[12] = sy + 16; p[13] = 0x10; p[14] = pal \| 0x40; p[15] = x + 8;` |
| v0.c:45 | `_draw_char` | 636 | 3.8% | `p[4] = sy;      p[5] = 0x12; p[6] = pal \| 0x40; p[7] = x + 8;` |
| (asm) | `nmi` | 621 | 3.7% | `` |
| v0.c:47 | `_draw_char` | 584 | 3.5% | `p[8]  = sy + 16; p[9]  = 0x10; p[10] = pal;        p[11] = x;` |
| v0.c:44 | `_draw_char` | 516 | 3.1% | `p[0] = sy;      p[1] = 0x12; p[2] = pal;        p[3] = x;` |
| v0.c:74 | `_update_enemies` | 476 | 2.9% | `if (ex[i] < espeed[i]) {` |
| nes.c:24 | `_pad_read` | 421 | 2.5% | `` |
| v0.c:99 | `_fill_oam` | 369 | 2.2% | `draw_char((unsigned char)(ex[i] >> 8), GROUND_Y, epal[i]);` |
| v0.c:40 | `_draw_char` | 368 | 2.2% | `unsigned char *p = OAM + ((unsigned int)oam_n << 2);` |
| v0.c:68 | `_update_enemies` | 343 | 2.1% | `ex[i] += espeed[i];` |
| v0.c:41 | `_draw_char` | 164 | 1.0% | `unsigned char sy = y - 1;          /* OAM Y is one line above */` |
| v0.c:66 | `_update_enemies` | 155 | 0.9% | `for (i = 0; i < NUM_ENEMIES; ++i) {` |
| v0.c:98 | `_fill_oam` | 136 | 0.8% | `if (i == h1 \|\| i == h2) continue;` |
