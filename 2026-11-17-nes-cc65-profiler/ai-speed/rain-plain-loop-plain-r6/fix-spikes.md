| loop (ends at frame) | busy cycles | NMIs | hottest lines in that loop |
| --- | --- | --- | --- |
| 131 (191) | 23,946 | 1 | fix.c:128 `y = dy[i] + dsp[i];` 1,600<br>fix.c:156 `++q;` 1,378<br>fix.c:176 `++q;` 1,320 |
| 133 (193) | 23,274 | 1 | fix.c:128 `y = dy[i] + dsp[i];` 1,600<br>fix.c:156 `++q;` 1,166<br>fix.c:176 `++q;` 1,155 |
| 783 (843) | 23,008 | 1 | fix.c:128 `y = dy[i] + dsp[i];` 1,600<br>fix.c:138 `(unsigned int)dx[i] < (unsigned int)ux + UMB_W)) {` 1,430<br>fix.c:199 `DROP_OAM_Y[o] = dy[i];` 1,290 |
| 213 (273) | 22,958 | 1 | fix.c:128 `y = dy[i] + dsp[i];` 1,600<br>fix.c:176 `++q;` 1,430<br>fix.c:156 `++q;` 1,378 |
| 1008 (1068) | 22,844 | 1 | fix.c:128 `y = dy[i] + dsp[i];` 1,600<br>fix.c:138 `(unsigned int)dx[i] < (unsigned int)ux + UMB_W)) {` 1,286<br>fix.c:156 `++q;` 1,113 |
