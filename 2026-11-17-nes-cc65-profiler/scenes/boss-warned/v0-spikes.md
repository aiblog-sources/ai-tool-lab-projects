| loop (ends at frame) | busy cycles | NMIs | hottest lines in that loop |
| --- | --- | --- | --- |
| 183 (427) | 45,754 | 2 | v0.c:49 `return qsin[i] + (unsigned char)(((unsigned char)(qsin[i + 1] - qsin[i]) * f) >> 3);` 10,223<br>v0.c:119 `OAM_X(s)    = (unsigned char)(x + (c << 3));` 4,932<br>v0.c:111 `unsigned char s = SPR_BOSS + r * 6 + c;` 3,888 |
| 191 (443) | 45,754 | 2 | v0.c:49 `return qsin[i] + (unsigned char)(((unsigned char)(qsin[i + 1] - qsin[i]) * f) >> 3);` 10,223<br>v0.c:119 `OAM_X(s)    = (unsigned char)(x + (c << 3));` 4,932<br>v0.c:111 `unsigned char s = SPR_BOSS + r * 6 + c;` 3,888 |
| 199 (459) | 45,754 | 2 | v0.c:49 `return qsin[i] + (unsigned char)(((unsigned char)(qsin[i + 1] - qsin[i]) * f) >> 3);` 10,223<br>v0.c:119 `OAM_X(s)    = (unsigned char)(x + (c << 3));` 4,932<br>v0.c:111 `unsigned char s = SPR_BOSS + r * 6 + c;` 3,888 |
| 207 (475) | 45,754 | 2 | v0.c:49 `return qsin[i] + (unsigned char)(((unsigned char)(qsin[i + 1] - qsin[i]) * f) >> 3);` 10,223<br>v0.c:119 `OAM_X(s)    = (unsigned char)(x + (c << 3));` 4,932<br>v0.c:111 `unsigned char s = SPR_BOSS + r * 6 + c;` 3,888 |
| 175 (411) | 45,754 | 2 | v0.c:49 `return qsin[i] + (unsigned char)(((unsigned char)(qsin[i + 1] - qsin[i]) * f) >> 3);` 10,223<br>v0.c:119 `OAM_X(s)    = (unsigned char)(x + (c << 3));` 4,932<br>v0.c:111 `unsigned char s = SPR_BOSS + r * 6 + c;` 3,888 |
