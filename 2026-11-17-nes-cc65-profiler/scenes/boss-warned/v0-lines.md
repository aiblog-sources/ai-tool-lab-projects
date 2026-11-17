| line | function | cycles/frame | busy | source |
| --- | --- | --- | --- | --- |
| v0.c:49 | `_sin_q` | 5,093 | 22.3% | `return qsin[i] + (unsigned char)(((unsigned char)(qsin[i + 1] - qsin[i]) * f) >> 3);` |
| v0.c:119 | `_draw` | 2,466 | 10.8% | `OAM_X(s)    = (unsigned char)(x + (c << 3));` |
| v0.c:111 | `_draw` | 1,944 | 8.5% | `unsigned char s = SPR_BOSS + r * 6 + c;` |
| v0.c:116 | `_draw` | 1,422 | 6.2% | `OAM_Y(s)    = BOSS_TOP + (r << 3) - 1;` |
| v0.c:117 | `_draw` | 1,278 | 5.6% | `OAM_TILE(s) = t;` |
| v0.c:118 | `_draw` | 1,152 | 5.0% | `OAM_ATTR(s) = 0x00;` |
| v0.c:90 | `_draw` | 808 | 3.5% | `x = cx + isin(a + 64) - 4;` |
| v0.c:58 | `_isin` | 671 | 2.9% | `else        v = sin_q(idx);` |
| v0.c:110 | `_draw` | 501 | 2.2% | `for (c = 0; c < 6; ++c) {` |
| v0.c:59 | `_isin` | 460 | 2.0% | `if (a & 128) v = -v;` |
| v0.c:112 | `_draw` | 406 | 1.8% | `if (r == 0 \|\| r == 5 \|\| c == 0 \|\| c == 5) t = 0x01;       /* edge */` |
| v0.c:89 | `_draw` | 396 | 1.7% | `unsigned char a = orbit_ang + (i << 5);` |
| v0.c:91 | `_draw` | 382 | 1.7% | `y = BOSS_CY + isin(a) - 4;` |
| v0.c:47 | `_sin_q` | 366 | 1.6% | `unsigned char i = idx >> 3;` |
| v0.c:120 | `_draw` | 360 | 1.6% | `}` |
