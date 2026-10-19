| Repo | Mode | Characters | o200k tokens | vs full |
| --- | --- | --- | --- | --- |
| ky | full | 198,581 | 47,852 | 1.00 |
| ky | nocomments | 105,757 | 26,061 | 0.54 |
| ky | compress | 182,209 | 43,676 | 0.91 |
| requests | full | 219,348 | 50,280 | 1.00 |
| requests | nocomments | 155,543 | 36,194 | 0.72 |
| requests | compress | 122,227 | 31,102 | 0.62 |
| cobra | full | 210,427 | 53,690 | 1.00 |
| cobra | nocomments | 158,415 | 42,617 | 0.79 |
| cobra | compress | 88,122 | 20,712 | 0.39 |

Files that --compress made larger, and text repeated inside compressed files:

| Repo | File | Full tokens | Compress tokens | Ratio | Repeated chunk tokens |
| --- | --- | --- | --- | --- | --- |
| ky | source/core/constants.ts | 2,586 | 2,555 | 0.99 | 594 |
| ky | source/types/common.ts | 63 | 66 | 1.05 | 0 |
| ky | source/types/hooks.ts | 4,117 | 7,397 | 1.80 | 3,201 |
| ky | source/types/ky.ts | 1,839 | 3,485 | 1.90 | 1,595 |
| ky | source/types/options.ts | 6,107 | 11,221 | 1.84 | 4,607 |
| ky | source/types/ResponsePromise.ts | 673 | 982 | 1.46 | 239 |
| ky | source/types/retry.ts | 1,460 | 2,757 | 1.89 | 1,240 |
| ky | source/types/standard-schema.ts | 386 | 407 | 1.05 | 0 |
| requests | src/requests/__version__.py | 173 | 176 | 1.02 | 0 |
| cobra | command_notwin.go | 149 | 165 | 1.11 | 0 |

ky: 11,584 of 42,670 compressed tokens are repeated chunks (27%)
requests: 74 of 30,293 compressed tokens are repeated chunks (0%)
cobra: 121 of 20,023 compressed tokens are repeated chunks (1%)

Function definitions kept by --compress:

requests: 186 of 268 kept; signatures over several lines: 5 of 70 kept (matched by name, so a same-named definition elsewhere in the file counts as kept)
cobra: 240 of 240 kept; signatures over several lines: 0 of 0 kept (matched by name, so a same-named definition elsewhere in the file counts as kept)
