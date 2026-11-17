| line | function | cycles/frame | busy | source |
| --- | --- | --- | --- | --- |
| v0.c:83 | `_main` | 6,068 | 27.6% | `c = i % 3;              /* palette 0..2; tile $01 = colour 1 */` |
| v0.c:34 | `_put` | 1,752 | 8.0% | `OAM_TILE(n) = tile;` |
| v0.c:36 | `_put` | 1,752 | 8.0% | `OAM_X(n)    = x;` |
| v0.c:35 | `_put` | 1,704 | 7.7% | `OAM_ATTR(n) = attr;` |
| v0.c:33 | `_put` | 1,584 | 7.2% | `OAM_Y(n)    = y;` |
| v0.c:94 | `_main` | 800 | 3.6% | `put(n, (unsigned char)(x + 8), CAR_ROW, 0x02, c);` |
| v0.c:87 | `_main` | 762 | 3.5% | `put(n, (unsigned char)x, CAR_ROW, 0x01, c);` |
| v0.c:63 | `_main` | 750 | 3.4% | `car_x[i] += 1;` |
| v0.c:82 | `_main` | 750 | 3.4% | `x = car_x[i];` |
| v0.c:32 | `_put` | 720 | 3.3% | `{` |
| (asm) | `nmi` | 621 | 2.8% | `` |
| v0.c:93 | `_main` | 602 | 2.7% | `if (x + 8 >= 0 && x + 8 < 256)` |
| v0.c:64 | `_main` | 550 | 2.5% | `if (car_x[i] >= 256)` |
| nes.c:24 | `_pad_read` | 421 | 1.9% | `` |
| v0.c:96 | `_main` | 411 | 1.9% | `put(n, 0, 0xFF, 0x00, 0);` |
