| line | function | cycles/frame | busy | source |
| --- | --- | --- | --- | --- |
| fix.c:147 | `_main` | 736 | 5.7% | `for (i = 0; i < NUM_DROPS; ++i) {` |
| fix.c:149 | `_main` | 720 | 5.6% | `y = old + dsp[i];` |
| fix.c:192 | `_main` | 718 | 5.6% | `OAM_DY[off] = dy[k];` |
| fix.c:193 | `_main` | 718 | 5.6% | `OAM_DX[off] = dx[k];` |
| fix.c:197 | `_main` | 682 | 5.3% | `OAM_DY[off] = dy[k];` |
| fix.c:198 | `_main` | 682 | 5.3% | `OAM_DX[off] = dx[k];` |
| fix.c:62 | `_band_inc` | 670 | 5.2% | `++band_cnt[bl];` |
| fix.c:70 | `_band_dec` | 670 | 5.2% | `--band_cnt[bl];` |
| (asm) | `nmi` | 621 | 4.8% | `` |
| fix.c:148 | `_main` | 480 | 3.7% | `old = dy[i];` |
| fix.c:152 | `_main` | 437 | 3.4% | `dy[i] = y;` |
| nes.c:24 | `_pad_read` | 421 | 3.3% | `` |
| fix.c:196 | `_main` | 388 | 3.0% | `for (k = 0; k < rot; ++k) {` |
| fix.c:191 | `_main` | 366 | 2.8% | `for (k = rot; k < NUM_DROPS; ++k) {` |
| fix.c:63 | `_band_inc` | 361 | 2.8% | `} while (++bl <= bh);` |
