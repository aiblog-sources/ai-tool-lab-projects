| loop (ends at frame) | busy cycles | NMIs | hottest lines in that loop |
| --- | --- | --- | --- |
| 131 (206) | 36,512 | 2 | fix.c:61 `for (; l <= hi; ++l) --band_cnt[l];` 4,067<br>fix.c:52 `for (; l <= hi; ++l) ++band_cnt[l];` 3,819<br>fix.c:71 `for (; l <= hi; ++l) {` 2,040 |
| 130 (204) | 36,343 | 2 | fix.c:61 `for (; l <= hi; ++l) --band_cnt[l];` 4,120<br>fix.c:52 `for (; l <= hi; ++l) ++band_cnt[l];` 4,067<br>fix.c:71 `for (; l <= hi; ++l) {` 2,155 |
| 132 (208) | 36,059 | 2 | fix.c:52 `for (; l <= hi; ++l) ++band_cnt[l];` 3,996<br>fix.c:61 `for (; l <= hi; ++l) --band_cnt[l];` 3,819<br>fix.c:71 `for (; l <= hi; ++l) {` 2,103 |
| 215 (300) | 35,418 | 2 | fix.c:61 `for (; l <= hi; ++l) --band_cnt[l];` 3,978<br>fix.c:52 `for (; l <= hi; ++l) ++band_cnt[l];` 3,925<br>fix.c:71 `for (; l <= hi; ++l) {` 1,948 |
| 129 (202) | 35,415 | 2 | fix.c:52 `for (; l <= hi; ++l) ++band_cnt[l];` 4,120<br>fix.c:61 `for (; l <= hi; ++l) --band_cnt[l];` 3,818<br>fix.c:71 `for (; l <= hi; ++l) {` 2,102 |
