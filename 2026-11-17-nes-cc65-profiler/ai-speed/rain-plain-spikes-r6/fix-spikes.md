| loop (ends at frame) | busy cycles | NMIs | hottest lines in that loop |
| --- | --- | --- | --- |
| 128 (188) | 21,947 | 1 | fix.c:62 `++*s_p;` 1,624<br>fix.c:73 `--*s_p;` 1,595<br>fix.c:86 `if (*s_p >= BAND_MAX) return 0;` 1,434 |
| 129 (189) | 21,825 | 1 | fix.c:73 `--*s_p;` 1,624<br>fix.c:62 `++*s_p;` 1,392<br>fix.c:86 `if (*s_p >= BAND_MAX) return 0;` 1,290 |
| 962 (1022) | 21,430 | 1 | fix.c:73 `--*s_p;` 1,479<br>fix.c:62 `++*s_p;` 1,392<br>fix.c:86 `if (*s_p >= BAND_MAX) return 0;` 1,182 |
| 961 (1021) | 21,360 | 1 | fix.c:62 `++*s_p;` 1,479<br>fix.c:86 `if (*s_p >= BAND_MAX) return 0;` 1,224<br>fix.c:73 `--*s_p;` 1,189 |
| 213 (273) | 21,205 | 1 | fix.c:62 `++*s_p;` 1,450<br>fix.c:73 `--*s_p;` 1,450<br>fix.c:86 `if (*s_p >= BAND_MAX) return 0;` 1,278 |
