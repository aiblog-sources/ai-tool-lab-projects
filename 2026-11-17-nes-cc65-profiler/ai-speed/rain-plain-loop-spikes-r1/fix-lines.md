| line | function | cycles/frame | busy | source |
| --- | --- | --- | --- | --- |
| fix.c:82 | `_band_inc` | 1,164 | 8.4% | `for (; s_l <= s_hi; ++s_l) ++band_cnt[s_l];` |
| fix.c:90 | `_band_dec` | 1,163 | 8.4% | `for (; s_l <= s_hi; ++s_l) --band_cnt[s_l];` |
| fix.c:199 | `_main` | 800 | 5.7% | `OAM[s_o + 3] = dx[s_i];` |
| fix.c:204 | `_main` | 760 | 5.5% | `OAM[s_o + 3] = dx[s_i];` |
| fix.c:158 | `_main` | 736 | 5.3% | `for (s_i = 0; s_i < NUM_DROPS; ++s_i) {` |
| fix.c:160 | `_main` | 720 | 5.2% | `s_y = s_oy + dsp[s_i];` |
| fix.c:198 | `_main` | 676 | 4.9% | `OAM[s_o]     = dy[s_i];` |
| fix.c:203 | `_main` | 644 | 4.6% | `OAM[s_o]     = dy[s_i];` |
| (asm) | `nmi` | 621 | 4.5% | `` |
| fix.c:180 | `_main` | 513 | 3.7% | `dy[s_i] = s_y;` |
| fix.c:159 | `_main` | 480 | 3.4% | `s_oy = dy[s_i];` |
| fix.c:182 | `_main` | 480 | 3.4% | `s_y = dy[s_i];` |
| fix.c:98 | `_band_ok` | 455 | 3.3% | `for (; s_l <= s_hi; ++s_l) {` |
| nes.c:24 | `_pad_read` | 421 | 3.0% | `` |
| fix.c:202 | `_main` | 388 | 2.8% | `for (s_i = 0; s_i < rot; ++s_i) {` |
