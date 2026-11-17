| loop (ends at frame) | busy cycles | NMIs | hottest lines in that loop |
| --- | --- | --- | --- |
| 991 (1051) | 17,071 | 1 | v1.c:58 `unsigned char *o = OAM + ((unsigned int)idx << 2);` 2,668<br>v1.c:134 `unsigned char x = form_x + i * SPACING;` 1,227<br>v1.c:59 `o[0] = y - 1;                   /* OAM Y is top line - 1 */` 1,131 |
| 62 (122) | 16,951 | 1 | v1.c:58 `unsigned char *o = OAM + ((unsigned int)idx << 2);` 2,668<br>v1.c:134 `unsigned char x = form_x + i * SPACING;` 1,227<br>v1.c:59 `o[0] = y - 1;                   /* OAM Y is top line - 1 */` 1,131 |
| 63 (123) | 16,951 | 1 | v1.c:58 `unsigned char *o = OAM + ((unsigned int)idx << 2);` 2,668<br>v1.c:134 `unsigned char x = form_x + i * SPACING;` 1,227<br>v1.c:59 `o[0] = y - 1;                   /* OAM Y is top line - 1 */` 1,131 |
| 442 (502) | 16,947 | 1 | v1.c:58 `unsigned char *o = OAM + ((unsigned int)idx << 2);` 2,668<br>v1.c:134 `unsigned char x = form_x + i * SPACING;` 1,227<br>v1.c:59 `o[0] = y - 1;                   /* OAM Y is top line - 1 */` 1,131 |
| 443 (503) | 16,947 | 1 | v1.c:58 `unsigned char *o = OAM + ((unsigned int)idx << 2);` 2,668<br>v1.c:134 `unsigned char x = form_x + i * SPACING;` 1,227<br>v1.c:59 `o[0] = y - 1;                   /* OAM Y is top line - 1 */` 1,131 |
