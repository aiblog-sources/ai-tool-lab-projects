| line | function | cycles/frame | busy | source |
| --- | --- | --- | --- | --- |
| fix.c:54 | `_band_add` | 3,913 | 18.1% | `for (; l <= hi; ++l) band_cnt[l - UMB_Y] += d;` |
| fix.c:130 | `_main` | 1,491 | 6.9% | `y = dy[i] + dsp[i];` |
| fix.c:165 | `_main` | 1,049 | 4.8% | `p += 4;` |
| fix.c:170 | `_main` | 1,001 | 4.6% | `p += 4;` |
| fix.c:164 | `_main` | 954 | 4.4% | `p[3] = dx[i];` |
| fix.c:129 | `_main` | 946 | 4.4% | `for (i = 0; i < NUM_DROPS; ++i) {` |
| fix.c:169 | `_main` | 910 | 4.2% | `p[3] = dx[i];` |
| fix.c:163 | `_main` | 782 | 3.6% | `p[0] = dy[i];` |
| fix.c:168 | `_main` | 746 | 3.4% | `p[0] = dy[i];` |
| fix.c:50 | `_band_add` | 714 | 3.3% | `if (y + 8 <= UMB_Y \|\| y >= UMB_Y + UMB_H) return;` |
| fix.c:66 | `_band_fits` | 699 | 3.2% | `if (band_cnt[l - UMB_Y] >= BAND_MAX) return 0;` |
| fix.c:65 | `_band_fits` | 686 | 3.2% | `for (; l <= hi; ++l) {` |
| fix.c:132 | `_main` | 658 | 3.0% | `dy[i] = y;` |
| (asm) | `nmi` | 585 | 2.7% | `` |
| fix.c:167 | `_main` | 488 | 2.3% | `for (i = 0; i < rot; ++i) {` |
