| line | function | cycles/frame | busy | source |
| --- | --- | --- | --- | --- |
| fix.c:56 | `_band_add` | 1,813 | 9.6% | `*s_p += d;` |
| fix.c:136 | `_main` | 880 | 4.7% | `y = dy[i] + dsp[i];` |
| fix.c:164 | `_main` | 758 | 4.0% | `op[3] = dx[i];` |
| fix.c:135 | `_main` | 736 | 3.9% | `for (i = 0; i < NUM_DROPS; ++i) {` |
| fix.c:171 | `_main` | 722 | 3.8% | `op[3] = dx[i];` |
| (asm) | `nmi` | 621 | 3.3% | `` |
| fix.c:94 | `_draw_umbrella` | 616 | 3.3% | `OAM_X(n)    = ux + (c << 3);` |
| fix.c:161 | `_main` | 615 | 3.3% | `op[0] = dy[i];` |
| fix.c:168 | `_main` | 585 | 3.1% | `op[0] = dy[i];` |
| fix.c:92 | `_draw_umbrella` | 568 | 3.0% | `OAM_TILE(n) = row ? 0x02 : 0x01;` |
| fix.c:91 | `_draw_umbrella` | 560 | 3.0% | `OAM_Y(n)    = UMB_Y + (row << 3);` |
| fix.c:93 | `_draw_umbrella` | 504 | 2.7% | `OAM_ATTR(n) = 1;` |
| fix.c:162 | `_main` | 492 | 2.6% | `op[1] = DROP_TILE;` |
| fix.c:163 | `_main` | 492 | 2.6% | `op[2] = 0;` |
| fix.c:71 | `_band_fits` | 475 | 2.5% | `if (*s_p >= BAND_MAX) return 0;` |
