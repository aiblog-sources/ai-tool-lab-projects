| loop (ends at frame) | busy cycles | NMIs | hottest lines in that loop |
| --- | --- | --- | --- |
| 128 (188) | 29,673 | 1 | fix.c:55 `for (; l <= hi; ++l) band_cnt[l - UMB_Y] += d;` 11,850<br>fix.c:67 `if (band_cnt[l - UMB_Y] >= BAND_MAX) return 0;` 2,260<br>fix.c:66 `for (; l <= hi; ++l) {` 2,101 |
| 129 (189) | 29,410 | 1 | fix.c:55 `for (; l <= hi; ++l) band_cnt[l - UMB_Y] += d;` 11,158<br>fix.c:67 `if (band_cnt[l - UMB_Y] >= BAND_MAX) return 0;` 2,032<br>fix.c:66 `for (; l <= hi; ++l) {` 1,918 |
| 130 (190) | 28,199 | 1 | fix.c:55 `for (; l <= hi; ++l) band_cnt[l - UMB_Y] += d;` 10,534<br>fix.c:67 `if (band_cnt[l - UMB_Y] >= BAND_MAX) return 0;` 1,900<br>fix.c:66 `for (; l <= hi; ++l) {` 1,880 |
| 127 (187) | 27,999 | 1 | fix.c:55 `for (; l <= hi; ++l) band_cnt[l - UMB_Y] += d;` 11,398<br>fix.c:67 `if (band_cnt[l - UMB_Y] >= BAND_MAX) return 0;` 2,146<br>fix.c:66 `for (; l <= hi; ++l) {` 1,981 |
| 213 (273) | 27,946 | 1 | fix.c:55 `for (; l <= hi; ++l) band_cnt[l - UMB_Y] += d;` 10,706<br>fix.c:67 `if (band_cnt[l - UMB_Y] >= BAND_MAX) return 0;` 2,020<br>fix.c:66 `for (; l <= hi; ++l) {` 1,948 |
