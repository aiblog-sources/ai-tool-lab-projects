| line | function | cycles/frame | busy | source |
| --- | --- | --- | --- | --- |
| fix.c:131 | `_main` | 1,200 | 8.7% | `y = oy + dsp[i];` |
| fix.c:129 | `_main` | 1,016 | 7.4% | `for (i = 0; i < NUM_DROPS; ++i) {` |
| fix.c:176 | `_main` | 882 | 6.4% | `DROP_Y(s) = dy[i];` |
| fix.c:177 | `_main` | 882 | 6.4% | `DROP_X(s) = dx[i];` |
| fix.c:181 | `_main` | 838 | 6.1% | `DROP_Y(s) = dy[i];` |
| fix.c:182 | `_main` | 838 | 6.1% | `DROP_X(s) = dx[i];` |
| fix.c:160 | `_main` | 828 | 6.0% | `dy[i] = y;` |
| fix.c:130 | `_main` | 760 | 5.5% | `oy = dy[i];` |
| (asm) | `nmi` | 621 | 4.5% | `` |
| fix.c:156 | `_main` | 611 | 4.4% | `for (l = lo; l <= hi; ++l) ++bl[l];` |
| fix.c:180 | `_main` | 522 | 3.8% | `for (i = 0; i < rot; ++i) {` |
| fix.c:159 | `_main` | 521 | 3.8% | `for (l = lo; l <= hi; ++l) --bl[l];` |
| fix.c:175 | `_main` | 509 | 3.7% | `for (i = rot; i < NUM_DROPS; ++i) {` |
| nes.c:24 | `_pad_read` | 421 | 3.1% | `` |
| fix.c:178 | `_main` | 348 | 2.5% | `s += 4;` |
