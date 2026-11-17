| line | function | cycles/frame | busy | source |
| --- | --- | --- | --- | --- |
| fix.c:57 | `_band_add` | 3,470 | 20.6% | `for (; s_l <= s_hi; ++s_l) band_cnt[s_l - UMB_Y] += d;` |
| fix.c:165 | `_main` | 902 | 5.4% | `((unsigned char *)0x0220)[s_j + 3] = dx[s_i];` |
| fix.c:170 | `_main` | 858 | 5.1% | `((unsigned char *)0x0220)[s_j + 3] = dx[s_i];` |
| fix.c:126 | `_main` | 736 | 4.4% | `for (s_i = 0; s_i < NUM_DROPS; ++s_i) {` |
| fix.c:68 | `_band_fits` | 733 | 4.4% | `if (band_cnt[s_l - UMB_Y] >= BAND_MAX) return 0;` |
| fix.c:128 | `_main` | 720 | 4.3% | `s_y = s_old + dsp[s_i];` |
| fix.c:164 | `_main` | 718 | 4.3% | `((unsigned char *)0x0220)[s_j]     = dy[s_i];` |
| fix.c:169 | `_main` | 682 | 4.1% | `((unsigned char *)0x0220)[s_j]     = dy[s_i];` |
| (asm) | `nmi` | 621 | 3.7% | `` |
| fix.c:129 | `_main` | 600 | 3.6% | `if (s_old < 193 && s_y < 193) {` |
| fix.c:127 | `_main` | 480 | 2.9% | `s_old = dy[s_i];` |
| fix.c:53 | `_band_add` | 461 | 2.7% | `if (y + 8 <= UMB_Y \|\| y >= UMB_Y + UMB_H) return;` |
| fix.c:67 | `_band_fits` | 455 | 2.7% | `for (; s_l <= s_hi; ++s_l) {` |
| fix.c:131 | `_main` | 437 | 2.6% | `dy[s_i] = s_y;` |
| nes.c:24 | `_pad_read` | 421 | 2.5% | `` |
