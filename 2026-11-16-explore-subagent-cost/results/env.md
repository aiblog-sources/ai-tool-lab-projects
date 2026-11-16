# Environment of the run (2026-11-16)

- Claude Code 2.1.325, `claude -p --model claude-sonnet-5-5`, stream-json output, no MCP servers
  (`--strict-mcp-config`), no user or project settings (`--setting-sources ""`), CLAUDE.md and auto-memory disabled.
  Built-in agents listed in the init event: claude, Explore, general-purpose, Plan, statusline-setup, web-fetch.
- Main set: 120 runs between 03:06 and 03:18 local time, 3 at a time. Broad set: 36 runs between 03:13 and 03:16.
- Costs are Claude Code's `total_cost_usd` and per-model `costUSD` (API list prices); the runs used a subscription.
- Repos: ky 3541888 (2026-10-09), requests 611c616 (2026-09-21), diff-tour 268ffd0 (2026-10-24), django f24f38c
  (2026-11-13, shallow clone). ky has an AGENTS.md, which Claude Code does not read.
- Subagent model for `explore-haiku`: `CLAUDE_CODE_SUBAGENT_MODEL=claude-haiku-5-5` and
  `CLAUDE_CODE_SUBAGENT_MODEL_FORCE=1`. The variable alone left Explore on Sonnet in a probe run.
- Grading by hand, blind to the condition label (`blind.py`), one grader (the agent that ran the benchmark).
- Laptop with a 2-core, 4-thread CPU; the model runs remotely, so the CPU does not affect results.
