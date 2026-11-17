| loop (ends at frame) | busy cycles | NMIs | hottest lines in that loop |
| --- | --- | --- | --- |
| 1109 (1169) | 14,268 | 1 | v1.c:100 `seg = 1 + (unsigned char)((rot + 3 * k) % (SEGS - 1));` 5,243<br>v1.c:31 `idx = (h - seg * DELAY) & HMASK;` 3,320<br>v1.c:35 `OAM_X(slot)    = hx[idx];` 680 |
| 1128 (1188) | 14,268 | 1 | v1.c:100 `seg = 1 + (unsigned char)((rot + 3 * k) % (SEGS - 1));` 5,243<br>v1.c:31 `idx = (h - seg * DELAY) & HMASK;` 3,320<br>v1.c:35 `OAM_X(slot)    = hx[idx];` 680 |
| 748 (808) | 14,268 | 1 | v1.c:100 `seg = 1 + (unsigned char)((rot + 3 * k) % (SEGS - 1));` 5,243<br>v1.c:31 `idx = (h - seg * DELAY) & HMASK;` 3,320<br>v1.c:35 `OAM_X(slot)    = hx[idx];` 680 |
| 83 (143) | 14,267 | 1 | v1.c:100 `seg = 1 + (unsigned char)((rot + 3 * k) % (SEGS - 1));` 5,243<br>v1.c:31 `idx = (h - seg * DELAY) & HMASK;` 3,320<br>v1.c:35 `OAM_X(slot)    = hx[idx];` 680 |
| 1147 (1207) | 14,266 | 1 | v1.c:100 `seg = 1 + (unsigned char)((rot + 3 * k) % (SEGS - 1));` 5,243<br>v1.c:31 `idx = (h - seg * DELAY) & HMASK;` 3,320<br>v1.c:35 `OAM_X(slot)    = hx[idx];` 680 |
