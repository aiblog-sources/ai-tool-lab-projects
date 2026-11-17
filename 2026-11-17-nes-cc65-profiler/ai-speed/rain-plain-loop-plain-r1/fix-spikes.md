| loop (ends at frame) | busy cycles | NMIs | hottest lines in that loop |
| --- | --- | --- | --- |
| 128 (188) | 19,781 | 1 | fix.c:62 `++band_cnt[bl];` 1,904<br>fix.c:70 `--band_cnt[bl];` 1,870<br>fix.c:63 `} while (++bl <= bh);` 1,026 |
| 129 (189) | 19,707 | 1 | fix.c:70 `--band_cnt[bl];` 1,904<br>fix.c:62 `++band_cnt[bl];` 1,632<br>fix.c:71 `} while (++bl <= bh);` 979 |
| 962 (1022) | 19,584 | 1 | fix.c:70 `--band_cnt[bl];` 1,734<br>fix.c:62 `++band_cnt[bl];` 1,632<br>fix.c:166 `if (xl + 8 > ux && xl < (unsigned int)ux + UMB_W) {` 1,319 |
| 961 (1021) | 19,409 | 1 | fix.c:62 `++band_cnt[bl];` 1,734<br>fix.c:70 `--band_cnt[bl];` 1,394<br>fix.c:166 `if (xl + 8 > ux && xl < (unsigned int)ux + UMB_W) {` 1,320 |
| 808 (868) | 19,154 | 1 | fix.c:70 `--band_cnt[bl];` 1,632<br>fix.c:62 `++band_cnt[bl];` 1,564<br>fix.c:166 `if (xl + 8 > ux && xl < (unsigned int)ux + UMB_W) {` 1,320 |
