| loop (ends at frame) | busy cycles | NMIs | hottest lines in that loop |
| --- | --- | --- | --- |
| 757 (817) | 10,678 | 1 | v1.c:101 `idx = (h - (unsigned char)(seg * DELAY)) & HMASK;` 2,793<br>v1.c:105 `OAM_X(j)    = hist_x[idx];` 1,501<br>v1.c:102 `OAM_Y(j)    = hist_y[idx];` 1,378 |
| 754 (814) | 10,678 | 1 | v1.c:101 `idx = (h - (unsigned char)(seg * DELAY)) & HMASK;` 2,793<br>v1.c:105 `OAM_X(j)    = hist_x[idx];` 1,501<br>v1.c:102 `OAM_Y(j)    = hist_y[idx];` 1,378 |
| 746 (806) | 10,678 | 1 | v1.c:101 `idx = (h - (unsigned char)(seg * DELAY)) & HMASK;` 2,793<br>v1.c:105 `OAM_X(j)    = hist_x[idx];` 1,501<br>v1.c:102 `OAM_Y(j)    = hist_y[idx];` 1,378 |
| 751 (811) | 10,677 | 1 | v1.c:101 `idx = (h - (unsigned char)(seg * DELAY)) & HMASK;` 2,793<br>v1.c:105 `OAM_X(j)    = hist_x[idx];` 1,501<br>v1.c:102 `OAM_Y(j)    = hist_y[idx];` 1,377 |
| 743 (803) | 10,677 | 1 | v1.c:101 `idx = (h - (unsigned char)(seg * DELAY)) & HMASK;` 2,793<br>v1.c:105 `OAM_X(j)    = hist_x[idx];` 1,501<br>v1.c:102 `OAM_Y(j)    = hist_y[idx];` 1,377 |
