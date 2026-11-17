| line | function | cycles/frame | busy | source |
| --- | --- | --- | --- | --- |
| fix.c:55 | `_band_add` | 3,886 | 18.4% | `for (; l <= hi; ++l) band_cnt[l - UMB_Y] += d;` |
| fix.c:133 | `_main` | 1,123 | 5.3% | `if (y < DROP_BOTTOM && !IN_BAND(y) && !IN_BAND(old)) {` |
| fix.c:132 | `_main` | 1,105 | 5.2% | `y = old + dsp[i];` |
| fix.c:163 | `_main` | 1,040 | 4.9% | `p += 4;` |
| fix.c:168 | `_main` | 986 | 4.7% | `p += 4;` |
| fix.c:162 | `_main` | 945 | 4.5% | `p[3] = dx[i];` |
| fix.c:130 | `_main` | 935 | 4.4% | `for (i = 0; i < NUM_DROPS; ++i) {` |
| fix.c:167 | `_main` | 896 | 4.2% | `p[3] = dx[i];` |
| fix.c:161 | `_main` | 775 | 3.7% | `p[0] = dy[i];` |
| fix.c:166 | `_main` | 735 | 3.5% | `p[0] = dy[i];` |
| fix.c:131 | `_main` | 700 | 3.3% | `old = dy[i];` |
| fix.c:67 | `_band_fits` | 695 | 3.3% | `if (band_cnt[l - UMB_Y] >= BAND_MAX) return 0;` |
| fix.c:134 | `_main` | 689 | 3.3% | `dy[i] = y;      /* far from the umbrella band: plain move */` |
| fix.c:66 | `_band_fits` | 682 | 3.2% | `for (; l <= hi; ++l) {` |
| (asm) | `nmi` | 579 | 2.7% | `` |
