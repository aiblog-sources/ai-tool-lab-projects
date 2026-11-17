| loop (ends at frame) | busy cycles | NMIs | hottest lines in that loop |
| --- | --- | --- | --- |
| 764 (889) | 39,504 | 2 | fix.c:55 `for (; l <= hi; ++l) band_cnt[l - UMB_Y] += d;` 11,226<br>fix.c:67 `if (band_cnt[l - UMB_Y] >= BAND_MAX) return 0;` 2,026<br>fix.c:66 `for (; l <= hi; ++l) {` 1,934 |
| 133 (205) | 39,334 | 2 | fix.c:55 `for (; l <= hi; ++l) band_cnt[l - UMB_Y] += d;` 11,280<br>fix.c:67 `if (band_cnt[l - UMB_Y] >= BAND_MAX) return 0;` 2,216<br>fix.c:66 `for (; l <= hi; ++l) {` 2,103 |
| 131 (201) | 38,815 | 2 | fix.c:55 `for (; l <= hi; ++l) band_cnt[l - UMB_Y] += d;` 11,850<br>fix.c:67 `if (band_cnt[l - UMB_Y] >= BAND_MAX) return 0;` 2,342<br>fix.c:66 `for (; l <= hi; ++l) {` 2,155 |
| 132 (203) | 38,814 | 2 | fix.c:55 `for (; l <= hi; ++l) band_cnt[l - UMB_Y] += d;` 11,384<br>fix.c:67 `if (band_cnt[l - UMB_Y] >= BAND_MAX) return 0;` 2,146<br>fix.c:66 `for (; l <= hi; ++l) {` 2,040 |
| 1000 (1148) | 38,495 | 2 | fix.c:55 `for (; l <= hi; ++l) band_cnt[l - UMB_Y] += d;` 11,642<br>fix.c:67 `if (band_cnt[l - UMB_Y] >= BAND_MAX) return 0;` 2,064<br>fix.c:66 `for (; l <= hi; ++l) {` 1,947 |
