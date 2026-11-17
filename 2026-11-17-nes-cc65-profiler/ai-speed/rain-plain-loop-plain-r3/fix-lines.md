| line | function | cycles/frame | busy | source |
| --- | --- | --- | --- | --- |
| fix.c:55 | `_band_add` | 4,220 | 29.9% | `for (; l <= hi; ++l) band_cnt[l - UMB_Y] += d;` |
| fix.c:196 | `_main` | 1,526 | 10.8% | `"bne @cp\n");` |
| fix.c:146 | `_main` | 1,127 | 8.0% | `"stx _si\n");` |
| fix.c:67 | `_band_fits` | 752 | 5.3% | `if (band_cnt[l - UMB_Y] >= BAND_MAX) return 0;` |
| fix.c:66 | `_band_fits` | 739 | 5.2% | `for (; l <= hi; ++l) {` |
| (asm) | `nmi` | 621 | 4.4% | `` |
| nes.c:24 | `_pad_read` | 421 | 3.0% | `` |
| fix.c:51 | `_band_add` | 403 | 2.9% | `if (y + 8 <= UMB_Y \|\| y >= UMB_Y + UMB_H) return;` |
| fix.c:153 | `_main` | 347 | 2.5% | `if (IN_BAND(o)) band_add(o, -1);` |
| fix.c:169 | `_main` | 303 | 2.1% | `if (IN_BAND(o)) band_add(o, 1);` |
| fix.c:160 | `_main` | 285 | 2.0% | `(unsigned int)dx[i] < (unsigned int)m + UMB_W) {` |
| fix.c:54 | `_band_add` | 283 | 2.0% | `if (hi > UMB_Y + UMB_H - 1) hi = UMB_Y + UMB_H - 1;` |
| fix.c:62 | `_band_fits` | 206 | 1.5% | `if (y + 8 <= UMB_Y \|\| y >= UMB_Y + UMB_H) return 1;` |
| fix.c:49 | `_band_add` | 201 | 1.4% | `{` |
| fix.c:152 | `_main` | 191 | 1.4% | `y = o + dsp[i];` |
