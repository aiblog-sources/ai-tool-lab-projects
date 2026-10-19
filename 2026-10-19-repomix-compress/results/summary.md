# Results (240 runs, model claude-sonnet-5-5; versions in results/env.md)

### All 30 questions

| Condition | Right | Wrong | Abstained | Runs |
| --- | --- | --- | --- | --- |
| none | 55 | 1 | 4 | 60 |
| full | 60 | 0 | 0 | 60 |
| nocomments | 58 | 0 | 2 | 60 |
| compress | 44 | 0 | 16 | 60 |

### The 17 questions whose answer text survives --compress

| Condition | Right | Wrong | Abstained | Runs |
| --- | --- | --- | --- | --- |
| none | 33 | 0 | 1 | 34 |
| full | 34 | 0 | 0 | 34 |
| nocomments | 32 | 0 | 2 | 34 |
| compress | 34 | 0 | 0 | 34 |

### The 13 questions whose answer text --compress removes

| Condition | Right | Wrong | Abstained | Runs |
| --- | --- | --- | --- | --- |
| none | 22 | 1 | 3 | 26 |
| full | 26 | 0 | 0 | 26 |
| nocomments | 26 | 0 | 0 | 26 |
| compress | 10 | 0 | 16 | 26 |

### ky only

| Condition | Right | Wrong | Abstained | Runs |
| --- | --- | --- | --- | --- |
| none | 15 | 1 | 4 | 20 |
| full | 20 | 0 | 0 | 20 |
| nocomments | 18 | 0 | 2 | 20 |
| compress | 14 | 0 | 6 | 20 |

### requests only

| Condition | Right | Wrong | Abstained | Runs |
| --- | --- | --- | --- | --- |
| none | 20 | 0 | 0 | 20 |
| full | 20 | 0 | 0 | 20 |
| nocomments | 20 | 0 | 0 | 20 |
| compress | 16 | 0 | 4 | 20 |

### cobra only

| Condition | Right | Wrong | Abstained | Runs |
| --- | --- | --- | --- | --- |
| none | 20 | 0 | 0 | 20 |
| full | 20 | 0 | 0 | 20 |
| nocomments | 20 | 0 | 0 | 20 |
| compress | 14 | 0 | 6 | 20 |

### Input tokens per run (Claude's count, median over the repo's runs)

| Repo | none | full | nocomments | compress | compress vs full |
| --- | --- | --- | --- | --- | --- |
| ky | 651 | 78,282 | 45,177 | 69,082 | 0.88 |
| requests | 647 | 81,097 | 59,232 | 50,985 | 0.63 |
| cobra | 642 | 89,558 | 72,393 | 34,523 | 0.39 |

### Cost reported by claude -p (API prices; the runs used a subscription)

| Condition | Total USD | Cache read share of input |
| --- | --- | --- |
| none | 0.20 | 50% |
| full | 9.63 | 53% |
| nocomments | 7.29 | 50% |
| compress | 6.40 | 50% |

Total: USD 23.52

### Per question (right out of 2 runs)

| Question | Answer text survives --compress | none | full | nocomments | compress |
| --- | --- | --- | --- | --- | --- |
| ky-retry-limit | yes | 2 | 2 | 2 | 2 |
| ky-retry-status | yes | 2 | 2 | 2 | 2 |
| ky-limit-error | no | 0 (2 abst.) | 2 | 2 | 0 (2 abst.) |
| ky-max-timeout | no | 2 | 2 | 2 | 2 |
| ky-symfony-header | yes | 1 (1 abst.) | 2 | 0 (2 abst.) | 2 |
| ky-epoch-threshold | no | 0 (1 abst.) | 2 | 2 | 2 |
| ky-text-accept | no | 2 | 2 | 2 | 0 (2 abst.) |
| ky-boundary-size | no | 2 | 2 | 2 | 2 |
| ky-stream-error | no | 2 | 2 | 2 | 0 (2 abst.) |
| ky-prefixurl | no | 2 | 2 | 2 | 2 |
| rq-poolsize | yes | 2 | 2 | 2 | 2 |
| rq-redirect-limit | yes | 2 | 2 | 2 | 2 |
| rq-iter-chunk | yes | 2 | 2 | 2 | 2 |
| rq-content-chunk | yes | 2 | 2 | 2 | 2 |
| rq-digest-401 | no | 2 | 2 | 2 | 0 (2 abst.) |
| rq-too-many | no | 2 | 2 | 2 | 0 (2 abst.) |
| rq-http-https | no | 2 | 2 | 2 | 2 |
| rq-netrc | yes | 2 | 2 | 2 | 2 |
| rq-ua-name | yes | 2 | 2 | 2 | 2 |
| rq-allow-redirects | yes | 2 | 2 | 2 | 2 |
| cb-complete-cmd | yes | 2 | 2 | 2 | 2 |
| cb-suggest-dist | no | 2 | 2 | 2 | 0 (2 abst.) |
| cb-usage-pad | yes | 2 | 2 | 2 | 2 |
| cb-help-short | no | 2 | 2 | 2 | 0 (2 abst.) |
| cb-unknown-cmd | no | 2 | 2 | 2 | 0 (2 abst.) |
| cb-mousetrap | yes | 2 | 2 | 2 | 2 |
| cb-group-annotation | yes | 2 | 2 | 2 | 2 |
| cb-env-prefix | yes | 2 | 2 | 2 | 2 |
| cb-desc-env | yes | 2 | 2 | 2 | 2 |
| cb-version-short | yes | 2 | 2 | 2 | 2 |
