| line | function | cycles/frame | busy | source |
| --- | --- | --- | --- | --- |
| fix.c:57 | `_band_add` | 3,413 | 15.4% | `for (; l <= hi; ++l) band_cnt[l] += d;` |
| fix.c:135 | `_main` | 1,124 | 5.1% | `y = old + dsp[i];` |
| fix.c:161 | `_main` | 1,058 | 4.8% | `p += 4;` |
| fix.c:166 | `_main` | 1,002 | 4.5% | `p += 4;` |
| fix.c:160 | `_main` | 962 | 4.3% | `p[3] = dx[i];` |
| fix.c:133 | `_main` | 951 | 4.3% | `for (i = 0; i < NUM_DROPS; ++i) {` |
| fix.c:165 | `_main` | 911 | 4.1% | `p[3] = dx[i];` |
| fix.c:136 | `_main` | 828 | 3.7% | `if (IN_BAND(old)) band_add(old, -1);` |
| fix.c:159 | `_main` | 789 | 3.5% | `p[0] = dy[i];` |
| fix.c:147 | `_main` | 787 | 3.5% | `t = dy[i];` |
| fix.c:144 | `_main` | 775 | 3.5% | `dy[i] = y;` |
| fix.c:164 | `_main` | 747 | 3.4% | `p[0] = dy[i];` |
| fix.c:134 | `_main` | 712 | 3.2% | `old = dy[i];` |
| fix.c:70 | `_band_fits` | 688 | 3.1% | `for (; l <= hi; ++l) {` |
| fix.c:143 | `_main` | 667 | 3.0% | `} else if (!IN_BAND(y) \|\| band_fits(y)) {` |
