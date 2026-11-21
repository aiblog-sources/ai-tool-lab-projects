## Pass rates (all vectors right)

| Model | CPU | First try | After 1 fix | After 2 fixes | Assembled first try |
| --- | --- | --- | --- | --- | --- |
| claude-opus-5-5 | 6502 | 30/30 | 30/30 | 30/30 | 30/30 |
| claude-opus-5-5 | z80 | 28/30 | 30/30 | 30/30 | 28/30 |
| claude-sonnet-5-5 | 6502 | 29/30 | 30/30 | 30/30 | 30/30 |
| claude-sonnet-5-5 | z80 | 30/30 | 30/30 | 30/30 | 30/30 |

## First try per task (both models, 3 reps each)

| Task | 6502 first try | 6502 final | Z80 first try | Z80 final |
| --- | --- | --- | --- | --- |
| popcount | 6/6 | 6/6 | 6/6 | 6/6 |
| mul8 | 5/6 | 6/6 | 6/6 | 6/6 |
| div16by8 | 6/6 | 6/6 | 6/6 | 6/6 |
| utoa16 | 6/6 | 6/6 | 6/6 | 6/6 |
| isqrt16 | 6/6 | 6/6 | 5/6 | 6/6 |
| adds8 | 6/6 | 6/6 | 6/6 | 6/6 |
| cmp_s16 | 6/6 | 6/6 | 6/6 | 6/6 |
| memmove | 6/6 | 6/6 | 5/6 | 6/6 |
| sort_u8 | 6/6 | 6/6 | 6/6 | 6/6 |
| box_overlap | 6/6 | 6/6 | 6/6 | 6/6 |

## Per model and task (first try / final, out of 3)


claude-opus-5-5

| Task | 6502 | Z80 |
| --- | --- | --- |
| popcount | 3 / 3 | 3 / 3 |
| mul8 | 3 / 3 | 3 / 3 |
| div16by8 | 3 / 3 | 3 / 3 |
| utoa16 | 3 / 3 | 3 / 3 |
| isqrt16 | 3 / 3 | 2 / 3 |
| adds8 | 3 / 3 | 3 / 3 |
| cmp_s16 | 3 / 3 | 3 / 3 |
| memmove | 3 / 3 | 2 / 3 |
| sort_u8 | 3 / 3 | 3 / 3 |
| box_overlap | 3 / 3 | 3 / 3 |

claude-sonnet-5-5

| Task | 6502 | Z80 |
| --- | --- | --- |
| popcount | 3 / 3 | 3 / 3 |
| mul8 | 2 / 3 | 3 / 3 |
| div16by8 | 3 / 3 | 3 / 3 |
| utoa16 | 3 / 3 | 3 / 3 |
| isqrt16 | 3 / 3 | 3 / 3 |
| adds8 | 3 / 3 | 3 / 3 |
| cmp_s16 | 3 / 3 | 3 / 3 |
| memmove | 3 / 3 | 3 / 3 |
| sort_u8 | 3 / 3 | 3 / 3 |
| box_overlap | 3 / 3 | 3 / 3 |

## What went wrong on the first try


6502:

- 1 x mul8: product # want #

z80:

- 2 x did not assemble

## Cycle claims vs measured worst case (exhaustive tasks, routines that passed)

| CPU | Routines | Exact | Within 10% | Under-claimed by >10% | Over-claimed by >10% | No claim |
| --- | --- | --- | --- | --- | --- | --- |
| 6502 | 24 | 18 | 24 | 0 | 0 | 0 |
| z80 | 24 | 19 | 24 | 0 | 0 | 0 |

Each claim against the measurement (task, model, rep, round: claimed / measured max):

