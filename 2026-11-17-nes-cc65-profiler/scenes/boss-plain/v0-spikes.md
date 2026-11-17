| loop (ends at frame) | busy cycles | NMIs | hottest lines in that loop |
| --- | --- | --- | --- |
| 171 (403) | 36,322 | 2 | v0.c:90 `put_sprite(i++, bx + (c << 3), by + (r << 3), tile, 0);` 10,656<br>v0.c:60 `OAM_TILE(i) = tile;` 3,504<br>v0.c:62 `OAM_X(i)    = (unsigned char)x;` 3,504 |
| 172 (405) | 36,322 | 2 | v0.c:90 `put_sprite(i++, bx + (c << 3), by + (r << 3), tile, 0);` 10,656<br>v0.c:60 `OAM_TILE(i) = tile;` 3,504<br>v0.c:62 `OAM_X(i)    = (unsigned char)x;` 3,504 |
| 173 (407) | 36,322 | 2 | v0.c:90 `put_sprite(i++, bx + (c << 3), by + (r << 3), tile, 0);` 10,656<br>v0.c:60 `OAM_TILE(i) = tile;` 3,504<br>v0.c:62 `OAM_X(i)    = (unsigned char)x;` 3,504 |
| 174 (409) | 36,322 | 2 | v0.c:90 `put_sprite(i++, bx + (c << 3), by + (r << 3), tile, 0);` 10,656<br>v0.c:60 `OAM_TILE(i) = tile;` 3,504<br>v0.c:62 `OAM_X(i)    = (unsigned char)x;` 3,504 |
| 175 (411) | 36,322 | 2 | v0.c:90 `put_sprite(i++, bx + (c << 3), by + (r << 3), tile, 0);` 10,656<br>v0.c:60 `OAM_TILE(i) = tile;` 3,504<br>v0.c:62 `OAM_X(i)    = (unsigned char)x;` 3,504 |
