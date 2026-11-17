| line | function | cycles/frame | busy | source |
| --- | --- | --- | --- | --- |
| fix.c:63 | `_band_add` | 4,051 | 22.7% | `for (; l <= hi; ++l) band_cnt[l - UMB_Y] += d;` |
| fix.c:175 | `_main` | 743 | 4.2% | `p[3] = dx[i];` |
| fix.c:75 | `_band_fits` | 724 | 4.1% | `if (band_cnt[l - UMB_Y] >= BAND_MAX) return 0;` |
| fix.c:148 | `_main` | 718 | 4.0% | `for (i = 0; i < NUM_DROPS; ++i) {` |
| fix.c:74 | `_band_fits` | 710 | 4.0% | `for (; l <= hi; ++l) {` |
| fix.c:150 | `_main` | 703 | 3.9% | `y = oldy + dsp[i];` |
| fix.c:181 | `_main` | 702 | 3.9% | `p[3] = dx[i];` |
| (asm) | `nmi` | 609 | 3.4% | `` |
| fix.c:174 | `_main` | 602 | 3.4% | `p[0] = dy[i];` |
| fix.c:151 | `_main` | 592 | 3.3% | `if (oldy >= BAND_LO && oldy < BAND_HI) band_add(oldy, -1);` |
| fix.c:180 | `_main` | 570 | 3.2% | `p[0] = dy[i];` |
| fix.c:163 | `_main` | 500 | 2.8% | `dy[i] = y;` |
| fix.c:149 | `_main` | 469 | 2.6% | `oldy = dy[i];` |
| fix.c:165 | `_main` | 469 | 2.6% | `oldy = dy[i];` |
| fix.c:166 | `_main` | 459 | 2.6% | `if (oldy >= BAND_LO && oldy < BAND_HI) band_add(oldy, 1);` |
