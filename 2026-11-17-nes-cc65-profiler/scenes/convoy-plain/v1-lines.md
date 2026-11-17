| line | function | cycles/frame | busy | source |
| --- | --- | --- | --- | --- |
| v1.c:92 | `_main` | 5,597 | 21.3% | `c = i % 3;          /* palette 0..2; tile $01 = colour 1 */` |
| v1.c:35 | `_put` | 1,752 | 6.7% | `OAM_TILE(n) = tile;` |
| v1.c:37 | `_put` | 1,752 | 6.7% | `OAM_X(n)    = x;` |
| v1.c:36 | `_put` | 1,704 | 6.5% | `OAM_ATTR(n) = attr;` |
| v1.c:87 | `_main` | 1,627 | 6.2% | `for (k = 0; k < nv; ++k) {` |
| v1.c:34 | `_put` | 1,584 | 6.0% | `OAM_Y(n)    = y;` |
| v1.c:101 | `_main` | 1,148 | 4.4% | `put(n, (unsigned char)(x + 8), CAR_ROW, 0x02, c);` |
| v1.c:96 | `_main` | 1,111 | 4.2% | `put(n, (unsigned char)x, CAR_ROW, 0x01, c);` |
| v1.c:79 | `_main` | 770 | 2.9% | `x = car_x[i];` |
| v1.c:64 | `_main` | 750 | 2.9% | `car_x[i] += 1;` |
| v1.c:91 | `_main` | 747 | 2.8% | `x = car_x[i];` |
| v1.c:33 | `_put` | 720 | 2.7% | `{` |
| (asm) | `nmi` | 621 | 2.4% | `` |
| v1.c:106 | `_main` | 608 | 2.3% | `start = (unsigned char)((s + 4) % nv);` |
| v1.c:100 | `_main` | 594 | 2.3% | `if (x + 8 >= 0 && x + 8 < 256) {` |
