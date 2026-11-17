| line | function | cycles/frame | busy | source |
| --- | --- | --- | --- | --- |
| fix.c:162 | `_main` | 841 | 6.9% | `} else if (!IN_BAND(old) && !IN_BAND(y)) {` |
| fix.c:156 | `_main` | 736 | 6.0% | `for (i = 0; i < NUM_DROPS; ++i) {` |
| fix.c:158 | `_main` | 720 | 5.9% | `y = old + dsp[i];` |
| fix.c:203 | `_main` | 718 | 5.9% | `DROP_OAM_Y[j] = dy[i];` |
| fix.c:204 | `_main` | 718 | 5.9% | `DROP_OAM_X[j] = dx[i];` |
| fix.c:208 | `_main` | 682 | 5.6% | `DROP_OAM_Y[j] = dy[i];` |
| fix.c:209 | `_main` | 682 | 5.6% | `DROP_OAM_X[j] = dx[i];` |
| (asm) | `nmi` | 621 | 5.1% | `` |
| fix.c:61 | `_rng_inc` | 567 | 4.6% | `for (j = lo; j <= hi; ++j) ++band_cnt[j - UMB_Y];` |
| fix.c:66 | `_rng_dec` | 562 | 4.6% | `for (j = lo; j <= hi; ++j) --band_cnt[j - UMB_Y];` |
| fix.c:181 | `_main` | 557 | 4.6% | `if (hi > y - 1) hi = y - 1;` |
| fix.c:184 | `_main` | 513 | 4.2% | `dy[i] = y;` |
| fix.c:157 | `_main` | 480 | 3.9% | `old = dy[i];` |
| nes.c:24 | `_pad_read` | 421 | 3.5% | `` |
| fix.c:207 | `_main` | 388 | 3.2% | `for (i = 0; i < rot; ++i) {` |
