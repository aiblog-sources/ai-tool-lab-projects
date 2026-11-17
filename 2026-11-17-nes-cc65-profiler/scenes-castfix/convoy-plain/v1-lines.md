| line | function | cycles/frame | busy | source |
| --- | --- | --- | --- | --- |
| v1.c:92 | `_main` | 5,607 | 24.6% | `c = i % 3;          /* palette 0..2; tile $01 = colour 1 */` |
| v1.c:87 | `_main` | 1,627 | 7.1% | `for (k = 0; k < nv; ++k) {` |
| v1.c:101 | `_main` | 1,148 | 5.0% | `put(n, (unsigned char)(x + 8), CAR_ROW, 0x02, c);` |
| v1.c:96 | `_main` | 1,111 | 4.9% | `put(n, (unsigned char)x, CAR_ROW, 0x01, c);` |
| v1.c:35 | `_put` | 864 | 3.8% | `OAM_TILE(n) = tile;` |
| v1.c:37 | `_put` | 864 | 3.8% | `OAM_X(n)    = x;` |
| v1.c:34 | `_put` | 816 | 3.6% | `OAM_Y(n)    = y;` |
| v1.c:36 | `_put` | 816 | 3.6% | `OAM_ATTR(n) = attr;` |
| v1.c:79 | `_main` | 770 | 3.4% | `x = car_x[i];` |
| v1.c:64 | `_main` | 750 | 3.3% | `car_x[i] += 1;` |
| v1.c:91 | `_main` | 747 | 3.3% | `x = car_x[i];` |
| v1.c:33 | `_put` | 720 | 3.2% | `{` |
| (asm) | `nmi` | 621 | 2.7% | `` |
| v1.c:106 | `_main` | 609 | 2.7% | `start = (unsigned char)((s + 4) % nv);` |
| v1.c:100 | `_main` | 594 | 2.6% | `if (x + 8 >= 0 && x + 8 < 256) {` |
