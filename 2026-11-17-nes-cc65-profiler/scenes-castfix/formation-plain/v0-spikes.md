| loop (ends at frame) | busy cycles | NMIs | hottest lines in that loop |
| --- | --- | --- | --- |
| 61 (121) | 21,171 | 1 | v0.c:56 `unsigned char *o = OAM + ((unsigned int)idx << 2);` 3,404<br>v0.c:132 `unsigned char x = form_x + i * SPACING;` 1,640<br>v0.c:57 `o[0] = y - 1;                   /* OAM Y is top line - 1 */` 1,443 |
| 443 (503) | 21,043 | 1 | v0.c:56 `unsigned char *o = OAM + ((unsigned int)idx << 2);` 3,404<br>v0.c:132 `unsigned char x = form_x + i * SPACING;` 1,640<br>v0.c:57 `o[0] = y - 1;                   /* OAM Y is top line - 1 */` 1,443 |
| 91 (151) | 21,038 | 1 | v0.c:56 `unsigned char *o = OAM + ((unsigned int)idx << 2);` 3,404<br>v0.c:132 `unsigned char x = form_x + i * SPACING;` 1,640<br>v0.c:57 `o[0] = y - 1;                   /* OAM Y is top line - 1 */` 1,443 |
| 555 (615) | 21,037 | 1 | v0.c:56 `unsigned char *o = OAM + ((unsigned int)idx << 2);` 3,404<br>v0.c:132 `unsigned char x = form_x + i * SPACING;` 1,640<br>v0.c:57 `o[0] = y - 1;                   /* OAM Y is top line - 1 */` 1,443 |
| 64 (124) | 21,033 | 1 | v0.c:56 `unsigned char *o = OAM + ((unsigned int)idx << 2);` 3,404<br>v0.c:132 `unsigned char x = form_x + i * SPACING;` 1,640<br>v0.c:57 `o[0] = y - 1;                   /* OAM Y is top line - 1 */` 1,443 |
