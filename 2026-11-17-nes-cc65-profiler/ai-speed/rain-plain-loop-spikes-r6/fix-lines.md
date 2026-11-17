| line | function | cycles/frame | busy | source |
| --- | --- | --- | --- | --- |
| fix.c:190 | `_main` | 1,480 | 12.0% | `o[3] = dx[i];` |
| fix.c:189 | `_main` | 1,200 | 9.7% | `o[0] = dy[i];` |
| fix.c:141 | `_main` | 736 | 6.0% | `for (i = 0; i < NUM_DROPS; ++i) {` |
| fix.c:143 | `_main` | 720 | 5.8% | `y = old + dsp[i];` |
| fix.c:188 | `_main` | 695 | 5.6% | `for (k = 0; k < NUM_DROPS; ++k) {` |
| (asm) | `nmi` | 621 | 5.0% | `` |
| fix.c:191 | `_main` | 600 | 4.9% | `o += 4;` |
| fix.c:175 | `_main` | 564 | 4.6% | `for (; l <= hi; ++l) ++band_cnt[l - UMB_Y];` |
| fix.c:180 | `_main` | 549 | 4.4% | `for (; l <= hi; ++l) --band_cnt[l - UMB_Y];` |
| fix.c:181 | `_main` | 513 | 4.2% | `dy[i] = y;` |
| fix.c:142 | `_main` | 480 | 3.9% | `old = dy[i];` |
| nes.c:24 | `_pad_read` | 421 | 3.4% | `` |
| fix.c:193 | `_main` | 363 | 2.9% | `if (i == NUM_DROPS) i = 0;` |
| fix.c:144 | `_main` | 320 | 2.6% | `if (y <= UMB_Y - 8) {` |
| fix.c:169 | `_main` | 303 | 2.5% | `for (t = l; t <= hi; ++t) {` |
