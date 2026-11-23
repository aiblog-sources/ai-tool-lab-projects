# Results

## First try and after fixes

| Prompt | Model | CPU | Task | First try | After 1 fix | After 2 fixes | First-try failures |
| --- | --- | --- | --- | --- | --- | --- | --- |
| plain | claude-opus-5-5 | 6502 | big | 3/3 | 3/3 | 3/3 | - |
| plain | claude-opus-5-5 | 6502 | col | 3/3 | 3/3 | 3/3 | - |
| plain | claude-opus-5-5 | 6502 | pal | 3/3 | 3/3 | 3/3 | - |
| plain | claude-opus-5-5 | 6502 | row | 3/3 | 3/3 | 3/3 | - |
| plain | claude-opus-5-5 | z80 | big | 3/3 | 3/3 | 3/3 | - |
| plain | claude-opus-5-5 | z80 | col | 3/3 | 3/3 | 3/3 | - |
| plain | claude-opus-5-5 | z80 | pal | 3/3 | 3/3 | 3/3 | - |
| plain | claude-opus-5-5 | z80 | row | 3/3 | 3/3 | 3/3 | - |
| plain | claude-sonnet-5-5 | 6502 | big | 3/3 | 3/3 | 3/3 | - |
| plain | claude-sonnet-5-5 | 6502 | col | 3/3 | 3/3 | 3/3 | - |
| plain | claude-sonnet-5-5 | 6502 | pal | 3/3 | 3/3 | 3/3 | - |
| plain | claude-sonnet-5-5 | 6502 | row | 3/3 | 3/3 | 3/3 | - |
| plain | claude-sonnet-5-5 | z80 | big | 3/3 | 3/3 | 3/3 | - |
| plain | claude-sonnet-5-5 | z80 | col | 3/3 | 3/3 | 3/3 | - |
| plain | claude-sonnet-5-5 | z80 | pal | 3/3 | 3/3 | 3/3 | - |
| plain | claude-sonnet-5-5 | z80 | row | 3/3 | 3/3 | 3/3 | - |
| warned | claude-opus-5-5 | 6502 | big | 3/3 | 3/3 | 3/3 | - |
| warned | claude-opus-5-5 | 6502 | col | 3/3 | 3/3 | 3/3 | - |
| warned | claude-opus-5-5 | 6502 | huge | 0/3 | 2/3 | 2/3 | r1: late, wrong bytes; r2: late, wrong bytes; r3: late, wrong bytes |
| warned | claude-opus-5-5 | 6502 | pal | 3/3 | 3/3 | 3/3 | - |
| warned | claude-opus-5-5 | 6502 | row | 3/3 | 3/3 | 3/3 | - |
| warned | claude-opus-5-5 | z80 | big | 3/3 | 3/3 | 3/3 | - |
| warned | claude-opus-5-5 | z80 | col | 3/3 | 3/3 | 3/3 | - |
| warned | claude-opus-5-5 | z80 | huge | 1/3 | 3/3 | 3/3 | r1: late; r2: late |
| warned | claude-opus-5-5 | z80 | pal | 3/3 | 3/3 | 3/3 | - |
| warned | claude-opus-5-5 | z80 | row | 3/3 | 3/3 | 3/3 | - |
| warned | claude-sonnet-5-5 | 6502 | big | 3/3 | 3/3 | 3/3 | - |
| warned | claude-sonnet-5-5 | 6502 | col | 3/3 | 3/3 | 3/3 | - |
| warned | claude-sonnet-5-5 | 6502 | huge | 0/3 | 0/3 | 0/3 | r1: wrong bytes; r2: wrong bytes; r3: wrong bytes |
| warned | claude-sonnet-5-5 | 6502 | pal | 3/3 | 3/3 | 3/3 | - |
| warned | claude-sonnet-5-5 | 6502 | row | 3/3 | 3/3 | 3/3 | - |
| warned | claude-sonnet-5-5 | z80 | big | 3/3 | 3/3 | 3/3 | - |
| warned | claude-sonnet-5-5 | z80 | col | 3/3 | 3/3 | 3/3 | - |
| warned | claude-sonnet-5-5 | z80 | huge | 0/3 | 1/3 | 1/3 | r1: wrong bytes; r2: wrong bytes; r3: late, wrong bytes |
| warned | claude-sonnet-5-5 | z80 | pal | 3/3 | 3/3 | 3/3 | - |
| warned | claude-sonnet-5-5 | z80 | row | 3/3 | 3/3 | 3/3 | - |

| Prompt | Model | CPU | First try | After 1 fix | After 2 fixes |
| --- | --- | --- | --- | --- | --- |
| huge | claude-opus-5-5 | 6502 | 0/3 | 2/3 | 2/3 |
| huge | claude-opus-5-5 | z80 | 1/3 | 3/3 | 3/3 |
| huge | claude-sonnet-5-5 | 6502 | 0/3 | 0/3 | 0/3 |
| huge | claude-sonnet-5-5 | z80 | 0/3 | 1/3 | 1/3 |
| plain | claude-opus-5-5 | 6502 | 12/12 | 12/12 | 12/12 |
| plain | claude-opus-5-5 | z80 | 12/12 | 12/12 | 12/12 |
| plain | claude-sonnet-5-5 | 6502 | 12/12 | 12/12 | 12/12 |
| plain | claude-sonnet-5-5 | z80 | 12/12 | 12/12 | 12/12 |
| warned | claude-opus-5-5 | 6502 | 12/12 | 12/12 | 12/12 |
| warned | claude-opus-5-5 | z80 | 12/12 | 12/12 | 12/12 |
| warned | claude-sonnet-5-5 | 6502 | 12/12 | 12/12 | 12/12 |
| warned | claude-sonnet-5-5 | z80 | 12/12 | 12/12 | 12/12 |

