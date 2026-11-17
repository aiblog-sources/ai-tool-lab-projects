| loop (ends at frame) | busy cycles | NMIs | hottest lines in that loop |
| --- | --- | --- | --- |
| 95 (251) | 55,400 | 2 | v1.c:50 `for (; l <= hi; ++l) band_cnt[l - UMB_Y] += d;` 10,688<br>v1.c:46 `if (y + 8 <= UMB_Y \|\| y >= UMB_Y + UMB_H) return;` 4,406<br>v1.c:127 `band_add(dy[i], -1);` 3,280 |
| 93 (247) | 55,071 | 2 | v1.c:50 `for (; l <= hi; ++l) band_cnt[l - UMB_Y] += d;` 10,376<br>v1.c:46 `if (y + 8 <= UMB_Y \|\| y >= UMB_Y + UMB_H) return;` 4,370<br>v1.c:127 `band_add(dy[i], -1);` 3,280 |
| 94 (249) | 54,785 | 2 | v1.c:50 `for (; l <= hi; ++l) band_cnt[l - UMB_Y] += d;` 10,584<br>v1.c:46 `if (y + 8 <= UMB_Y \|\| y >= UMB_Y + UMB_H) return;` 4,388<br>v1.c:127 `band_add(dy[i], -1);` 3,280 |
| 96 (253) | 54,297 | 2 | v1.c:50 `for (; l <= hi; ++l) band_cnt[l - UMB_Y] += d;` 10,792<br>v1.c:46 `if (y + 8 <= UMB_Y \|\| y >= UMB_Y + UMB_H) return;` 4,406<br>v1.c:127 `band_add(dy[i], -1);` 3,280 |
| 97 (255) | 54,012 | 2 | v1.c:50 `for (; l <= hi; ++l) band_cnt[l - UMB_Y] += d;` 10,290<br>v1.c:46 `if (y + 8 <= UMB_Y \|\| y >= UMB_Y + UMB_H) return;` 4,413<br>v1.c:127 `band_add(dy[i], -1);` 3,280 |
