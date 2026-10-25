# Can a Claude Code Skill call an MCP tool? Skill vs MCP, round two

**Finding:** a Skill whose instructions say "search with `mcp__notes__search_notes`" got Claude Code to use the MCP
tool in 133 of 144 runs across Haiku 5.5, Sonnet 5.5 and Opus 5.5, and it rescued a vaguely named server (`s1` /
`query`) from 25 to 44 of 48 searches. When the Skill instead wraps its own script, Opus picked the script in 46 of 48
runs and the MCP tool in none, while Sonnet and Haiku split about evenly. The same Skill shipped in a plugin was picked
less than half as often as the project-level copy (10 vs 24 of 48).

Write-up: https://aitoollabs.dev/posts/claude-code-skill-call-mcp-tool/
Round one (same search, same questions): [2026-10-17-mcp-vs-skill](../2026-10-17-mcp-vs-skill/)

| Condition (12 questions x 2 phrasings x 2 runs) | Searched with MCP | Searched with the Skill's script | No search | Right answer |
| --- | --- | --- | --- | --- |
| Sonnet, MCP server + script Skill | 20 | 24 | 4 | 48 / 48 |
| Haiku, MCP server + script Skill | 23 | 22 | 3 | 45 / 48 |
| Opus, MCP server + script Skill | 0 | 46 | 2 | 48 / 48 |
| Sonnet, MCP server + bridge Skill (points at the MCP tool) | 44 | n/a | 4 | 47 / 48 |
| Haiku, MCP server + bridge Skill | 43 | n/a | 5 | 41 / 48 |
| Opus, MCP server + bridge Skill | 46 | n/a | 2 | 48 / 48 |
| Sonnet, vague MCP server only (`s1` / `query`) | 25 | n/a | 23 | 31 / 48 |
| Sonnet, vague MCP server + bridge Skill naming `mcp__s1__query` | 44 | n/a | 4 | 48 / 48 |
| Sonnet, MCP server + the script Skill from a plugin | 34 | 10 | 4 | 48 / 48 |

Other numbers from the same runs (Claude Code 2.1.310, a 2-core laptop, 432 runs in about 70 minutes):

- Both repetitions agreed within 2 runs on every row (`results/summary.md`).
- Every MCP call in the bridge conditions came after Claude loaded the tool's schema with ToolSearch. No call failed.
- Opus never called ToolSearch when the script Skill was installed: it invoked the Skill in 46 of 48 runs, including all
  24 that started with "Check my notes."
- Per answer with the script Skill installed (mean cost at API prices as reported by Claude Code, the runs used a
  subscription; median wall time): Sonnet USD 0.033 and 9.1 s, Opus USD 0.073 and 15.7 s, Haiku USD 0.002 and 10.2 s. The bridge Skill costs one extra turn (the Skill call).
- Idle context, one-turn prompt with no tools used: nothing 16,367 input tokens, project Skill +43, bridge Skill +45,
  plugin Skill +47.
- Read-only file browsing was denied in every condition (see below). Haiku tried it anyway in 30 of its 96 runs, and
  Sonnet with only the vague server in 44 of 48.

## What is here

| Path | What it is |
| --- | --- |
| `notes-index.mjs`, `server.mjs` | The BM25 notes search and its MCP server (`search_notes`, `read_note`), unchanged from round one. |
| `skills/script/` | The round-one Skill: instructions plus a CLI (`search.mjs`) that Claude runs through Bash. |
| `skills/bridge/SKILL.md` | Same name and description, but the body tells Claude to use `mcp__notes__search_notes` and `mcp__notes__read_note`. No script. |
| `skills/bridge-vague/SKILL.md` | The bridge Skill pointing at the vague server's `mcp__s1__query` and `mcp__s1__get`. |
| `plugin/` | A plugin named `notes` that ships the script Skill (loaded with `--plugin-dir`, listed as `notes:search-notes`). |
| `bench/run.py` | Asks `claude -p` the 12 questions under each condition in `bench/conditions.json`, records every tool call. |
| `bench/overhead.py` | Input tokens for a no-tool prompt with each Skill variant installed. |
| `bench/summarize.py` | Builds `results/summary.md` from the raw runs. |
| `results/` | Raw runs (local paths replaced with `<work>`, `<cache>`, `<repo>` and `~`), `summary.md`, `overhead.json`. |

## Use the bridge pattern

Needs Node 20 or later.

```bash
npm install
claude mcp add notes -- node "$PWD/server.mjs" /path/to/your/markdown/folder
mkdir -p .claude/skills && cp -r skills/bridge .claude/skills/search-notes
```

The Skill only holds instructions; the MCP server does the work. Name the exact `mcp__<server>__<tool>` tools in the
body. Claude still loads their schemas through tool search first, so nothing else is needed.

## Rerun the experiment

The benchmark calls `claude -p` 432 times (Claude Code reported USD 14.88 at API prices). The questions in
`bench/questions.json` are about seven posts of the blog, which were the corpus.

```bash
python3 bench/run.py <corpus> <scratch-folder> results/rep1.jsonl
python3 bench/run.py <corpus> <scratch-folder> results/rep2.jsonl
python3 bench/overhead.py <scratch-folder> results/overhead.json
python3 bench/summarize.py results/rep1.jsonl results/rep2.jsonl
```

`run.py` sets `CLAUDE_CODE_DISABLE_CLAUDE_MDS=1` and `CLAUDE_CODE_DISABLE_AUTO_MEMORY=1` and uses
`--setting-sources project`. Every condition allows both MCP server names, the Skill tool and `node` in Bash, and
denies `Read`, `Grep`, `Glob` and read-only shell commands (`grep`, `find`, `ls`, `cat` and others). Round one left
those on; in this round's smoke test a run with the vague server grepped the whole home folder looking for notes, so
they are off here. If you turn them back on, keep the corpus where a grep from a parent folder cannot reach it.

License: MIT.
