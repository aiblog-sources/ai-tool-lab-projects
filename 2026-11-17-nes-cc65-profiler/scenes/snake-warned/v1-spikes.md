| loop (ends at frame) | busy cycles | NMIs | hottest lines in that loop |
| --- | --- | --- | --- |
| 1109 (1169) | 17,128 | 1 | v1.c:100 `seg = 1 + (unsigned char)((rot + 3 * k) % (SEGS - 1));` 5,243<br>v1.c:31 `idx = (h - seg * DELAY) & HMASK;` 3,320<br>v1.c:35 `OAM_X(slot)    = hx[idx];` 1,420 |
| 1128 (1188) | 17,128 | 1 | v1.c:100 `seg = 1 + (unsigned char)((rot + 3 * k) % (SEGS - 1));` 5,243<br>v1.c:31 `idx = (h - seg * DELAY) & HMASK;` 3,320<br>v1.c:35 `OAM_X(slot)    = hx[idx];` 1,420 |
| 748 (808) | 17,128 | 1 | v1.c:100 `seg = 1 + (unsigned char)((rot + 3 * k) % (SEGS - 1));` 5,243<br>v1.c:31 `idx = (h - seg * DELAY) & HMASK;` 3,320<br>v1.c:35 `OAM_X(slot)    = hx[idx];` 1,420 |
| 83 (143) | 17,127 | 1 | v1.c:100 `seg = 1 + (unsigned char)((rot + 3 * k) % (SEGS - 1));` 5,243<br>v1.c:31 `idx = (h - seg * DELAY) & HMASK;` 3,320<br>v1.c:35 `OAM_X(slot)    = hx[idx];` 1,420 |
| 1147 (1207) | 17,126 | 1 | v1.c:100 `seg = 1 + (unsigned char)((rot + 3 * k) % (SEGS - 1));` 5,243<br>v1.c:31 `idx = (h - seg * DELAY) & HMASK;` 3,320<br>v1.c:35 `OAM_X(slot)    = hx[idx];` 1,420 |
