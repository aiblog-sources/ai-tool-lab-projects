| line | function | cycles/frame | busy | source |
| --- | --- | --- | --- | --- |
| fix.c:65 | `_band_inc` | 1,543 | 9.1% | `for (; l <= h; ++l) ++band_cnt[l];` |
| fix.c:72 | `_band_dec` | 1,538 | 9.1% | `for (; l <= h; ++l) --band_cnt[l];` |
| fix.c:151 | `_main` | 1,200 | 7.1% | `y = o + dsp[i];` |
| fix.c:149 | `_main` | 1,017 | 6.0% | `for (i = 0; i < NUM_DROPS; ++i) {` |
| fix.c:192 | `_main` | 882 | 5.2% | `OAMYP[j] = dy[i];` |
| fix.c:193 | `_main` | 882 | 5.2% | `OAMXP[j] = dx[i];` |
| fix.c:197 | `_main` | 838 | 4.9% | `OAMYP[j] = dy[i];` |
| fix.c:198 | `_main` | 838 | 4.9% | `OAMXP[j] = dx[i];` |
| fix.c:79 | `_band_ok` | 778 | 4.6% | `for (; l <= h; ++l) {` |
| fix.c:150 | `_main` | 760 | 4.5% | `o = dy[i];` |
| fix.c:153 | `_main` | 706 | 4.2% | `dy[i] = y;` |
| (asm) | `nmi` | 621 | 3.7% | `` |
| fix.c:196 | `_main` | 522 | 3.1% | `for (i = 0; i < rot; ++i) {` |
| fix.c:191 | `_main` | 509 | 3.0% | `for (i = rot; i < NUM_DROPS; ++i) {` |
| nes.c:24 | `_pad_read` | 421 | 2.5% | `` |
