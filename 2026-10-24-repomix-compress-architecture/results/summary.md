## Checklist points by repo and condition (5 questions x 5 points x 2 runs)

| Condition | ky (TypeScript) | requests (Python) | diff-tour (Go, new) | All | Wrong claims |
| --- | --- | --- | --- | --- | --- |
| No repo (model memory) | 34/50 | 49/50 | 0/50 | 83/150 | 17 |
| Full pack | 48/50 | 49/50 | 42/50 | 139/150 | 0 |
| `--compress` | 41/50 | 35/50 | 36/50 | 112/150 | 1 |
| `--compress`, private methods fixed | 45/50 |  |  | 45/50 | 0 |

## Per question (points out of 10: 5 points x 2 runs)

| Question | No repo (model memory) | Full pack | `--compress` | `--compress`, private methods fixed |
| --- | --- | --- | --- | --- |
| ky-retry-flow | 7 | 10 | 8 | 10 |
| ky-timeout | 7 | 10 | 8 | 9 |
| ky-hooks-order | 4 | 9 | 8 | 9 |
| ky-json-shortcut | 8 | 9 | 7 | 8 |
| ky-force-retry | 8 | 10 | 10 | 9 |
| requests-get-flow | 10 | 10 | 10 |  |
| requests-redirects | 10 | 10 | 8 |  |
| requests-adapters | 10 | 10 | 5 |  |
| requests-env-settings | 9 | 9 | 4 |  |
| requests-hooks-cookies | 10 | 10 | 8 |  |
| diff-tour-flow | 0 | 8 | 10 |  |
| diff-tour-invalid | 0 | 8 | 9 |  |
| diff-tour-sandbox | 0 | 8 | 8 |  |
| diff-tour-cache | 0 | 10 | 6 |  |
| diff-tour-ask | 0 | 8 | 3 |  |

## Answers that say part of the answer is missing, "NOT SURE", tokens, time and cost

| Condition | Runs | Says something is missing | NOT SURE | Median input tokens (ky / requests / diff-tour) | Median wall time | Cost at API prices |
| --- | --- | --- | --- | --- | --- | --- |
| No repo (model memory) | 30 | - | 10 | 678 / 657 / 684 | 8.3 s | $0.18 |
| Full pack | 30 | 14 | 0 | 78,307 / 81,105 / 101,911 | 9.7 s | $5.48 |
| `--compress` | 30 | 30 | 0 | 69,107 / 50,993 / 51,060 | 10.5 s | $3.68 |
| `--compress`, private methods fixed | 10 | 10 | 0 | 69,898 / - / - | 10.1 s | $1.50 |

Total: 100 runs, $10.84 at API prices, model claude-sonnet-5-5, Claude Code [None], first run 2026-10-24T03:10:41, last 2026-10-24T03:18:38.

## Wrong claims, by hand

- ky-hooks-order, No repo (model memory), run 1: 2 (beforeRequest runs inside fetch on every attempt; beforeRetry hooks run before the wait)
- ky-hooks-order, No repo (model memory), run 2: 2 (beforeRequest runs inside fetch on every attempt; beforeError runs on each HTTPError before the retry decision)
- ky-retry-flow, `--compress`, run 2: 1 (says the request is cloned after the fetch (it is cloned before))
- ky-retry-flow, No repo (model memory), run 1: 4 (old _retry wrapper; beforeRequest inside _fetch on every attempt; beforeError hooks before each retry; retryCount incremented in calculateRetryDelay)
- ky-retry-flow, No repo (model memory), run 2: 3 (old _retry(function_) wrapper; beforeError hooks before each retry; retryCount incremented in calculateRetryDelay)
- ky-timeout, No repo (model memory), run 2: 1 (says a TimeoutError is retried depending on retry.limit (not retried unless retryOnTimeout))
- requests-env-settings, No repo (model memory), run 1: 2 (names get_environment_proxies as current; says session proxies beat environment proxies (env values are merged into the request-level dict first))
- requests-env-settings, No repo (model memory), run 2: 2 (names get_environment_proxies (the function is get_environ_proxies); says session settings beat environment variables)
- requests-redirects, No repo (model memory), run 2: 1 (says redirects strip fragments (requests carries the previous fragment over))
