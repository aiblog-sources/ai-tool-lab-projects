| loop (ends at frame) | busy cycles | NMIs | hottest lines in that loop |
| --- | --- | --- | --- |
| 783 (843) | 22,104 | 1 | v0.c:83 `c = i % 3;              /* palette 0..2; tile $01 = colour 1 */` 6,068<br>v0.c:34 `OAM_TILE(n) = tile;` 1,752<br>v0.c:36 `OAM_X(n)    = x;` 1,752 |
| 813 (873) | 22,104 | 1 | v0.c:83 `c = i % 3;              /* palette 0..2; tile $01 = colour 1 */` 6,068<br>v0.c:34 `OAM_TILE(n) = tile;` 1,752<br>v0.c:36 `OAM_X(n)    = x;` 1,752 |
| 1083 (1143) | 22,103 | 1 | v0.c:83 `c = i % 3;              /* palette 0..2; tile $01 = colour 1 */` 6,068<br>v0.c:34 `OAM_TILE(n) = tile;` 1,752<br>v0.c:36 `OAM_X(n)    = x;` 1,752 |
| 703 (763) | 22,098 | 1 | v0.c:83 `c = i % 3;              /* palette 0..2; tile $01 = colour 1 */` 6,068<br>v0.c:34 `OAM_TILE(n) = tile;` 1,752<br>v0.c:36 `OAM_X(n)    = x;` 1,752 |
| 403 (463) | 22,098 | 1 | v0.c:83 `c = i % 3;              /* palette 0..2; tile $01 = colour 1 */` 6,068<br>v0.c:34 `OAM_TILE(n) = tile;` 1,752<br>v0.c:36 `OAM_X(n)    = x;` 1,752 |
