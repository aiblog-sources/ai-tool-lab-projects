| loop (ends at frame) | busy cycles | NMIs | hottest lines in that loop |
| --- | --- | --- | --- |
| 130 (190) | 18,351 | 1 | fix.c:156 `for (l = lo; l <= hi; ++l) ++bl[l];` 1,939<br>fix.c:159 `for (l = lo; l <= hi; ++l) --bl[l];` 1,445<br>fix.c:131 `y = oy + dsp[i];` 1,200 |
| 961 (1021) | 18,092 | 1 | fix.c:156 `for (l = lo; l <= hi; ++l) ++bl[l];` 1,913<br>fix.c:176 `DROP_Y(s) = dy[i];` 1,333<br>fix.c:177 `DROP_X(s) = dx[i];` 1,333 |
| 900 (960) | 17,744 | 1 | fix.c:156 `for (l = lo; l <= hi; ++l) ++bl[l];` 1,744<br>fix.c:181 `DROP_Y(s) = dy[i];` 1,204<br>fix.c:182 `DROP_X(s) = dx[i];` 1,204 |
| 809 (869) | 17,729 | 1 | fix.c:156 `for (l = lo; l <= hi; ++l) ++bl[l];` 1,532<br>fix.c:131 `y = oy + dsp[i];` 1,200<br>fix.c:143 `(unsigned int)dx[i] < (unsigned int)ux + UMB_W);` 1,085 |
| 646 (706) | 17,699 | 1 | fix.c:156 `for (l = lo; l <= hi; ++l) ++bl[l];` 1,515<br>fix.c:143 `(unsigned int)dx[i] < (unsigned int)ux + UMB_W);` 1,395<br>fix.c:159 `for (l = lo; l <= hi; ++l) --bl[l];` 1,374 |
