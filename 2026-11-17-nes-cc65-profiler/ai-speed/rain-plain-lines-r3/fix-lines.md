| line | function | cycles/frame | busy | source |
| --- | --- | --- | --- | --- |
| fix.c:53 | `_band_add` | 3,963 | 18.5% | `for (; l <= hi; ++l) band_cnt[l - UMB_Y] += d;` |
| fix.c:127 | `_main` | 1,147 | 5.4% | `if (y < DROP_BOTTOM && !IN_BAND(y) && !IN_BAND(oy)) {` |
| fix.c:126 | `_main` | 1,125 | 5.3% | `y = oy + dsp[i];` |
| fix.c:160 | `_main` | 1,060 | 5.0% | `p += 4;` |
| fix.c:165 | `_main` | 1,003 | 4.7% | `p += 4;` |
| fix.c:159 | `_main` | 963 | 4.5% | `p[3] = dx[i];` |
| fix.c:124 | `_main` | 952 | 4.4% | `for (i = 0; i < NUM_DROPS; ++i) {` |
| fix.c:164 | `_main` | 912 | 4.3% | `p[3] = dx[i];` |
| fix.c:158 | `_main` | 790 | 3.7% | `p[0] = dy[i];` |
| fix.c:163 | `_main` | 748 | 3.5% | `p[0] = dy[i];` |
| fix.c:125 | `_main` | 712 | 3.3% | `oy = dy[i];` |
| fix.c:65 | `_band_fits` | 708 | 3.3% | `if (band_cnt[l - UMB_Y] >= BAND_MAX) return 0;` |
| fix.c:128 | `_main` | 701 | 3.3% | `dy[i] = y;          /* fast path: band not involved */` |
| fix.c:64 | `_band_fits` | 695 | 3.2% | `for (; l <= hi; ++l) {` |
| (asm) | `nmi` | 588 | 2.7% | `` |
