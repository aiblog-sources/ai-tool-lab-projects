240 runs. Models: ['claude-haiku-5-5', 'claude-sonnet-5-5'].

## Bash allowed (npx, npm, node, tsc, grep...)

| Model | Condition | Runs | Runs with tsc errors left | Errors left (total) | Hidden tests all pass | Claude tried to run tsc (median; denied without a shell) | Hook runs (median) | Hook reports with errors (median) | Hook time per run (median, s) | Turns (median) | Wall time (median, s) | Cost per run (mean, USD) |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| haiku | none | 12 | 0 | 0 | 12/12 | 6 | 0 | 0 | 0.0 | 40.5 | 58 | 0.0295 |
| haiku | claude-md | 12 | 0 | 0 | 12/12 | 7 | 0 | 0 | 0.0 | 37.5 | 57 | 0.0190 |
| haiku | post-exit1 | 12 | 0 | 0 | 12/12 | 6.5 | 17 | 16 | 7.6 | 48 | 59 | 0.0176 |
| haiku | post-exit2 | 12 | 0 | 0 | 12/12 | 8.5 | 19 | 18 | 7.9 | 35.5 | 64 | 0.0243 |
| haiku | stop-exit2 | 12 | 0 | 0 | 12/12 | 7 | 1 | 0 | 0.3 | 45 | 56 | 0.0191 |
| sonnet | none | 12 | 0 | 0 | 12/12 | 4.5 | 0 | 0 | 0.0 | 22 | 39 | 0.1840 |
| sonnet | claude-md | 12 | 0 | 0 | 12/12 | 5.5 | 0 | 0 | 0.0 | 40.5 | 46 | 0.1850 |
| sonnet | post-exit1 | 12 | 0 | 0 | 12/12 | 5 | 10 | 8.5 | 5.2 | 29.5 | 50 | 0.1845 |
| sonnet | post-exit2 | 12 | 0 | 0 | 12/12 | 6.5 | 10.5 | 9.5 | 4.9 | 30 | 51 | 0.2336 |
| sonnet | stop-exit2 | 12 | 0 | 0 | 12/12 | 5 | 1 | 0 | 0.5 | 25.5 | 49 | 0.1773 |

## No shell (file tools only)

| Model | Condition | Runs | Runs with tsc errors left | Errors left (total) | Hidden tests all pass | Claude tried to run tsc (median; denied without a shell) | Hook runs (median) | Hook reports with errors (median) | Hook time per run (median, s) | Turns (median) | Wall time (median, s) | Cost per run (mean, USD) |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| haiku | none | 12 | 0 | 0 | 12/12 | 1 | 0 | 0 | 0.0 | 47 | 47 | 0.0152 |
| haiku | claude-md | 12 | 0 | 0 | 12/12 | 1 | 0 | 0 | 0.0 | 56.5 | 55 | 0.0161 |
| haiku | post-exit1 | 12 | 0 | 0 | 12/12 | 1 | 24.5 | 23.5 | 11.9 | 53.5 | 56 | 0.0177 |
| haiku | post-exit2 | 12 | 0 | 0 | 12/12 | 1 | 28 | 26.5 | 12.3 | 54.5 | 63 | 0.0216 |
| haiku | stop-exit2 | 12 | 0 | 0 | 12/12 | 1 | 1 | 0 | 0.5 | 46 | 54 | 0.0149 |
| sonnet | none | 12 | 1 | 28 | 12/12 | 0 | 0 | 0 | 0.0 | 42.5 | 43 | 0.1780 |
| sonnet | claude-md | 12 | 0 | 0 | 12/12 | 0 | 0 | 0 | 0.0 | 43.5 | 51 | 0.1743 |
| sonnet | post-exit1 | 12 | 0 | 0 | 12/12 | 0 | 25.5 | 24.5 | 11.5 | 49.5 | 53 | 0.1881 |
| sonnet | post-exit2 | 12 | 0 | 0 | 12/12 | 0 | 24 | 23 | 11.7 | 42.5 | 51 | 0.2925 |
| sonnet | stop-exit2 | 12 | 0 | 0 | 12/12 | 0 | 1 | 0 | 0.4 | 41 | 44 | 0.1789 |

## Per task: runs with tsc errors left / runs with a hidden test failing

