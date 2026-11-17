| loop (ends at frame) | busy cycles | NMIs | hottest lines in that loop |
| --- | --- | --- | --- |
| 441 (501) | 22,073 | 1 | v0.c:56 `unsigned char *o = OAM + ((unsigned int)idx << 2);` 3,404<br>v0.c:132 `unsigned char x = form_x + i * SPACING;` 1,640<br>v0.c:139 `OAM_Y(idx) = 0xFF;` 1,512 |
| 541 (601) | 22,060 | 1 | v0.c:56 `unsigned char *o = OAM + ((unsigned int)idx << 2);` 3,404<br>v0.c:132 `unsigned char x = form_x + i * SPACING;` 1,640<br>v0.c:139 `OAM_Y(idx) = 0xFF;` 1,512 |
| 444 (504) | 21,942 | 1 | v0.c:56 `unsigned char *o = OAM + ((unsigned int)idx << 2);` 3,404<br>v0.c:132 `unsigned char x = form_x + i * SPACING;` 1,640<br>v0.c:139 `OAM_Y(idx) = 0xFF;` 1,512 |
| 92 (152) | 21,937 | 1 | v0.c:56 `unsigned char *o = OAM + ((unsigned int)idx << 2);` 3,404<br>v0.c:132 `unsigned char x = form_x + i * SPACING;` 1,640<br>v0.c:139 `OAM_Y(idx) = 0xFF;` 1,512 |
| 556 (616) | 21,936 | 1 | v0.c:56 `unsigned char *o = OAM + ((unsigned int)idx << 2);` 3,404<br>v0.c:132 `unsigned char x = form_x + i * SPACING;` 1,640<br>v0.c:139 `OAM_Y(idx) = 0xFF;` 1,512 |
