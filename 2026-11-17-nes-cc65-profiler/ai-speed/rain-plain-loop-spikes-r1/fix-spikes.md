| loop (ends at frame) | busy cycles | NMIs | hottest lines in that loop |
| --- | --- | --- | --- |
| 128 (188) | 20,353 | 1 | fix.c:82 `for (; s_l <= s_hi; ++s_l) ++band_cnt[s_l];` 3,300<br>fix.c:90 `for (; s_l <= s_hi; ++s_l) --band_cnt[s_l];` 3,231<br>fix.c:98 `for (; s_l <= s_hi; ++s_l) {` 1,293 |
| 962 (1022) | 20,269 | 1 | fix.c:90 `for (; s_l <= s_hi; ++s_l) --band_cnt[s_l];` 3,003<br>fix.c:82 `for (; s_l <= s_hi; ++s_l) ++band_cnt[s_l];` 2,820<br>fix.c:199 `OAM[s_o + 3] = dx[s_i];` 1,170 |
| 129 (189) | 20,024 | 1 | fix.c:90 `for (; s_l <= s_hi; ++s_l) --band_cnt[s_l];` 3,300<br>fix.c:82 `for (; s_l <= s_hi; ++s_l) ++band_cnt[s_l];` 2,856<br>fix.c:98 `for (; s_l <= s_hi; ++s_l) {` 1,180 |
| 961 (1021) | 19,980 | 1 | fix.c:82 `for (; s_l <= s_hi; ++s_l) ++band_cnt[s_l];` 3,003<br>fix.c:90 `for (; s_l <= s_hi; ++s_l) --band_cnt[s_l];` 2,421<br>fix.c:199 `OAM[s_o + 3] = dx[s_i];` 1,209 |
| 808 (868) | 19,793 | 1 | fix.c:90 `for (; s_l <= s_hi; ++s_l) --band_cnt[s_l];` 2,820<br>fix.c:82 `for (; s_l <= s_hi; ++s_l) ++band_cnt[s_l];` 2,718<br>fix.c:171 `(unsigned int)dx[s_i] < (unsigned int)ux + UMB_W) {` 1,104 |
