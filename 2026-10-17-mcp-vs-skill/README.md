# MCP server or Skill: which one Claude Code uses when both do the same job

**Finding:** I gave Claude Code the same notes search twice, as an MCP server and as a Skill, and asked 12 questions
288 times. With both installed, it searched with the MCP tool in 21 of 48 runs and with the Skill in 25. Which one won
depended on the prompt ("Check my notes." went 17 to 7 for MCP, plain questions 18 to 4 for the Skill) and on what the
MCP server shows before it is loaded: a vague server name lost every run to the Skill (0 of 48), and two sentences of
server instructions won every run (45 of 48, the other 3 did not search).

Write-up: https://aitoollabs.dev/posts/claude-code-skill-vs-mcp/

| Installed (12 questions x 2 phrasings x 2 runs) | Searched with MCP | Searched with the Skill | No search | Right answer |
| --- | --- | --- | --- | --- |
| MCP server only | 44 | 0 | 4 | 48 / 48 |
| Skill only | 0 | 46 | 2 | 48 / 48 |
| Both | 21 | 25 | 2 | 48 / 48 |
| Both, server instructions added | 45 | 0 | 3 | 48 / 48 |
| Both, tool search off (`ENABLE_TOOL_SEARCH=false`) | 48 | 0 | 0 | 48 / 48 |
| Both, vague MCP names (`s1` / `query`) | 0 | 46 | 2 | 47 / 48 |

| Both installed, by phrasing | Searched with MCP | Searched with the Skill | No search |
| --- | --- | --- | --- |
| "Check my notes. ..." | 17 / 24 | 7 / 24 | 0 / 24 |
| Plain question | 4 / 24 | 18 / 24 | 2 / 24 |

Other numbers from the same runs (Claude Code 2.1.302, Claude Sonnet 5.5, a 2-core laptop):

- No run searched with both. In 11 of the 48 runs with both installed, Claude loaded both (invoked the Skill and
  fetched the MCP schema) and then searched with one of them.
- Either path answered correctly. Median per answer: MCP 4 turns and 7.4 to 7.8 s, Skill 5 turns and 7.7 to 8.0 s.
  The Skill path costs one more turn (the Skill tool call itself) and the answers were the same.
- Context before anything is used: the Skill's listing added 44 input tokens, the deferred MCP server 22 (two tool
  names). With tool search off the MCP server added 348 tokens, and Claude Code's own deferred tools about 14,600.
- The answers in "No search" runs came from the model's own knowledge (Ollama's default context and Hermes Agent's
  minimum context, both public). They matched the notes, so they count as right.

## What is here

| Path | What it is |
| --- | --- |
| `notes-index.mjs` | The search: BM25 over every `.md` file in a folder, with snippets. Both sides import it. |
| `server.mjs` | The MCP side: `search_notes` and `read_note` (same server as [2026-10-15](../2026-10-15-mcp-server-under-100-lines/)). |
| `skill/search-notes/SKILL.md`, `search.mjs` | The Skill side: instructions plus a CLI that Claude runs through Bash. |
| `bench/run.py` | Asks `claude -p` 12 questions under 6 conditions and 2 phrasings, records every tool call. |
| `bench/conditions.json`, `bench/questions.json` | What is installed in each condition, and the questions with expected answers. |
| `bench/overhead.py` | Input tokens for a prompt that needs no tools, with each side installed. |
| `bench/summarize.py` | Builds the tables above from `results/`. |
| `results/` | Raw runs (local paths replaced with `~` and `<work>`), `summary.md` and `overhead.json`. |

## Use the Skill or the server

Needs Node 20 or later.

```bash
npm install
# MCP server
claude mcp add notes -- node "$PWD/server.mjs" /path/to/your/markdown/folder
# Skill (project level): copy or link the folder and tell the script where the notes are
mkdir -p .claude/skills && ln -s "$PWD/skill/search-notes" .claude/skills/search-notes
export NOTES_DIR=/path/to/your/markdown/folder
```

The Skill's script imports `../../notes-index.mjs`, so link the folder (Node follows the link) rather than copying it,
or copy `notes-index.mjs` next to it and change the import.

## Rerun the experiment

The benchmark calls `claude -p` 288 times (36 minutes of runs; Claude Code reported USD 9.17 at API prices, it ran on
a subscription). The questions in `bench/questions.json` are about seven posts of the blog, which were the corpus.

```bash
python3 bench/run.py <corpus> <scratch-folder> results/rep1.jsonl sonnet
python3 bench/run.py <corpus> <scratch-folder> results/rep2.jsonl sonnet
python3 bench/overhead.py <scratch-folder> results/overhead.json sonnet
python3 bench/summarize.py results/rep1.jsonl results/rep2.jsonl
```

`run.py` sets `CLAUDE_CODE_DISABLE_CLAUDE_MDS=1` and `CLAUDE_CODE_DISABLE_AUTO_MEMORY=1` and uses
`--setting-sources project`, so personal CLAUDE.md files, memory and user-level Skills stay out of the runs. Every
condition gets the same permissions: the MCP server, the Skill tool and `node` in Bash. The vague-name condition was
added after the first repetition had started, so its first repetition ran after the others instead of interleaved.

License: MIT.
