| loop (ends at frame) | busy cycles | NMIs | hottest lines in that loop |
| --- | --- | --- | --- |
| 128 (188) | 20,277 | 1 | fix.c:58 `++band_cnt[bl];` 1,904<br>fix.c:68 `--band_cnt[bl];` 1,870<br>fix.c:59 `} while (++bl <= bh);` 1,026 |
| 129 (189) | 20,106 | 1 | fix.c:68 `--band_cnt[bl];` 1,904<br>fix.c:58 `++band_cnt[bl];` 1,632<br>fix.c:69 `} while (++bl <= bh);` 979 |
| 962 (1022) | 20,086 | 1 | fix.c:68 `--band_cnt[bl];` 1,734<br>fix.c:58 `++band_cnt[bl];` 1,632<br>fix.c:149 `(unsigned int)dx[i] < (unsigned int)ux + UMB_W) {` 1,103 |
| 961 (1021) | 19,876 | 1 | fix.c:58 `++band_cnt[bl];` 1,734<br>fix.c:68 `--band_cnt[bl];` 1,394<br>fix.c:149 `(unsigned int)dx[i] < (unsigned int)ux + UMB_W) {` 1,104 |
| 808 (868) | 19,730 | 1 | fix.c:68 `--band_cnt[bl];` 1,632<br>fix.c:58 `++band_cnt[bl];` 1,564<br>fix.c:149 `(unsigned int)dx[i] < (unsigned int)ux + UMB_W) {` 1,104 |
