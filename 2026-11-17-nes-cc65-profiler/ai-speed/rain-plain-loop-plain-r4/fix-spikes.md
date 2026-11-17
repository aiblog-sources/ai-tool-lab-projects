| loop (ends at frame) | busy cycles | NMIs | hottest lines in that loop |
| --- | --- | --- | --- |
| 128 (188) | 27,789 | 1 | fix.c:65 `for (; l <= h; ++l) ++band_cnt[l];` 4,371<br>fix.c:72 `for (; l <= h; ++l) --band_cnt[l];` 4,269<br>fix.c:79 `for (; l <= h; ++l) {` 2,213 |
| 129 (189) | 27,580 | 1 | fix.c:72 `for (; l <= h; ++l) --band_cnt[l];` 4,362<br>fix.c:65 `for (; l <= h; ++l) ++band_cnt[l];` 3,790<br>fix.c:79 `for (; l <= h; ++l) {` 2,018 |
| 130 (190) | 26,698 | 1 | fix.c:65 `for (; l <= h; ++l) ++band_cnt[l];` 3,921<br>fix.c:72 `for (; l <= h; ++l) --band_cnt[l];` 3,780<br>fix.c:79 `for (; l <= h; ++l) {` 1,980 |
| 127 (187) | 26,690 | 1 | fix.c:65 `for (; l <= h; ++l) ++band_cnt[l];` 4,277<br>fix.c:72 `for (; l <= h; ++l) --band_cnt[l];` 4,026<br>fix.c:79 `for (; l <= h; ++l) {` 2,087 |
| 213 (273) | 26,687 | 1 | fix.c:65 `for (; l <= h; ++l) ++band_cnt[l];` 3,921<br>fix.c:72 `for (; l <= h; ++l) --band_cnt[l];` 3,894<br>fix.c:79 `for (; l <= h; ++l) {` 2,052 |
