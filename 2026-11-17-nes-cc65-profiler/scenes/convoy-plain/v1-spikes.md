| loop (ends at frame) | busy cycles | NMIs | hottest lines in that loop |
| --- | --- | --- | --- |
| 750 (810) | 26,641 | 1 | v1.c:92 `c = i % 3;          /* palette 0..2; tile $01 = colour 1 */` 5,766<br>v1.c:35 `OAM_TILE(n) = tile;` 1,752<br>v1.c:37 `OAM_X(n)    = x;` 1,752 |
| 806 (866) | 26,641 | 1 | v1.c:92 `c = i % 3;          /* palette 0..2; tile $01 = colour 1 */` 5,766<br>v1.c:35 `OAM_TILE(n) = tile;` 1,752<br>v1.c:37 `OAM_X(n)    = x;` 1,752 |
| 47 (107) | 26,639 | 1 | v1.c:92 `c = i % 3;          /* palette 0..2; tile $01 = colour 1 */` 5,766<br>v1.c:35 `OAM_TILE(n) = tile;` 1,752<br>v1.c:37 `OAM_X(n)    = x;` 1,752 |
| 42 (102) | 26,639 | 1 | v1.c:92 `c = i % 3;          /* palette 0..2; tile $01 = colour 1 */` 5,766<br>v1.c:35 `OAM_TILE(n) = tile;` 1,752<br>v1.c:37 `OAM_X(n)    = x;` 1,752 |
| 98 (158) | 26,639 | 1 | v1.c:92 `c = i % 3;          /* palette 0..2; tile $01 = colour 1 */` 5,766<br>v1.c:35 `OAM_TILE(n) = tile;` 1,752<br>v1.c:37 `OAM_X(n)    = x;` 1,752 |
