| line | function | cycles/frame | busy | source |
| --- | --- | --- | --- | --- |
| v0.c:49 | `_sin_q` | 5,093 | 26.2% | `return qsin[i] + (unsigned char)(((unsigned char)(qsin[i + 1] - qsin[i]) * f) >> 3);` |
| v0.c:111 | `_draw` | 1,944 | 10.0% | `unsigned char s = SPR_BOSS + r * 6 + c;` |
| v0.c:119 | `_draw` | 1,800 | 9.3% | `OAM_X(s)    = (unsigned char)(x + (c << 3));` |
| v0.c:116 | `_draw` | 864 | 4.4% | `OAM_Y(s)    = BOSS_TOP + (r << 3) - 1;` |
| v0.c:90 | `_draw` | 808 | 4.2% | `x = cx + isin(a + 64) - 4;` |
| v0.c:58 | `_isin` | 671 | 3.5% | `else        v = sin_q(idx);` |
| v0.c:117 | `_draw` | 612 | 3.2% | `OAM_TILE(s) = t;` |
| v0.c:110 | `_draw` | 501 | 2.6% | `for (c = 0; c < 6; ++c) {` |
| v0.c:118 | `_draw` | 486 | 2.5% | `OAM_ATTR(s) = 0x00;` |
| v0.c:59 | `_isin` | 460 | 2.4% | `if (a & 128) v = -v;` |
| v0.c:112 | `_draw` | 406 | 2.1% | `if (r == 0 \|\| r == 5 \|\| c == 0 \|\| c == 5) t = 0x01;       /* edge */` |
| v0.c:89 | `_draw` | 396 | 2.0% | `unsigned char a = orbit_ang + (i << 5);` |
| v0.c:91 | `_draw` | 382 | 2.0% | `y = BOSS_CY + isin(a) - 4;` |
| v0.c:47 | `_sin_q` | 366 | 1.9% | `unsigned char i = idx >> 3;` |
| v0.c:120 | `_draw` | 360 | 1.9% | `}` |
