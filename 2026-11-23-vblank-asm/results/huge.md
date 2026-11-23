# The copy that cannot fit (task `huge`)

NES: OAM DMA + 256 bytes; Master System: 1024 bytes. Last write in cycles (NES: from the NMI handler's first instruction, vertical blank ends at 2260-2263; Master System: from line 192, ends at 15960).

| Model | CPU | Rep | Round 0 | Round 1 | Round 2 |
| --- | --- | --- | --- | --- | --- |
| claude-sonnet-5-5 | 6502 | 1 | truncated, last write 2173, 56 bytes missing or wrong | truncated, last write 2173, 56 bytes missing or wrong | truncated, last write 2237, 48 bytes missing or wrong |
| claude-sonnet-5-5 | 6502 | 2 | truncated, last write 2167, 56 bytes missing or wrong | truncated, last write 2167, 56 bytes missing or wrong | late, last write 2639 |
| claude-sonnet-5-5 | 6502 | 3 | truncated, last write 2181, 56 bytes missing or wrong | truncated, last write 2181, 56 bytes missing or wrong | truncated, last write 2241, 48 bytes missing or wrong |
| claude-sonnet-5-5 | z80 | 1 | truncated, last write 15732, 96 bytes missing or wrong | late, last write 16048, 64 bytes missing or wrong | truncated, last write 15943, 58 bytes missing or wrong |
| claude-sonnet-5-5 | z80 | 2 | truncated, last write 15847, 64 bytes missing or wrong | two frames, last write 15926 |  |
| claude-sonnet-5-5 | z80 | 3 | late, last write 16048, 64 bytes missing or wrong | truncated, last write 15943, 58 bytes missing or wrong | late, last write 16077 |
| claude-opus-5-5 | 6502 | 1 | late, last write 2625, 43 bytes missing or wrong | two frames, last write 1624 |  |
| claude-opus-5-5 | 6502 | 2 | late, last write 2625, 43 bytes missing or wrong | late, last write 2625, 43 bytes missing or wrong | truncated, last write 2257, 46 bytes missing or wrong |
| claude-opus-5-5 | 6502 | 3 | late, last write 2615, 43 bytes missing or wrong | two frames, last write 1616 |  |
| claude-opus-5-5 | z80 | 1 | late, last write 16871 | two frames, last write 8743 |  |
| claude-opus-5-5 | z80 | 2 | late, last write 16871 | two frames, last write 8735 |  |
| claude-opus-5-5 | z80 | 3 | two frames, last write 8758 |  |  |
