| line | function | cycles/frame | busy | source |
| --- | --- | --- | --- | --- |
| fix.c:54 | `_band_add` | 3,925 | 18.2% | `for (; l <= hi; ++l) band_cnt[l - UMB_Y] += d;` |
| fix.c:131 | `_main` | 1,127 | 5.2% | `y = o + dsp[i];` |
| fix.c:167 | `_main` | 1,062 | 4.9% | `p += 4;` |
| fix.c:172 | `_main` | 1,004 | 4.7% | `p += 4;` |
| fix.c:166 | `_main` | 966 | 4.5% | `p[3] = dx[i];` |
| fix.c:129 | `_main` | 954 | 4.4% | `for (i = 0; i < NUM_DROPS; ++i) {` |
| fix.c:171 | `_main` | 913 | 4.2% | `p[3] = dx[i];` |
| fix.c:165 | `_main` | 792 | 3.7% | `p[0] = dy[i];` |
| fix.c:170 | `_main` | 748 | 3.5% | `p[0] = dy[i];` |
| fix.c:130 | `_main` | 714 | 3.3% | `o = dy[i];` |
| fix.c:66 | `_band_fits` | 701 | 3.3% | `if (band_cnt[l - UMB_Y] >= BAND_MAX) return 0;` |
| fix.c:65 | `_band_fits` | 688 | 3.2% | `for (; l <= hi; ++l) {` |
| fix.c:134 | `_main` | 664 | 3.1% | `dy[i] = y;` |
| (asm) | `nmi` | 589 | 2.7% | `` |
| fix.c:149 | `_main` | 489 | 2.3% | `band_add(dy[i], 1);` |
