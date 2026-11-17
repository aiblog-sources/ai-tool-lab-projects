| line | function | cycles/frame | busy | source |
| --- | --- | --- | --- | --- |
| v0.c:90 | `_main` | 1,760 | 9.2% | `top = y;` |
| v0.c:111 | `_main` | 1,680 | 8.8% | `OAM_X(slot)    = dx[i];` |
| v0.c:89 | `_main` | 1,600 | 8.4% | `y = dy[i] + dsp[i];` |
| v0.c:93 | `_main` | 1,428 | 7.5% | `} else if (top + 8 > UMB_Y && top < UMB_Y + 16 &&` |
| v0.c:108 | `_main` | 1,320 | 6.9% | `OAM_Y(slot)    = dy[i];` |
| v0.c:109 | `_main` | 1,240 | 6.5% | `OAM_TILE(slot) = DROP_TILE;` |
| v0.c:110 | `_main` | 1,240 | 6.5% | `OAM_ATTR(slot) = 0;` |
| v0.c:88 | `_main` | 1,016 | 5.3% | `for (i = 0; i < NUM_DROPS; ++i) {` |
| v0.c:104 | `_main` | 975 | 5.1% | `for (k = 0; k < NUM_DROPS; ++k) {` |
| v0.c:98 | `_main` | 829 | 4.3% | `dy[i] = y;` |
| v0.c:105 | `_main` | 760 | 4.0% | `i = k + rot;` |
| v0.c:107 | `_main` | 760 | 4.0% | `slot = 8 + k;` |
| (asm) | `nmi` | 621 | 3.3% | `` |
| v0.c:91 | `_main` | 480 | 2.5% | `if (y >= DROP_BOTTOM) {` |
| v0.c:106 | `_main` | 473 | 2.5% | `if (i >= NUM_DROPS) i -= NUM_DROPS;` |
