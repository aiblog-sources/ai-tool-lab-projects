| Condition | Questions | Runs | Runs that got lockfile text | Denied calls | Lockfile chars (median run) | Correct | Input tokens (median) | Cost USD (total) |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| none | lockfile | 12 | 12 | 2 | 1,106 | 12/12 | 48,963 | 0.48 |
| none | code | 6 | 3 | 0 | 83 | 6/6 | 49,499 | 0.18 |
| claudeignore | lockfile | 12 | 12 | 0 | 1,042 | 12/12 | 48,967 | 0.41 |
| claudeignore | code | 6 | 3 | 0 | 83 | 6/6 | 49,329 | 0.18 |
| dotignore | lockfile | 12 | 12 | 0 | 979 | 12/12 | 55,628 | 0.35 |
| dotignore | code | 6 | 0 | 0 | 0 | 6/6 | 49,517 | 0.18 |
| deny | lockfile | 12 | 0 | 14 | 0 | 0/12 | 69,479 | 0.43 |
| deny | code | 6 | 0 | 0 | 0 | 6/6 | 49,473 | 0.18 |
| instruction | lockfile | 12 | 1 | 0 | 0 | 1/12 | 42,439 | 0.40 |
| instruction | code | 6 | 2 | 0 | 0 | 6/6 | 49,621 | 0.18 |
| deny_npmls | lockfile | 12 | 0 | 0 | 0 | 12/12 | 27,349 | 0.24 |
| deny_npmls | code | 6 | 0 | 0 | 0 | 6/6 | 49,741 | 0.19 |
| deny_npmls_untrusted | lockfile | 8 | 0 | 0 | 0 | 0/8 | 41,260 | 0.22 |
| deny_npmls_untrusted | code | 4 | 0 | 0 | 0 | 4/4 | 49,868 | 0.12 |
