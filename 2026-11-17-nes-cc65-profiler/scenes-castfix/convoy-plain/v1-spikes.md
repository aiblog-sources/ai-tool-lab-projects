| loop (ends at frame) | busy cycles | NMIs | hottest lines in that loop |
| --- | --- | --- | --- |
| 750 (810) | 23,210 | 1 | v1.c:92 `c = i % 3;          /* palette 0..2; tile $01 = colour 1 */` 5,776<br>v1.c:87 `for (k = 0; k < nv; ++k) {` 1,672<br>v1.c:101 `put(n, (unsigned char)(x + 8), CAR_ROW, 0x02, c);` 1,220 |
| 806 (866) | 23,210 | 1 | v1.c:92 `c = i % 3;          /* palette 0..2; tile $01 = colour 1 */` 5,776<br>v1.c:87 `for (k = 0; k < nv; ++k) {` 1,672<br>v1.c:101 `put(n, (unsigned char)(x + 8), CAR_ROW, 0x02, c);` 1,220 |
| 47 (107) | 23,208 | 1 | v1.c:92 `c = i % 3;          /* palette 0..2; tile $01 = colour 1 */` 5,776<br>v1.c:87 `for (k = 0; k < nv; ++k) {` 1,672<br>v1.c:101 `put(n, (unsigned char)(x + 8), CAR_ROW, 0x02, c);` 1,220 |
| 42 (102) | 23,208 | 1 | v1.c:92 `c = i % 3;          /* palette 0..2; tile $01 = colour 1 */` 5,776<br>v1.c:87 `for (k = 0; k < nv; ++k) {` 1,672<br>v1.c:101 `put(n, (unsigned char)(x + 8), CAR_ROW, 0x02, c);` 1,220 |
| 98 (158) | 23,208 | 1 | v1.c:92 `c = i % 3;          /* palette 0..2; tile $01 = colour 1 */` 5,776<br>v1.c:87 `for (k = 0; k < nv; ++k) {` 1,672<br>v1.c:101 `put(n, (unsigned char)(x + 8), CAR_ROW, 0x02, c);` 1,220 |
