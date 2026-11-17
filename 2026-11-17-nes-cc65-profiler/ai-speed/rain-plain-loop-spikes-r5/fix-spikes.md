| loop (ends at frame) | busy cycles | NMIs | hottest lines in that loop |
| --- | --- | --- | --- |
| 130 (190) | 17,760 | 1 | fix.c:61 `for (j = lo; j <= hi; ++j) ++band_cnt[j - UMB_Y];` 1,840<br>fix.c:181 `if (hi > y - 1) hi = y - 1;` 1,764<br>fix.c:66 `for (j = lo; j <= hi; ++j) --band_cnt[j - UMB_Y];` 1,698 |
| 961 (1021) | 17,060 | 1 | fix.c:61 `for (j = lo; j <= hi; ++j) ++band_cnt[j - UMB_Y];` 1,745<br>fix.c:181 `if (hi > y - 1) hi = y - 1;` 1,176<br>fix.c:167 `(unsigned int)dx[i] < (unsigned int)ux + UMB_W) {` 1,104 |
| 646 (706) | 16,872 | 1 | fix.c:61 `for (j = lo; j <= hi; ++j) ++band_cnt[j - UMB_Y];` 1,380<br>fix.c:181 `if (hi > y - 1) hi = y - 1;` 1,323<br>fix.c:66 `for (j = lo; j <= hi; ++j) --band_cnt[j - UMB_Y];` 1,242 |
| 647 (707) | 16,789 | 1 | fix.c:61 `for (j = lo; j <= hi; ++j) ++band_cnt[j - UMB_Y];` 1,446<br>fix.c:181 `if (hi > y - 1) hi = y - 1;` 1,323<br>fix.c:66 `for (j = lo; j <= hi; ++j) --band_cnt[j - UMB_Y];` 1,241 |
| 900 (960) | 16,695 | 1 | fix.c:61 `for (j = lo; j <= hi; ++j) ++band_cnt[j - UMB_Y];` 1,615<br>fix.c:181 `if (hi > y - 1) hi = y - 1;` 1,176<br>fix.c:66 `for (j = lo; j <= hi; ++j) --band_cnt[j - UMB_Y];` 1,089 |
