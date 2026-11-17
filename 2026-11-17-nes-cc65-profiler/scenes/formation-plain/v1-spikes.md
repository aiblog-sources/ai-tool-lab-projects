| loop (ends at frame) | busy cycles | NMIs | hottest lines in that loop |
| --- | --- | --- | --- |
| 991 (1051) | 18,222 | 1 | v1.c:58 `unsigned char *o = OAM + ((unsigned int)idx << 2);` 2,668<br>v1.c:141 `OAM_Y(idx) = 0xFF;` 1,960<br>v1.c:134 `unsigned char x = form_x + i * SPACING;` 1,230 |
| 62 (122) | 18,102 | 1 | v1.c:58 `unsigned char *o = OAM + ((unsigned int)idx << 2);` 2,668<br>v1.c:141 `OAM_Y(idx) = 0xFF;` 1,960<br>v1.c:134 `unsigned char x = form_x + i * SPACING;` 1,230 |
| 63 (123) | 18,102 | 1 | v1.c:58 `unsigned char *o = OAM + ((unsigned int)idx << 2);` 2,668<br>v1.c:141 `OAM_Y(idx) = 0xFF;` 1,960<br>v1.c:134 `unsigned char x = form_x + i * SPACING;` 1,230 |
| 442 (502) | 18,098 | 1 | v1.c:58 `unsigned char *o = OAM + ((unsigned int)idx << 2);` 2,668<br>v1.c:141 `OAM_Y(idx) = 0xFF;` 1,960<br>v1.c:134 `unsigned char x = form_x + i * SPACING;` 1,230 |
| 443 (503) | 18,098 | 1 | v1.c:58 `unsigned char *o = OAM + ((unsigned int)idx << 2);` 2,668<br>v1.c:141 `OAM_Y(idx) = 0xFF;` 1,960<br>v1.c:134 `unsigned char x = form_x + i * SPACING;` 1,230 |
