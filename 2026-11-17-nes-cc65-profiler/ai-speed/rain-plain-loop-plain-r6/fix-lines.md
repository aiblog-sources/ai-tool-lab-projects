| line | function | cycles/frame | busy | source |
| --- | --- | --- | --- | --- |
| fix.c:128 | `_main` | 1,600 | 10.3% | `y = dy[i] + dsp[i];` |
| fix.c:127 | `_main` | 1,016 | 6.6% | `for (i = 0; i < NUM_DROPS; ++i) {` |
| fix.c:194 | `_main` | 882 | 5.7% | `DROP_OAM_Y[o] = dy[i];` |
| fix.c:195 | `_main` | 882 | 5.7% | `DROP_OAM_X[o] = dx[i];` |
| fix.c:199 | `_main` | 838 | 5.4% | `DROP_OAM_Y[o] = dy[i];` |
| fix.c:200 | `_main` | 838 | 5.4% | `DROP_OAM_X[o] = dx[i];` |
| fix.c:179 | `_main` | 827 | 5.3% | `dy[i] = y;` |
| (asm) | `nmi` | 621 | 4.0% | `` |
| fix.c:198 | `_main` | 522 | 3.4% | `for (i = 0; i < rot; ++i) {` |
| fix.c:193 | `_main` | 509 | 3.3% | `for (i = rot; i < NUM_DROPS; ++i) {` |
| nes.c:24 | `_pad_read` | 421 | 2.7% | `` |
| fix.c:176 | `_main` | 402 | 2.6% | `++q;` |
| fix.c:169 | `_main` | 400 | 2.6% | `++q;` |
| fix.c:156 | `_main` | 393 | 2.5% | `++q;` |
| fix.c:196 | `_main` | 348 | 2.3% | `o += 4;` |
