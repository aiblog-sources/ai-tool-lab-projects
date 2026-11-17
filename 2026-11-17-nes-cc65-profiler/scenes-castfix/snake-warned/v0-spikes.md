| loop (ends at frame) | busy cycles | NMIs | hottest lines in that loop |
| --- | --- | --- | --- |
| 747 (807) | 8,085 | 1 | v0.c:81 `idx = (h - seg * DELAY) & HMASK;` 3,320<br>v0.c:85 `OAM_X(k)    = hx[idx];` 680<br>v0.c:84 `OAM_ATTR(k) = (seg == 0) ? 1 : 0;` 662 |
| 87 (147) | 8,085 | 1 | v0.c:81 `idx = (h - seg * DELAY) & HMASK;` 3,320<br>v0.c:85 `OAM_X(k)    = hx[idx];` 680<br>v0.c:84 `OAM_ATTR(k) = (seg == 0) ? 1 : 0;` 662 |
| 1107 (1167) | 8,085 | 1 | v0.c:81 `idx = (h - seg * DELAY) & HMASK;` 3,320<br>v0.c:85 `OAM_X(k)    = hx[idx];` 680<br>v0.c:84 `OAM_ATTR(k) = (seg == 0) ? 1 : 0;` 662 |
| 1127 (1187) | 8,085 | 1 | v0.c:81 `idx = (h - seg * DELAY) & HMASK;` 3,320<br>v0.c:85 `OAM_X(k)    = hx[idx];` 680<br>v0.c:84 `OAM_ATTR(k) = (seg == 0) ? 1 : 0;` 662 |
| 67 (127) | 8,083 | 1 | v0.c:81 `idx = (h - seg * DELAY) & HMASK;` 3,320<br>v0.c:85 `OAM_X(k)    = hx[idx];` 680<br>v0.c:84 `OAM_ATTR(k) = (seg == 0) ? 1 : 0;` 662 |
