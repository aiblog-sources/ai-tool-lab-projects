| line | function | cycles/frame | busy | source |
| --- | --- | --- | --- | --- |
| v1.c:46 | `_band_add` | 2,166 | 9.7% | `if (y + 8 <= UMB_Y \|\| y >= UMB_Y + UMB_H) return;` |
| v1.c:50 | `_band_add` | 2,135 | 9.5% | `for (; l <= hi; ++l) band_cnt[l - UMB_Y] += d;` |
| v1.c:127 | `_main` | 1,640 | 7.3% | `band_add(dy[i], -1);` |
| v1.c:138 | `_main` | 1,640 | 7.3% | `band_add(dy[i], 1);` |
| v1.c:44 | `_band_add` | 1,200 | 5.4% | `{` |
| v1.c:57 | `_band_fits` | 1,178 | 5.3% | `if (y + 8 <= UMB_Y \|\| y >= UMB_Y + UMB_H) return 1;` |
| v1.c:134 | `_main` | 947 | 4.2% | `} else if (band_fits(y)) {` |
| v1.c:126 | `_main` | 880 | 3.9% | `top = y;` |
| v1.c:150 | `_main` | 840 | 3.8% | `OAM_X(slot)    = dx[i];` |
| v1.c:125 | `_main` | 800 | 3.6% | `y = dy[i] + dsp[i];` |
| v1.c:130 | `_main` | 715 | 3.2% | `} else if (top + 8 > UMB_Y && top < UMB_Y + 16 &&` |
| v1.c:147 | `_main` | 660 | 2.9% | `OAM_Y(slot)    = dy[i];` |
| v1.c:148 | `_main` | 620 | 2.8% | `OAM_TILE(slot) = DROP_TILE;` |
| v1.c:149 | `_main` | 620 | 2.8% | `OAM_ATTR(slot) = 0;` |
| v1.c:55 | `_band_fits` | 592 | 2.6% | `{` |
