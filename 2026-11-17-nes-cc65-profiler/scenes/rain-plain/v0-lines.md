| line | function | cycles/frame | busy | source |
| --- | --- | --- | --- | --- |
| v0.c:111 | `_main` | 3,160 | 12.1% | `OAM_X(slot)    = dx[i];` |
| v0.c:108 | `_main` | 2,800 | 10.7% | `OAM_Y(slot)    = dy[i];` |
| v0.c:109 | `_main` | 2,720 | 10.4% | `OAM_TILE(slot) = DROP_TILE;` |
| v0.c:110 | `_main` | 2,720 | 10.4% | `OAM_ATTR(slot) = 0;` |
| v0.c:90 | `_main` | 1,760 | 6.7% | `top = y;` |
| v0.c:89 | `_main` | 1,600 | 6.1% | `y = dy[i] + dsp[i];` |
| v0.c:93 | `_main` | 1,428 | 5.5% | `} else if (top + 8 > UMB_Y && top < UMB_Y + 16 &&` |
| v0.c:104 | `_main` | 1,017 | 3.9% | `for (k = 0; k < NUM_DROPS; ++k) {` |
| v0.c:88 | `_main` | 1,016 | 3.9% | `for (i = 0; i < NUM_DROPS; ++i) {` |
| v0.c:98 | `_main` | 829 | 3.2% | `dy[i] = y;` |
| v0.c:105 | `_main` | 760 | 2.9% | `i = k + rot;` |
| v0.c:107 | `_main` | 760 | 2.9% | `slot = 8 + k;` |
| v0.c:52 | `_draw_umbrella` | 664 | 2.5% | `OAM_X(n)    = ux + (c << 3);` |
| (asm) | `nmi` | 621 | 2.4% | `` |
| v0.c:50 | `_draw_umbrella` | 616 | 2.4% | `OAM_TILE(n) = row ? 0x02 : 0x01;` |
