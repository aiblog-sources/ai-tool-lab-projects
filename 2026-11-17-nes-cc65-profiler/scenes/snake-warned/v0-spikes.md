| loop (ends at frame) | busy cycles | NMIs | hottest lines in that loop |
| --- | --- | --- | --- |
| 748 (808) | 10,945 | 1 | v0.c:81 `idx = (h - seg * DELAY) & HMASK;` 3,320<br>v0.c:85 `OAM_X(k)    = hx[idx];` 1,420<br>v0.c:84 `OAM_ATTR(k) = (seg == 0) ? 1 : 0;` 1,402 |
| 1108 (1168) | 10,945 | 1 | v0.c:81 `idx = (h - seg * DELAY) & HMASK;` 3,320<br>v0.c:85 `OAM_X(k)    = hx[idx];` 1,420<br>v0.c:84 `OAM_ATTR(k) = (seg == 0) ? 1 : 0;` 1,402 |
| 88 (148) | 10,945 | 1 | v0.c:81 `idx = (h - seg * DELAY) & HMASK;` 3,320<br>v0.c:85 `OAM_X(k)    = hx[idx];` 1,420<br>v0.c:84 `OAM_ATTR(k) = (seg == 0) ? 1 : 0;` 1,402 |
| 1128 (1188) | 10,945 | 1 | v0.c:81 `idx = (h - seg * DELAY) & HMASK;` 3,320<br>v0.c:85 `OAM_X(k)    = hx[idx];` 1,420<br>v0.c:84 `OAM_ATTR(k) = (seg == 0) ? 1 : 0;` 1,402 |
| 68 (128) | 10,943 | 1 | v0.c:81 `idx = (h - seg * DELAY) & HMASK;` 3,320<br>v0.c:85 `OAM_X(k)    = hx[idx];` 1,420<br>v0.c:84 `OAM_ATTR(k) = (seg == 0) ? 1 : 0;` 1,402 |
