# Claude Code plugin Skill vs project Skill: why the plugin copy was picked less

**Finding:** on Opus 5.5 a Skill shipped in a plugin was used exactly as often as the same Skill in `.claude/skills/`
(46 of 48 runs each). On Sonnet 5.5 the plugin copy lost only on prompts that start with "Check my notes.": invoked
5 to 8 times out of 24 there against 18 for the project copy, while on plain questions all three copies were invoked
16 to 19 times. Naming the plugin like the MCP server (`notes`) cost a little more (16 script searches against 23 for
a plugin named `kb`). With both copies installed, the project Skill took every call (36 to 0). A plugin that bundles
its own MCP server sent Sonnet to that server in 39 of 48 runs. And `--strict-mcp-config` silently drops a plugin's
MCP server.

Write-up: https://aitoollabs.dev/posts/claude-code-plugin-skill-vs-project-skill/
Earlier rounds (same search, same 12 questions): [round one](../2026-10-17-mcp-vs-skill/),
[round two](../2026-10-25-skill-mcp-round-two/)

| Condition (12 questions x 2 phrasings x 2 runs) | First search: MCP | First search: the Skill's script | No search | Skill invoked | Right |
| --- | --- | --- | --- | --- | --- |
| Sonnet, MCP server + project Skill | 19 | 25 | 4 | 36 | 47 / 48 |
| Sonnet, MCP server + plugin Skill, plugin named `notes` | 27 | 16 | 5 | 21 | 47 / 48 |
| Sonnet, MCP server + plugin Skill, plugin named `kb` | 20 | 23 | 5 | 27 | 47 / 48 |
| Sonnet, MCP server + both copies installed | 8 | 36 (all project) | 4 | 36 project, 0 plugin | 48 / 48 |
| Sonnet, plugin `kb` that bundles the MCP server too | 39 | 5 | 4 | 5 | 48 / 48 |
| Sonnet, plugin `kb` + outside server, no `--strict-mcp-config` (control for the row above) | 23 | 21 | 4 | 27 | 48 / 48 |
| Opus, MCP server + project Skill | 0 | 46 | 2 | 46 | 48 / 48 |
| Opus, MCP server + plugin Skill, plugin `notes` | 0 | 46 | 2 | 46 | 48 / 48 |
| Opus, MCP server + plugin Skill, plugin `kb` | 0 | 46 | 2 | 46 | 48 / 48 |

Other numbers from the same runs (Claude Code 2.1.318, a 2-core laptop, 432 runs in about 85 minutes, USD 19.97 at API
prices as reported by Claude Code; the runs used a subscription):

- Both repetitions agreed within 2 runs on every row (`results/summary.md`).
- Sonnet by phrasing, Skill invoked out of 24: "Check my notes." prompts 18 (project), 5 (plugin `notes`), 8 (plugin
  `kb`); plain questions 18, 16, 19.
- In the init event's `skills` array the project Skill came first (position 0) and the plugin Skill after one of
  Claude Code's own skills (position 1, or 2 with both installed).
- Idle context, one-turn prompt with no tools used: nothing 17,047 input tokens, project Skill +43, plugin Skill +47
  (either name), both +90.
- The bundled server's tools are named `mcp__plugin_kb_notes__search_notes` and `mcp__plugin_kb_notes__read_note`.

## What is here

| Path | What it is |
| --- | --- |
| `notes-index.mjs`, `server.mjs` | The BM25 notes search and its MCP server (`search_notes`, `read_note`), unchanged from round one. |
| `skills/script/` | The project Skill: `SKILL.md` plus a CLI (`search.mjs`) that Claude runs through Bash, with its own copy of the index. |
| `plugins/notes/` | The same Skill in a plugin named `notes` (listed as `notes:search-notes`), as in round two. |
| `plugins/kb/` | The same plugin renamed `kb` (listed as `kb:search-notes`). |
| `plugins/kb-with-mcp/` | Plugin `kb` with the Skill and its own copy of the MCP server in `.mcp.json`. |
| `bench/run.py` | Asks `claude -p` the 12 questions under each condition in `bench/conditions.json`, records every tool call. |
| `bench/overhead.py` | Input tokens for a no-tool prompt with each Skill variant installed. |
| `bench/summarize.py` | Builds `results/summary.md` from the raw runs. |
| `bench/scrub.py` | Removes local paths, the account's other MCP connectors and the full skill list from the raw runs. |
| `results/` | Raw runs, `summary.md`, `overhead.json`. |

## Ship a Skill in a plugin

Needs Node 20 or later. A plugin is a folder with `.claude-plugin/plugin.json` and a `skills/` folder:

```bash
npm install
claude --plugin-dir "$PWD/plugins/kb"          # try it for one session
```

What the runs suggest:

- Give the plugin a name that is not the name of an MCP server that does the same job.
- Don't install the same Skill both in the project and from a plugin: the project copy wins and the plugin copy only
  costs 47 idle tokens.
- If the plugin ships an MCP server, expect Claude to use the server and skip the Skill. Put the server or the Skill in
  the plugin, not both, unless the Skill points at the server's tools.
- If you script `claude -p` with `--strict-mcp-config`, plugin MCP servers do not start. Pass them in `--mcp-config`
  yourself, or drop the flag.

## Rerun the experiment

The benchmark calls `claude -p` 432 times. The questions in `bench/questions.json` are about seven posts of the blog,
which were the corpus.

```bash
python3 bench/run.py <corpus> <scratch-folder> results/rep1.jsonl
python3 bench/run.py <corpus> <scratch-folder> results/rep2.jsonl
python3 bench/overhead.py <scratch-folder> results/overhead.json
python3 bench/summarize.py results/rep1.jsonl results/rep2.jsonl
```

`run.py` sets `CLAUDE_CODE_DISABLE_CLAUDE_MDS=1` and `CLAUDE_CODE_DISABLE_AUTO_MEMORY=1` and uses
`--setting-sources project`. Every condition allows the two MCP server names, the Skill tool and `node` in Bash, and
denies `Read`, `Grep`, `Glob` and read-only shell commands. The two conditions marked `"strict": false` run without
`--strict-mcp-config`, so the claude.ai connectors of the logged-in account load too; `scrub.py` keeps only their
count (`other_mcp_servers`).

MIT license.
