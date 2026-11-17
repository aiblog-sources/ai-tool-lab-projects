| loop (ends at frame) | busy cycles | NMIs | hottest lines in that loop |
| --- | --- | --- | --- |
| 783 (843) | 18,370 | 1 | v0.c:83 `c = i % 3;              /* palette 0..2; tile $01 = colour 1 */` 5,766<br>v0.c:34 `OAM_TILE(n) = tile;` 864<br>v0.c:36 `OAM_X(n)    = x;` 864 |
| 813 (873) | 18,370 | 1 | v0.c:83 `c = i % 3;              /* palette 0..2; tile $01 = colour 1 */` 5,766<br>v0.c:34 `OAM_TILE(n) = tile;` 864<br>v0.c:36 `OAM_X(n)    = x;` 864 |
| 1083 (1143) | 18,369 | 1 | v0.c:83 `c = i % 3;              /* palette 0..2; tile $01 = colour 1 */` 5,766<br>v0.c:34 `OAM_TILE(n) = tile;` 864<br>v0.c:36 `OAM_X(n)    = x;` 864 |
| 703 (763) | 18,364 | 1 | v0.c:83 `c = i % 3;              /* palette 0..2; tile $01 = colour 1 */` 5,766<br>v0.c:34 `OAM_TILE(n) = tile;` 864<br>v0.c:36 `OAM_X(n)    = x;` 864 |
| 403 (463) | 18,364 | 1 | v0.c:83 `c = i % 3;              /* palette 0..2; tile $01 = colour 1 */` 5,766<br>v0.c:34 `OAM_TILE(n) = tile;` 864<br>v0.c:36 `OAM_X(n)    = x;` 864 |
