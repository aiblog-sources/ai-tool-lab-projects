| loop (ends at frame) | busy cycles | NMIs | hottest lines in that loop |
| --- | --- | --- | --- |
| 128 (188) | 27,572 | 1 | fix.c:56 `*s_p += d;` 5,106<br>fix.c:71 `if (*s_p >= BAND_MAX) return 0;` 1,434<br>fix.c:57 `++s_p;` 999 |
| 962 (1022) | 27,455 | 1 | fix.c:56 `*s_p += d;` 4,554<br>fix.c:71 `if (*s_p >= BAND_MAX) return 0;` 1,182<br>fix.c:164 `op[3] = dx[i];` 1,110 |
| 129 (189) | 27,448 | 1 | fix.c:56 `*s_p += d;` 4,784<br>fix.c:71 `if (*s_p >= BAND_MAX) return 0;` 1,290<br>fix.c:57 `++s_p;` 936 |
| 961 (1021) | 27,188 | 1 | fix.c:56 `*s_p += d;` 4,232<br>fix.c:71 `if (*s_p >= BAND_MAX) return 0;` 1,224<br>fix.c:164 `op[3] = dx[i];` 1,147 |
| 130 (190) | 27,121 | 1 | fix.c:56 `*s_p += d;` 4,508<br>fix.c:71 `if (*s_p >= BAND_MAX) return 0;` 1,200<br>fix.c:57 `++s_p;` 882 |
