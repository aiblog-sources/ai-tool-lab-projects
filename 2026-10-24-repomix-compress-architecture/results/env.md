# Environment of the run (2026-10-24)

- Repomix 1.18.1 (npm, latest on the day), Node 22.14
- Claude Code 2.1.310, `claude -p --model claude-sonnet-5-5`, tools off, system prompt replaced, CLAUDE.md and auto-memory disabled
- 90 runs (3 conditions) between 03:10 and 03:17 local time, then 10 runs of the patched ky pack at 03:17-03:18, 2 at a time
- Repos (source folders only):
  - ky 3541888 (2026-10-09), `source/**`
  - requests 611c616 (2026-09-21), `src/**`
  - diff-tour 268ffd0 (2026-10-24; repository created 2026-10-10, after the model's training data), `**/*.go` without `**/*_test.go`
- Grading by hand, blind to the condition (see `blind.py`); one grader (the agent that ran the benchmark)
- Laptop with a 2-core, 4-thread CPU; the model runs remotely, so the CPU does not affect results