- 6502 isqrt16 claude-opus-5-5 r1: 803 / 803
- 6502 isqrt16 claude-opus-5-5 r2: 771 / 771
- 6502 isqrt16 claude-opus-5-5 r3: 835 / 835
- 6502 isqrt16 claude-sonnet-5-5 r1: 823 / 819
- 6502 isqrt16 claude-sonnet-5-5 r2: 857 / 859
- 6502 isqrt16 claude-sonnet-5-5 r3: 907 / 907
- 6502 mul8 claude-opus-5-5 r1: 134 / 134
- 6502 mul8 claude-opus-5-5 r2: 181 / 181
- 6502 mul8 claude-opus-5-5 r3: 137 / 137
- 6502 mul8 claude-sonnet-5-5 r1: 181 / 181
- 6502 mul8 claude-sonnet-5-5 r2: 210 / 210
- 6502 mul8 claude-sonnet-5-5 r3: 181 / 181
- 6502 popcount claude-opus-5-5 r1: 19 / 19
- 6502 popcount claude-opus-5-5 r2: 19 / 19
- 6502 popcount claude-opus-5-5 r3: 19 / 19
- 6502 popcount claude-sonnet-5-5 r1: 42 / 40
- 6502 popcount claude-sonnet-5-5 r2: 19 / 19
- 6502 popcount claude-sonnet-5-5 r3: 48 / 46
- 6502 utoa16 claude-opus-5-5 r1: 737 / 737
- 6502 utoa16 claude-opus-5-5 r2: 787 / 787
- 6502 utoa16 claude-opus-5-5 r3: 1422 / 1422
- 6502 utoa16 claude-sonnet-5-5 r1: 1300 / 1300
- 6502 utoa16 claude-sonnet-5-5 r2: 1895 / 1798
- 6502 utoa16 claude-sonnet-5-5 r3: 1899 / 1798
- z80 isqrt16 claude-opus-5-5 r1: 290 / 290
- z80 isqrt16 claude-opus-5-5 r2: 1044 / 1040
- z80 isqrt16 claude-opus-5-5 r3: 962 / 962
- z80 isqrt16 claude-sonnet-5-5 r1: 1071 / 1071
- z80 isqrt16 claude-sonnet-5-5 r2: 1111 / 1111
- z80 isqrt16 claude-sonnet-5-5 r3: 1285 / 1285
- z80 mul8 claude-opus-5-5 r1: 255 / 259
- z80 mul8 claude-opus-5-5 r2: 260 / 259
- z80 mul8 claude-opus-5-5 r3: 255 / 259
- z80 mul8 claude-sonnet-5-5 r1: 379 / 379
- z80 mul8 claude-sonnet-5-5 r2: 379 / 379
- z80 mul8 claude-sonnet-5-5 r3: 379 / 379
- z80 popcount claude-opus-5-5 r1: 45 / 45
- z80 popcount claude-opus-5-5 r2: 45 / 45
- z80 popcount claude-opus-5-5 r3: 45 / 45
- z80 popcount claude-sonnet-5-5 r1: 127 / 127
- z80 popcount claude-sonnet-5-5 r2: 123 / 123
- z80 popcount claude-sonnet-5-5 r3: 45 / 45
- z80 utoa16 claude-opus-5-5 r1: 1282 / 1282
- z80 utoa16 claude-opus-5-5 r2: 1282 / 1282
- z80 utoa16 claude-opus-5-5 r3: 1285 / 1285
- z80 utoa16 claude-sonnet-5-5 r1: 1282 / 1282
- z80 utoa16 claude-sonnet-5-5 r2: 1285 / 1285
- z80 utoa16 claude-sonnet-5-5 r3: 1268 / 1285

## Speed of the routines that passed (worst case over the test vectors, call and return included)

| Task | 6502 cycles (min to max over routines) | 6502 us | Z80 cycles | Z80 us |
| --- | --- | --- | --- | --- |
| popcount | 19 to 46 | 11 to 26 | 45 to 127 | 13 to 35 |
| mul8 | 134 to 210 | 75 to 117 | 259 to 379 | 72 to 106 |
| div16by8 | 472 to 734 | 264 to 410 | 687 to 897 | 192 to 251 |
| utoa16 | 737 to 1,798 | 412 to 1005 | 1,282 to 1,285 | 358 to 359 |
| isqrt16 | 771 to 907 | 431 to 507 | 290 to 1,285 | 81 to 359 |
| adds8 | 31 to 35 | 17 to 20 | 54 to 54 | 15 to 15 |
| cmp_s16 | 30 to 30 | 17 to 17 | 60 to 72 | 17 to 20 |
| memmove | 609 to 623 | 340 to 348 | 709 to 858 | 198 to 240 |
| sort_u8 | 24,210 to 62,266 | 13527 to 34790 | 13,459 to 150,360 | 3760 to 42005 |
| box_overlap | 58 to 137 | 32 to 77 | 204 to 423 | 57 to 118 |

## Cost and time

- claude-opus-5-5: 62 `claude -p` calls, USD 4.52 at API prices, median 23 s per call, median 1734 output tokens
- claude-sonnet-5-5: 61 `claude -p` calls, USD 1.16 at API prices, median 12 s per call, median 1347 output tokens
