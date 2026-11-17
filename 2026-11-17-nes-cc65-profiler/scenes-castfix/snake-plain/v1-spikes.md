| loop (ends at frame) | busy cycles | NMIs | hottest lines in that loop |
| --- | --- | --- | --- |
| 757 (817) | 7,961 | 1 | v1.c:101 `idx = (h - (unsigned char)(seg * DELAY)) & HMASK;` 2,793<br>v1.c:105 `OAM_X(j)    = hist_x[idx];` 798<br>v1.c:102 `OAM_Y(j)    = hist_y[idx];` 770 |
| 754 (814) | 7,961 | 1 | v1.c:101 `idx = (h - (unsigned char)(seg * DELAY)) & HMASK;` 2,793<br>v1.c:105 `OAM_X(j)    = hist_x[idx];` 798<br>v1.c:102 `OAM_Y(j)    = hist_y[idx];` 770 |
| 746 (806) | 7,961 | 1 | v1.c:101 `idx = (h - (unsigned char)(seg * DELAY)) & HMASK;` 2,793<br>v1.c:105 `OAM_X(j)    = hist_x[idx];` 798<br>v1.c:102 `OAM_Y(j)    = hist_y[idx];` 770 |
| 751 (811) | 7,960 | 1 | v1.c:101 `idx = (h - (unsigned char)(seg * DELAY)) & HMASK;` 2,793<br>v1.c:105 `OAM_X(j)    = hist_x[idx];` 798<br>v1.c:102 `OAM_Y(j)    = hist_y[idx];` 769 |
| 743 (803) | 7,960 | 1 | v1.c:101 `idx = (h - (unsigned char)(seg * DELAY)) & HMASK;` 2,793<br>v1.c:105 `OAM_X(j)    = hist_x[idx];` 798<br>v1.c:102 `OAM_Y(j)    = hist_y[idx];` 769 |
