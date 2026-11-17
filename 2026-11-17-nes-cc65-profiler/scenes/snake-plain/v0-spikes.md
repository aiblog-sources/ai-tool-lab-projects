| loop (ends at frame) | busy cycles | NMIs | hottest lines in that loop |
| --- | --- | --- | --- |
| 748 (808) | 10,669 | 1 | v0.c:93 `idx = (h - (unsigned char)(seg * DELAY)) & HMASK;` 2,793<br>v0.c:97 `OAM_X(j)    = hist_x[idx];` 1,501<br>v0.c:94 `OAM_Y(j)    = hist_y[idx];` 1,377 |
| 729 (789) | 10,668 | 1 | v0.c:93 `idx = (h - (unsigned char)(seg * DELAY)) & HMASK;` 2,793<br>v0.c:97 `OAM_X(j)    = hist_x[idx];` 1,501<br>v0.c:94 `OAM_Y(j)    = hist_y[idx];` 1,378 |
| 767 (827) | 10,667 | 1 | v0.c:93 `idx = (h - (unsigned char)(seg * DELAY)) & HMASK;` 2,793<br>v0.c:97 `OAM_X(j)    = hist_x[idx];` 1,501<br>v0.c:94 `OAM_Y(j)    = hist_y[idx];` 1,375 |
| 641 (701) | 10,666 | 1 | v0.c:93 `idx = (h - (unsigned char)(seg * DELAY)) & HMASK;` 2,793<br>v0.c:97 `OAM_X(j)    = hist_x[idx];` 1,501<br>v0.c:94 `OAM_Y(j)    = hist_y[idx];` 1,375 |
| 710 (770) | 10,666 | 1 | v0.c:93 `idx = (h - (unsigned char)(seg * DELAY)) & HMASK;` 2,793<br>v0.c:97 `OAM_X(j)    = hist_x[idx];` 1,501<br>v0.c:94 `OAM_Y(j)    = hist_y[idx];` 1,375 |
