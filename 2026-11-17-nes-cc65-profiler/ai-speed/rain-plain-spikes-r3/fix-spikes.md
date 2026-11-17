| loop (ends at frame) | busy cycles | NMIs | hottest lines in that loop |
| --- | --- | --- | --- |
| 128 (188) | 23,638 | 1 | fix.c:69 `++band_cnt[s_l];` 1,904<br>fix.c:60 `--band_cnt[s_l];` 1,870<br>fix.c:61 `} while (++s_l <= s_hi);` 1,006 |
| 129 (189) | 23,475 | 1 | fix.c:60 `--band_cnt[s_l];` 1,904<br>fix.c:69 `++band_cnt[s_l];` 1,632<br>fix.c:61 `} while (++s_l <= s_hi);` 1,026 |
| 962 (1022) | 23,459 | 1 | fix.c:60 `--band_cnt[s_l];` 1,734<br>fix.c:69 `++band_cnt[s_l];` 1,632<br>fix.c:179 `p[3] = dx[i];` 1,110 |
| 961 (1021) | 23,238 | 1 | fix.c:69 `++band_cnt[s_l];` 1,734<br>fix.c:60 `--band_cnt[s_l];` 1,394<br>fix.c:179 `p[3] = dx[i];` 1,147 |
| 808 (868) | 23,104 | 1 | fix.c:60 `--band_cnt[s_l];` 1,632<br>fix.c:69 `++band_cnt[s_l];` 1,564<br>fix.c:159 `(unsigned int)dx[i] < (unsigned int)ux + UMB_W) {` 1,104 |
