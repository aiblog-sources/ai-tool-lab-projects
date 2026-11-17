| line | function | cycles/frame | busy | source |
| --- | --- | --- | --- | --- |
| v1.c:46 | `_band_add` | 2,133 | 8.4% | `if (y + 8 <= UMB_Y \|\| y >= UMB_Y + UMB_H) return;` |
| v1.c:50 | `_band_add` | 2,070 | 8.1% | `for (; l <= hi; ++l) band_cnt[l - UMB_Y] += d;` |
| v1.c:127 | `_main` | 1,615 | 6.3% | `band_add(dy[i], -1);` |
| v1.c:138 | `_main` | 1,615 | 6.3% | `band_add(dy[i], 1);` |
| v1.c:150 | `_main` | 1,556 | 6.1% | `OAM_X(slot)    = dx[i];` |
| v1.c:147 | `_main` | 1,379 | 5.4% | `OAM_Y(slot)    = dy[i];` |
| v1.c:148 | `_main` | 1,340 | 5.3% | `OAM_TILE(slot) = DROP_TILE;` |
| v1.c:149 | `_main` | 1,340 | 5.3% | `OAM_ATTR(slot) = 0;` |
| v1.c:44 | `_band_add` | 1,182 | 4.6% | `{` |
| v1.c:57 | `_band_fits` | 1,160 | 4.6% | `if (y + 8 <= UMB_Y \|\| y >= UMB_Y + UMB_H) return 1;` |
| v1.c:134 | `_main` | 933 | 3.7% | `} else if (band_fits(y)) {` |
| v1.c:126 | `_main` | 867 | 3.4% | `top = y;` |
| v1.c:125 | `_main` | 788 | 3.1% | `y = dy[i] + dsp[i];` |
| v1.c:130 | `_main` | 687 | 2.7% | `} else if (top + 8 > UMB_Y && top < UMB_Y + 16 &&` |
| v1.c:55 | `_band_fits` | 583 | 2.3% | `{` |
