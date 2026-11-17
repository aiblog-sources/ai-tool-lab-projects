| loop (ends at frame) | busy cycles | NMIs | hottest lines in that loop |
| --- | --- | --- | --- |
| 961 (1021) | 16,636 | 1 | fix.c:175 `for (; l <= hi; ++l) ++band_cnt[l - UMB_Y];` 1,703<br>fix.c:190 `o[3] = dx[i];` 1,480<br>fix.c:189 `o[0] = dy[i];` 1,200 |
| 130 (190) | 16,257 | 1 | fix.c:175 `for (; l <= hi; ++l) ++band_cnt[l - UMB_Y];` 1,732<br>fix.c:180 `for (; l <= hi; ++l) --band_cnt[l - UMB_Y];` 1,578<br>fix.c:190 `o[3] = dx[i];` 1,480 |
| 809 (869) | 16,022 | 1 | fix.c:190 `o[3] = dx[i];` 1,480<br>fix.c:175 `for (; l <= hi; ++l) ++band_cnt[l - UMB_Y];` 1,394<br>fix.c:180 `for (; l <= hi; ++l) --band_cnt[l - UMB_Y];` 1,241 |
| 364 (424) | 15,915 | 1 | fix.c:175 `for (; l <= hi; ++l) ++band_cnt[l - UMB_Y];` 1,668<br>fix.c:190 `o[3] = dx[i];` 1,480<br>fix.c:180 `for (; l <= hi; ++l) --band_cnt[l - UMB_Y];` 1,386 |
| 962 (1022) | 15,900 | 1 | fix.c:190 `o[3] = dx[i];` 1,480<br>fix.c:180 `for (; l <= hi; ++l) --band_cnt[l - UMB_Y];` 1,281<br>fix.c:189 `o[0] = dy[i];` 1,200 |
