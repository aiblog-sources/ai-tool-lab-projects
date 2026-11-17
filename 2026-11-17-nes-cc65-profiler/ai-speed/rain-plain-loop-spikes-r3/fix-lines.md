| line | function | cycles/frame | busy | source |
| --- | --- | --- | --- | --- |
| fix.c:141 | `_main` | 736 | 5.3% | `for (i = 0; i < NUM_DROPS; ++i) {` |
| fix.c:143 | `_main` | 720 | 5.2% | `y = oy + dsp[i];` |
| fix.c:178 | `_main` | 718 | 5.2% | `((unsigned char *)0x0220)[x] = dy[k];` |
| fix.c:179 | `_main` | 718 | 5.2% | `((unsigned char *)0x0223)[x] = dx[k];` |
| fix.c:183 | `_main` | 682 | 4.9% | `((unsigned char *)0x0220)[x] = dy[k];` |
| fix.c:184 | `_main` | 682 | 4.9% | `((unsigned char *)0x0223)[x] = dx[k];` |
| fix.c:58 | `_band_inc` | 670 | 4.8% | `++band_cnt[bl];` |
| fix.c:68 | `_band_dec` | 670 | 4.8% | `--band_cnt[bl];` |
| (asm) | `nmi` | 621 | 4.5% | `` |
| fix.c:156 | `_main` | 513 | 3.7% | `dy[i] = y;` |
| fix.c:142 | `_main` | 480 | 3.5% | `oy = dy[i];` |
| fix.c:158 | `_main` | 480 | 3.5% | `t = dy[i];` |
| fix.c:144 | `_main` | 447 | 3.2% | `if (IN_BAND(oy)) band_dec(oy);` |
| nes.c:24 | `_pad_read` | 421 | 3.0% | `` |
| fix.c:147 | `_main` | 414 | 3.0% | `} else if (IN_BAND(y)) {` |
