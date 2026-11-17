| loop (ends at frame) | busy cycles | NMIs | hottest lines in that loop |
| --- | --- | --- | --- |
| 103 (163) | 14,574 | 1 | v0.c:67 `y = drop_y[i] + layer_speed[i & 3];   /* max 239 + 6 < 256 */` 2,120<br>v0.c:112 `OAM_Y(n)    = drop_y[i];` 1,600<br>v0.c:115 `OAM_X(n)    = drop_x[i];` 1,600 |
| 79 (139) | 14,574 | 1 | v0.c:67 `y = drop_y[i] + layer_speed[i & 3];   /* max 239 + 6 < 256 */` 2,120<br>v0.c:112 `OAM_Y(n)    = drop_y[i];` 1,600<br>v0.c:115 `OAM_X(n)    = drop_x[i];` 1,600 |
| 91 (151) | 14,574 | 1 | v0.c:67 `y = drop_y[i] + layer_speed[i & 3];   /* max 239 + 6 < 256 */` 2,120<br>v0.c:112 `OAM_Y(n)    = drop_y[i];` 1,600<br>v0.c:115 `OAM_X(n)    = drop_x[i];` 1,600 |
| 115 (175) | 14,574 | 1 | v0.c:67 `y = drop_y[i] + layer_speed[i & 3];   /* max 239 + 6 < 256 */` 2,120<br>v0.c:112 `OAM_Y(n)    = drop_y[i];` 1,600<br>v0.c:115 `OAM_X(n)    = drop_x[i];` 1,600 |
| 127 (187) | 14,574 | 1 | v0.c:67 `y = drop_y[i] + layer_speed[i & 3];   /* max 239 + 6 < 256 */` 2,120<br>v0.c:112 `OAM_Y(n)    = drop_y[i];` 1,600<br>v0.c:115 `OAM_X(n)    = drop_x[i];` 1,600 |
