| line | function | cycles/frame | busy | source |
| --- | --- | --- | --- | --- |
| fix.c:179 | `_main` | 1,200 | 7.1% | `y = o + dsp[i];` |
| fix.c:204 | `_main` | 1,128 | 6.6% | `p += 4;` |
| fix.c:209 | `_main` | 1,072 | 6.3% | `p += 4;` |
| fix.c:203 | `_main` | 1,025 | 6.0% | `p[3] = dx[i];` |
| fix.c:208 | `_main` | 975 | 5.7% | `p[3] = dx[i];` |
| fix.c:177 | `_main` | 974 | 5.7% | `for (i = 0; i < NUM_DROPS; ++i) {` |
| fix.c:202 | `_main` | 840 | 4.9% | `p[0] = dy[i];` |
| fix.c:207 | `_main` | 800 | 4.7% | `p[0] = dy[i];` |
| fix.c:178 | `_main` | 760 | 4.5% | `o = dy[i];` |
| fix.c:181 | `_main` | 707 | 4.2% | `dy[i] = y;` |
| fix.c:130 | `_step_band` | 657 | 3.9% | `for (j = s; j <= bHi; ++j) ++band_cnt[j - UMB_Y];` |
| fix.c:127 | `_step_band` | 653 | 3.8% | `for (j = aLo; j <= e; ++j) --band_cnt[j - UMB_Y];` |
| (asm) | `nmi` | 621 | 3.7% | `` |
| fix.c:206 | `_main` | 522 | 3.1% | `for (i = 0; i < rot; ++i) {` |
| fix.c:201 | `_main` | 510 | 3.0% | `for (i = rot; i < NUM_DROPS; ++i) {` |