## Timing of every first try that assembled

NES: CPU cycles from the first instruction of the NMI handler to the end of the last PPU write (OAM DMA counted as 513 cycles); vertical blank ends 2260-2262 cycles after it. Master System: Z80 cycles from the start of line 192 to the last VDP write; the handler starts at 252 and vertical blank ends at 15960.

| Prompt | Model | CPU | Task | Last write (min-max over reps) | VBlank end | Budget claimed | Cycles claimed | Measured routine cycles |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| plain | claude-opus-5-5 | 6502 | big | 1857 | 2260 | - | - | 1844 |
| plain | claude-opus-5-5 | 6502 | col | 300-511 | 2261 | - | - | 287-498 |
| plain | claude-opus-5-5 | 6502 | pal | 542-544 | 2262 | - | - | 529-531 |
| plain | claude-opus-5-5 | 6502 | row | 552-556 | 2260 | - | - | 539-543 |
| plain | claude-opus-5-5 | z80 | big | 12852 | 15960 | - | - | 12454 |
| plain | claude-opus-5-5 | z80 | col | 3117-3346 | 15960 | - | - | 2719-2948 |
| plain | claude-opus-5-5 | z80 | pal | 1135 | 15960 | - | - | 737 |
| plain | claude-opus-5-5 | z80 | row | 1915 | 15960 | - | - | 1517 |
| plain | claude-sonnet-5-5 | 6502 | big | 1861 | 2261 | - | - | 1848 |
| plain | claude-sonnet-5-5 | 6502 | col | 509-513 | 2262 | - | - | 496-500 |
| plain | claude-sonnet-5-5 | 6502 | pal | 534-542 | 2262 | - | - | 521-529 |
| plain | claude-sonnet-5-5 | 6502 | row | 556-558 | 2261 | - | - | 543-545 |
| plain | claude-sonnet-5-5 | z80 | big | 13305-13776 | 15960 | - | - | 12907-13378 |
| plain | claude-sonnet-5-5 | z80 | col | 3328-3496 | 15960 | - | - | 2930-3098 |
| plain | claude-sonnet-5-5 | z80 | pal | 1135-1141 | 15960 | - | - | 737-743 |
| plain | claude-sonnet-5-5 | z80 | row | 1861-1926 | 15960 | - | - | 1463-1528 |
| warned | claude-opus-5-5 | 6502 | big | 1851-1857 | 2260 | 2273 | 1840-1846 | 1838-1844 |
| warned | claude-opus-5-5 | 6502 | col | 298-509 | 2261 | 2273 | 285-496 | 285-496 |
| warned | claude-opus-5-5 | 6502 | huge | 2615-2625 | 2261 | 2273 | 2604-2614 | 2602-2612 |
| warned | claude-opus-5-5 | 6502 | pal | 307-532 | 2261 | 2273 | 294-519 | 294-519 |
| warned | claude-opus-5-5 | 6502 | row | 327-552 | 2260 | 2273 | 314-539 | 314-539 |
| warned | claude-opus-5-5 | z80 | big | 12775-13104 | 15960 | 15960 | 12375-12704 | 12377-12706 |
| warned | claude-opus-5-5 | z80 | col | 2878-3250 | 15960 | 15960 | 2478-2850 | 2480-2852 |
| warned | claude-opus-5-5 | z80 | huge | 8758-16871 | 15960 | 15960 | 8356-16471 | 8360-16473 |
| warned | claude-opus-5-5 | z80 | pal | 979-1135 | 15960 | 15960 | 579-737 | 581-737 |
| warned | claude-opus-5-5 | z80 | row | 1538-1918 | 15960 | 15960 | 1138-1518 | 1140-1520 |
| warned | claude-sonnet-5-5 | 6502 | big | 1851-1861 | 2260 | 2273 | 1840-1850 | 1838-1848 |
| warned | claude-sonnet-5-5 | 6502 | col | 509-513 | 2262 | 2273 | 496-500 | 496-500 |
| warned | claude-sonnet-5-5 | 6502 | huge | 2167-2181 | 2260 | 2273 | 2156-2170 | 2154-2168 |
| warned | claude-sonnet-5-5 | 6502 | pal | 538-550 | 2261 | 2273 | 525-537 | 525-537 |
| warned | claude-sonnet-5-5 | 6502 | row | 552-556 | 2261 | 2273 | 539-543 | 539-543 |
| warned | claude-sonnet-5-5 | z80 | big | 13285-13440 | 15960 | 15960 | 12892-13040 | 12887-13042 |
| warned | claude-sonnet-5-5 | z80 | col | 3120-3455 | 15960 | 15732-15960 | 2720-3057 | 2722-3057 |
| warned | claude-sonnet-5-5 | z80 | huge | 15732-16048 | 15960 | 15960 | 15334-15650 | 15334-15650 |
| warned | claude-sonnet-5-5 | z80 | pal | 1141 | 15960 | 15960 | 741 | 743 |
| warned | claude-sonnet-5-5 | z80 | row | 1850-1866 | 15960 | 15960 | 1452-1468 | 1452-1468 |

6502: cycle claims within 1% of the measurement in 30/30 first tries, within 5% in 30/30.

z80: cycle claims within 1% of the measurement in 30/30 first tries, within 5% in 30/30.

Claude calls: 125, cost at API prices USD 4.20, median wall time per call 15 s.
