| loop (ends at frame) | busy cycles | NMIs | hottest lines in that loop |
| --- | --- | --- | --- |
| 336 (747) | 62,677 | 3 | v1.c:50 `for (; l <= hi; ++l) band_cnt[l - UMB_Y] += d;` 10,480<br>v1.c:46 `if (y + 8 <= UMB_Y \|\| y >= UMB_Y + UMB_H) return;` 4,406<br>v1.c:127 `band_add(dy[i], -1);` 3,280 |
| 95 (256) | 62,622 | 3 | v1.c:50 `for (; l <= hi; ++l) band_cnt[l - UMB_Y] += d;` 10,688<br>v1.c:46 `if (y + 8 <= UMB_Y \|\| y >= UMB_Y + UMB_H) return;` 4,406<br>v1.c:127 `band_add(dy[i], -1);` 3,280 |
| 337 (750) | 62,451 | 3 | v1.c:50 `for (; l <= hi; ++l) band_cnt[l - UMB_Y] += d;` 10,584<br>v1.c:46 `if (y + 8 <= UMB_Y \|\| y >= UMB_Y + UMB_H) return;` 4,415<br>v1.c:127 `band_add(dy[i], -1);` 3,280 |
| 338 (753) | 62,352 | 3 | v1.c:50 `for (; l <= hi; ++l) band_cnt[l - UMB_Y] += d;` 10,688<br>v1.c:46 `if (y + 8 <= UMB_Y \|\| y >= UMB_Y + UMB_H) return;` 4,406<br>v1.c:127 `band_add(dy[i], -1);` 3,280 |
| 93 (250) | 62,293 | 3 | v1.c:50 `for (; l <= hi; ++l) band_cnt[l - UMB_Y] += d;` 10,376<br>v1.c:46 `if (y + 8 <= UMB_Y \|\| y >= UMB_Y + UMB_H) return;` 4,370<br>v1.c:127 `band_add(dy[i], -1);` 3,280 |
