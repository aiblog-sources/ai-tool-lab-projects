| line | function | cycles/frame | busy | source |
| --- | --- | --- | --- | --- |
| fix.c:189 | `_main` | 758 | 5.2% | `op[3] = dx[i];` |
| fix.c:163 | `_main` | 736 | 5.1% | `for (i = 0; i < NUM_DROPS; ++i) {` |
| fix.c:194 | `_main` | 722 | 5.0% | `op[3] = dx[i];` |
| fix.c:165 | `_main` | 720 | 5.0% | `y = d + dsp[i];` |
| (asm) | `nmi` | 621 | 4.3% | `` |
| fix.c:188 | `_main` | 615 | 4.2% | `op[0] = dy[i];` |
| fix.c:193 | `_main` | 585 | 4.0% | `op[0] = dy[i];` |
| fix.c:62 | `_band_inc` | 572 | 4.0% | `++*s_p;` |
| fix.c:73 | `_band_dec` | 571 | 3.9% | `--*s_p;` |
| fix.c:178 | `_main` | 513 | 3.5% | `dy[i] = y;               /* outside band: always fits */` |
| fix.c:164 | `_main` | 480 | 3.3% | `d = dy[i];` |
| fix.c:180 | `_main` | 480 | 3.3% | `d = dy[i];` |
| fix.c:86 | `_band_fits` | 475 | 3.3% | `if (*s_p >= BAND_MAX) return 0;` |
| fix.c:166 | `_main` | 447 | 3.1% | `if (d >= BAND_LO && d < BAND_HI) band_dec(d);` |
| nes.c:24 | `_pad_read` | 421 | 2.9% | `` |
