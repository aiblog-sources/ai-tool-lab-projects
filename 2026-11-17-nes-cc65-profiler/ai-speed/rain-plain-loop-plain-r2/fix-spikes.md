| loop (ends at frame) | busy cycles | NMIs | hottest lines in that loop |
| --- | --- | --- | --- |
| 131 (191) | 25,860 | 1 | fix.c:130 `for (j = s; j <= bHi; ++j) ++band_cnt[j - UMB_Y];` 2,081<br>fix.c:127 `for (j = aLo; j <= e; ++j) --band_cnt[j - UMB_Y];` 1,900<br>fix.c:90 `unsigned char aLo = 0, aHi = 0, bLo = 0, bHi = 0, s, e, j;` 1,586 |
| 783 (843) | 24,905 | 1 | fix.c:130 `for (j = s; j <= bHi; ++j) ++band_cnt[j - UMB_Y];` 1,719<br>fix.c:209 `p += 4;` 1,650<br>fix.c:127 `for (j = aLo; j <= e; ++j) --band_cnt[j - UMB_Y];` 1,536 |
| 130 (190) | 24,807 | 1 | fix.c:127 `for (j = aLo; j <= e; ++j) --band_cnt[j - UMB_Y];` 1,820<br>fix.c:130 `for (j = s; j <= bHi; ++j) ++band_cnt[j - UMB_Y];` 1,564<br>fix.c:90 `unsigned char aLo = 0, aHi = 0, bLo = 0, bHi = 0, s, e, j;` 1,464 |
| 1008 (1068) | 24,721 | 1 | fix.c:130 `for (j = s; j <= bHi; ++j) ++band_cnt[j - UMB_Y];` 1,560<br>fix.c:127 `for (j = aLo; j <= e; ++j) --band_cnt[j - UMB_Y];` 1,458<br>fix.c:204 `p += 4;` 1,375 |
| 133 (193) | 23,902 | 1 | fix.c:130 `for (j = s; j <= bHi; ++j) ++band_cnt[j - UMB_Y];` 1,820<br>fix.c:127 `for (j = aLo; j <= e; ++j) --band_cnt[j - UMB_Y];` 1,769<br>fix.c:90 `unsigned char aLo = 0, aHi = 0, bLo = 0, bHi = 0, s, e, j;` 1,342 |
