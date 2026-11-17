| loop (ends at frame) | busy cycles | NMIs | hottest lines in that loop |
| --- | --- | --- | --- |
| 132 (203) | 37,704 | 2 | fix.c:57 `for (; l <= hi; ++l) band_cnt[l] += d;` 9,900<br>fix.c:70 `for (; l <= hi; ++l) {` 2,040<br>fix.c:136 `if (IN_BAND(old)) band_add(old, -1);` 1,297 |
| 131 (201) | 37,344 | 2 | fix.c:57 `for (; l <= hi; ++l) band_cnt[l] += d;` 10,296<br>fix.c:70 `for (; l <= hi; ++l) {` 2,155<br>fix.c:161 `p += 4;` 1,320 |
| 133 (205) | 37,198 | 2 | fix.c:57 `for (; l <= hi; ++l) band_cnt[l] += d;` 9,810<br>fix.c:70 `for (; l <= hi; ++l) {` 2,103<br>fix.c:136 `if (IN_BAND(old)) band_add(old, -1);` 1,443 |
| 216 (297) | 36,271 | 2 | fix.c:57 `for (; l <= hi; ++l) band_cnt[l] += d;` 9,936<br>fix.c:70 `for (; l <= hi; ++l) {` 1,948<br>fix.c:136 `if (IN_BAND(old)) band_add(old, -1);` 1,224 |
| 130 (199) | 36,160 | 2 | fix.c:57 `for (; l <= hi; ++l) band_cnt[l] += d;` 9,990<br>fix.c:70 `for (; l <= hi; ++l) {` 2,102<br>fix.c:161 `p += 4;` 1,375 |