| Task | none | claude-md | post-exit1 | post-exit2 | stop-exit2 |
| --- | --- | --- | --- | --- | --- |
| refunded | 0 / 0 of 8 | 0 / 0 of 8 | 0 / 0 of 8 | 0 / 0 of 8 | 0 / 0 of 8 |
| cents | 0 / 0 of 8 | 0 / 0 of 8 | 0 / 0 of 8 | 0 / 0 of 8 | 0 / 0 of 8 |
| find-undefined | 0 / 0 of 8 | 0 / 0 of 8 | 0 / 0 of 8 | 0 / 0 of 8 | 0 / 0 of 8 |
| async-store | 0 / 0 of 8 | 0 / 0 of 8 | 0 / 0 of 8 | 0 / 0 of 8 | 0 / 0 of 8 |
| currency | 1 / 0 of 8 | 0 / 0 of 8 | 0 / 0 of 8 | 0 / 0 of 8 | 0 / 0 of 8 |
| name-split | 0 / 0 of 8 | 0 / 0 of 8 | 0 / 0 of 8 | 0 / 0 of 8 | 0 / 0 of 8 |

## Hook output as Claude Code recorded it (transcript attachment types, summed)

- post-exit1: `PostToolUse:hook_non_blocking_error:silent` x1078
- post-exit2: `PostToolUse:hook_blocking_error:rendered` x985

## Runs that left tsc errors or failed a hidden test

- currency / sonnet / noshell / none / rep 1: 28 tsc errors, tests 4/4, subtype success; first error: `src/admin/customerOrders.ts(21,69): error TS2554: Expected 2 arguments, but got 1.`

## Combo task: all six changes in one prompt, no shell (graded on tsc only)

| Model | Condition | Runs | Runs with tsc errors left | Errors left per run | Hook runs (median) | Hook reports with errors (median) | Turns (median) | Wall time (median, s) | Cost per run (mean, USD) | Result subtypes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| haiku | none | 3 | 0 | 0, 0, 0 | 0 | 0 | 120 | 237 | 0.1046 | success |
| haiku | claude-md | 3 | 1 | 0, 148, 0 | 0 | 0 | 127 | 208 | 0.0601 | success |
| haiku | post-exit1 | 3 | 1 | 0, 0, 8 | 81 | 80 | 144 | 261 | 0.1125 | success |
| haiku | post-exit2 | 3 | 0 | 0, 0, 0 | 55 | 54 | 120 | 252 | 0.2566 | success |
| haiku | stop-exit2 | 3 | 0 | 0, 0, 0 | 1 | 0 | 122 | 240 | 0.1028 | success |
| sonnet | none | 3 | 3 | 148, 148, 148 | 0 | 0 | 28 | 46 | 0.1736 | success |
| sonnet | claude-md | 3 | 3 | 148, 148, 148 | 0 | 0 | 26 | 43 | 0.1573 | success |
| sonnet | post-exit1 | 3 | 3 | 148, 148, 148 | 12 | 12 | 29 | 49 | 0.1656 | success |
| sonnet | post-exit2 | 3 | 0 | 0, 0, 0 | 63 | 61 | 122 | 172 | 1.0421 | success |
| sonnet | stop-exit2 | 3 | 0 | 0, 0, 0 | 2 | 1 | 119 | 163 | 0.6326 | success |

## Combo task: all six changes in one prompt, Bash allowed (graded on tsc only)

| Model | Condition | Runs | Runs with tsc errors left | Errors left per run | Hook runs (median) | Hook reports with errors (median) | Turns (median) | Wall time (median, s) | Cost per run (mean, USD) | Result subtypes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| haiku | none | 3 | 0 | 0, 0, 0 | 0 | 0 | 76 | 259 | 0.1814 | success |
| haiku | claude-md | 3 | 0 | 0, 0, 0 | 0 | 0 | 109 | 243 | 0.1305 | success |
| haiku | post-exit1 | 3 | 0 | 0, 0, 0 | 11 | 11 | 78 | 226 | 0.1043 | success |
| haiku | post-exit2 | 3 | 0 | 0, 0, 0 | 56 | 55 | 96 | 277 | 0.3429 | success |
| haiku | stop-exit2 | 3 | 0 | 0, 0, 0 | 1 | 0 | 122 | 275 | 0.2799 | success |
| sonnet | none | 3 | 0 | 0, 0, 0 | 0 | 0 | 52 | 144 | 0.5926 | success |
| sonnet | claude-md | 3 | 0 | 0, 0, 0 | 0 | 0 | 50 | 123 | 0.4998 | success |
| sonnet | post-exit1 | 3 | 1 | 0, 0, 66 | 6 | 6 | 26 | 96 | 0.3636 | success |
| sonnet | post-exit2 | 3 | 0 | 0, 0, 0 | 9 | 9 | 34 | 127 | 0.6499 | success |
| sonnet | stop-exit2 | 3 | 0 | 0, 0, 0 | 1 | 0 | 153 | 186 | 0.6874 | success |
