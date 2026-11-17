| line | function | cycles/frame | busy | source |
| --- | --- | --- | --- | --- |
| fix.c:58 | `_band_add` | 1,791 | 8.7% | `*p += d;` |
| fix.c:59 | `_band_add` | 1,759 | 8.5% | `++p;` |
| fix.c:144 | `_main` | 1,291 | 6.2% | `y = dy[i] + dsp[i];` |
| fix.c:178 | `_main` | 915 | 4.4% | `p += 4;` |
| fix.c:183 | `_main` | 859 | 4.2% | `p += 4;` |
| fix.c:80 | `_band_fits` | 849 | 4.1% | `++p;` |
| fix.c:177 | `_main` | 832 | 4.0% | `p[3] = dx[i];` |
| fix.c:143 | `_main` | 819 | 4.0% | `for (i = 0; i < NUM_DROPS; ++i) {` |
| fix.c:182 | `_main` | 781 | 3.8% | `p[3] = dx[i];` |
| fix.c:60 | `_band_add` | 698 | 3.4% | `} while (--n);` |
| fix.c:176 | `_main` | 682 | 3.3% | `p[0] = dy[i];` |
| fix.c:181 | `_main` | 641 | 3.1% | `p[0] = dy[i];` |
| fix.c:47 | `_band_add` | 625 | 3.0% | `if (y + 8 <= UMB_Y \|\| y >= UMB_Y + UMB_H) return;` |
| fix.c:147 | `_main` | 568 | 2.8% | `dy[i] = y;` |
| (asm) | `nmi` | 519 | 2.5% | `` |
