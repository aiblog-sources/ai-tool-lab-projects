| loop (ends at frame) | busy cycles | NMIs | hottest lines in that loop |
| --- | --- | --- | --- |
| 130 (190) | 13,922 | 1 | fix.c:154 `for (j = lo; j <= hi; ++j) ++band_cnt[j];` 1,612<br>fix.c:160 `for (j = lo; j <= hi; ++j) --band_cnt[j];` 1,414<br>fix.c:177 `p[3] = dx[i];` 814 |
| 214 (274) | 13,097 | 1 | fix.c:154 `for (j = lo; j <= hi; ++j) ++band_cnt[j];` 1,380<br>fix.c:160 `for (j = lo; j <= hi; ++j) --band_cnt[j];` 1,280<br>fix.c:182 `p[3] = dx[i];` 814 |
| 364 (424) | 13,096 | 1 | fix.c:154 `for (j = lo; j <= hi; ++j) ++band_cnt[j];` 1,493<br>fix.c:160 `for (j = lo; j <= hi; ++j) --band_cnt[j];` 1,183<br>fix.c:177 `p[3] = dx[i];` 1,036 |
| 271 (331) | 13,019 | 1 | fix.c:182 `p[3] = dx[i];` 1,443<br>fix.c:154 `for (j = lo; j <= hi; ++j) ++band_cnt[j];` 1,359<br>fix.c:160 `for (j = lo; j <= hi; ++j) --band_cnt[j];` 1,203 |
| 129 (189) | 12,953 | 1 | fix.c:160 `for (j = lo; j <= hi; ++j) --band_cnt[j];` 1,394<br>fix.c:154 `for (j = lo; j <= hi; ++j) ++band_cnt[j];` 962<br>fix.c:177 `p[3] = dx[i];` 851 |
