| line | function | cycles/frame | busy | source |
| --- | --- | --- | --- | --- |
| fix.c:52 | `_band_inc` | 1,346 | 5.8% | `for (; l <= hi; ++l) ++band_cnt[l];` |
| fix.c:61 | `_band_dec` | 1,344 | 5.8% | `for (; l <= hi; ++l) --band_cnt[l];` |
| fix.c:142 | `_main` | 1,108 | 4.7% | `y = old + dsp[i];` |
| fix.c:169 | `_main` | 1,040 | 4.5% | `p += 4;` |
| fix.c:176 | `_main` | 992 | 4.2% | `p += 4;` |
| fix.c:168 | `_main` | 945 | 4.0% | `p[3] = dx[i];` |
| fix.c:140 | `_main` | 938 | 4.0% | `for (i = 0; i < NUM_DROPS; ++i) {` |
| fix.c:175 | `_main` | 901 | 3.9% | `p[3] = dx[i];` |
| fix.c:157 | `_main` | 776 | 3.3% | `nd = dy[i];` |
| fix.c:165 | `_main` | 775 | 3.3% | `p[0] = dy[i];` |
| fix.c:155 | `_main` | 764 | 3.3% | `dy[i] = y;` |
| fix.c:172 | `_main` | 739 | 3.2% | `p[0] = dy[i];` |
| fix.c:143 | `_main` | 718 | 3.1% | `if (IN_BAND(old)) band_dec(old);` |
| fix.c:141 | `_main` | 702 | 3.0% | `old = dy[i];` |
| fix.c:71 | `_band_ok` | 681 | 2.9% | `for (; l <= hi; ++l) {` |
