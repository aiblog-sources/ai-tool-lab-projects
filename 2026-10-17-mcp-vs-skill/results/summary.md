
### All runs (12 questions x 2 phrasings x 2 repetitions per condition)

| Condition | Runs | Searched with the MCP tool | Searched with the Skill's script | No search | Searched with both | Invoked the Skill | Loaded the MCP schema | Other shell commands | Right answer |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| mcp | 48 | 44 | 0 | 4 | 0 | 0 | 44 | 15 | 48 |
| skill | 48 | 0 | 46 | 2 | 0 | 46 | 0 | 0 | 48 |
| both | 48 | 21 | 25 | 2 | 0 | 28 | 29 | 7 | 48 |
| both+instr | 48 | 45 | 0 | 3 | 0 | 1 | 43 | 2 | 48 |
| both-upfront | 48 | 48 | 0 | 0 | 0 | 0 | 0 | 3 | 48 |
| both-vague | 48 | 0 | 46 | 2 | 0 | 46 | 0 | 3 | 47 |

### Hinted ("Check my notes. ...")

| Condition | Runs | Searched with the MCP tool | Searched with the Skill's script | No search | Searched with both | Invoked the Skill | Loaded the MCP schema | Other shell commands | Right answer |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| mcp | 24 | 24 | 0 | 0 | 0 | 0 | 24 | 0 | 24 |
| skill | 24 | 0 | 24 | 0 | 0 | 24 | 0 | 0 | 24 |
| both | 24 | 17 | 7 | 0 | 0 | 9 | 24 | 2 | 24 |
| both+instr | 24 | 24 | 0 | 0 | 0 | 0 | 24 | 0 | 24 |
| both-upfront | 24 | 24 | 0 | 0 | 0 | 0 | 0 | 2 | 24 |
| both-vague | 24 | 0 | 24 | 0 | 0 | 24 | 0 | 0 | 24 |

### Bare question

| Condition | Runs | Searched with the MCP tool | Searched with the Skill's script | No search | Searched with both | Invoked the Skill | Loaded the MCP schema | Other shell commands | Right answer |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| mcp | 24 | 20 | 0 | 4 | 0 | 0 | 20 | 15 | 24 |
| skill | 24 | 0 | 22 | 2 | 0 | 22 | 0 | 0 | 24 |
| both | 24 | 4 | 18 | 2 | 0 | 19 | 5 | 5 | 24 |
| both+instr | 24 | 21 | 0 | 3 | 0 | 1 | 19 | 2 | 24 |
| both-upfront | 24 | 24 | 0 | 0 | 0 | 0 | 0 | 1 | 24 |
| both-vague | 24 | 0 | 22 | 2 | 0 | 22 | 0 | 3 | 23 |

### Cost of each path (runs that searched with exactly one side)

| Path | Runs | Median tool calls | Median turns | Median input tokens | Median output tokens | Median wall time (s) | Median cost (USD, API prices) | Right answer |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| mcp in mcp | 44 | 3.0 | 4.0 | 63164 | 408 | 7.8 | 0.0379 | 44/44 |
| mcp in both | 21 | 3 | 4 | 62241 | 355 | 7.6 | 0.0347 | 21/21 |
| mcp in both+instr | 45 | 3 | 4 | 62667 | 352 | 7.4 | 0.0340 | 45/45 |
| mcp in both-upfront | 48 | 2.0 | 3.0 | 91046 | 242 | 6.5 | 0.0338 | 48/48 |
| skill in skill | 46 | 3.0 | 5.0 | 60272 | 449 | 7.8 | 0.0291 | 46/46 |
| skill in both | 25 | 3 | 5 | 44041 | 504 | 7.7 | 0.0283 | 25/25 |
| skill in both-vague | 46 | 3.0 | 5.0 | 59550 | 458 | 8.0 | 0.0268 | 45/46 |

### Totals

- Runs: 288; tool errors: 23; exit codes: {0: 288}
- MCP status values: ['connected']; skill listed when expected: 240/240
- Models: ['claude-sonnet-5-5']
- Sum of reported cost: USD 9.17; sum of wall time: 36.3 min
