| Condition | Runs | Explore used | Cost (total) | Cost per run (median) | Haiku share | Main context at end (median) | Main input, all turns (median) | Subagent input (median) | Wall s (median) | Points | Wrong claims |
|---|---|---|---|---|---|---|---|---|---|---|---|
| no-subagent | 30 | 0 | $0.91 | $0.031 | 0% | 17,973 | 48,180 | 0 | 8 | 130/150 | 2 |
| default | 30 | 0 | $1.03 | $0.034 | 0% | 20,446 | 57,500 | 0 | 8 | 128/150 | 2 |
| explore | 30 | 30 | $2.81 | $0.091 | 0% | 20,380 | 56,144 | 47,390 | 23 | 135/150 | 0 |
| explore-haiku | 30 | 30 | $1.13 | $0.035 | 15% | 20,118 | 55,452 | 92,033 | 32 | 134/150 | 0 |

Per repo, median cost and wall time:
- ky: no-subagent $0.033 8s; default $0.036 8s; explore $0.100 24s; explore-haiku $0.035 35s
- requests: no-subagent $0.019 6s; default $0.026 7s; explore $0.078 21s; explore-haiku $0.037 27s
- diff-tour: no-subagent $0.039 8s; default $0.044 8s; explore $0.096 24s; explore-haiku $0.041 34s

Agent calls by type: {'Explore': 60}
Subagent models: ['claude-haiku-5-5', 'claude-sonnet-5-5']
Runs with 2+ result events: {'no-subagent': 0, 'default': 0, 'explore': 29, 'explore-haiku': 28}
Claude Code versions: ['2.1.325']
Tool calls (median main / sub): {'no-subagent': (2.0, 0.0), 'default': (2.0, 0.0), 'explore': (1.0, 3.0), 'explore-haiku': (1.0, 8.0)}
