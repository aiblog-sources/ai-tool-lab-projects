| loop (ends at frame) | busy cycles | NMIs | hottest lines in that loop |
| --- | --- | --- | --- |
| 748 (808) | 7,952 | 1 | v0.c:93 `idx = (h - (unsigned char)(seg * DELAY)) & HMASK;` 2,793<br>v0.c:97 `OAM_X(j)    = hist_x[idx];` 798<br>v0.c:94 `OAM_Y(j)    = hist_y[idx];` 769 |
| 729 (789) | 7,951 | 1 | v0.c:93 `idx = (h - (unsigned char)(seg * DELAY)) & HMASK;` 2,793<br>v0.c:97 `OAM_X(j)    = hist_x[idx];` 798<br>v0.c:94 `OAM_Y(j)    = hist_y[idx];` 770 |
| 767 (827) | 7,950 | 1 | v0.c:93 `idx = (h - (unsigned char)(seg * DELAY)) & HMASK;` 2,793<br>v0.c:97 `OAM_X(j)    = hist_x[idx];` 798<br>v0.c:94 `OAM_Y(j)    = hist_y[idx];` 767 |
| 641 (701) | 7,949 | 1 | v0.c:93 `idx = (h - (unsigned char)(seg * DELAY)) & HMASK;` 2,793<br>v0.c:97 `OAM_X(j)    = hist_x[idx];` 798<br>v0.c:94 `OAM_Y(j)    = hist_y[idx];` 767 |
| 710 (770) | 7,949 | 1 | v0.c:93 `idx = (h - (unsigned char)(seg * DELAY)) & HMASK;` 2,793<br>v0.c:97 `OAM_X(j)    = hist_x[idx];` 798<br>v0.c:94 `OAM_Y(j)    = hist_y[idx];` 767 |
