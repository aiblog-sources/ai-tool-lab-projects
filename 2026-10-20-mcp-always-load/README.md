# MCP alwaysLoad vs tool search off in Claude Code

**Finding:** marking one MCP tool with `_meta: {"anthropic/alwaysLoad": true}` made Claude Code call it in 96 of 96
runs, the same as turning tool search off (`ENABLE_TOOL_SEARCH=false`), for 238 tokens of context instead of about
12,000. With the default deferred loading, a vaguely named tool was called in only 30 of 48 runs.

Write-up: https://aitoollabs.dev/posts/claude-code-mcp-alwaysload/

| Loading mode (12 questions x 2 phrasings x 2 runs) | Clear names: called the tool | Vague names: called the tool | Context added |
| --- | --- | --- | --- |
| Deferred (default tool search) | 44 / 48 | 30 / 48 | +23 tokens |
| Per-tool `_meta["anthropic/alwaysLoad"]` on the search tool | 48 / 48 | 48 / 48 | +238 tokens |
| Per-server `"alwaysLoad": true` in the MCP config | 48 / 48 | not run | +351 tokens |
| Tool search off (`ENABLE_TOOL_SEARCH=false`) | 48 / 48 | 48 / 48 | +12,016 tokens |

"Context added" is the input tokens of a one-turn prompt compared with no MCP server at all (9,704 tokens), identical
in 3 of 3 launches each. Turning tool search off loads 11,665 tokens of Claude Code's own deferred tools on top of the
351 tokens of this server's two tools.

Other results from the same runs (Claude Code 2.1.304, Claude Sonnet 5.5, `claude -p`):

- With deferred loading, the vague tool was called in only 7 of 24 bare questions (no "check my notes"). Per-tool, 24
  of 24. When it wasn't called, Claude said it couldn't find the data rather than guessing, except on two questions it
  knew from memory.
- Calls to a tool left deferred before its schema was loaded: with only `search_notes` marked, Claude called
  `read_note` without loading it in 25 of 48 runs (27 calls). 23 worked, 4 failed input validation (`{}` or the
  ToolSearch query sent to the wrong tool); Claude recovered every time.
- A slow server (connecting after 10 or 25 s) did not lose its upfront tools: in `-p` mode and in stream-json input
  mode, Claude Code waited for it before the first request (24 of 24 runs had the tool in the first request).
- Claude Haiku 5.5 (one repetition, 96 runs): deferred 22/24 clear and 19/24 vague; per-tool 23/24 and 23/24. With a
  deferred vague tool, Haiku tried the blocked file and shell tools in 22 of 24 runs before giving up or searching.

Full tables: [`results/summary.md`](results/summary.md).

## What is here

| Path | What it is |
| --- | --- |
| `server.mjs` | The notes search server from [2026-10-15](../2026-10-15-mcp-server-under-100-lines/), plus `ALWAYS_LOAD=search` (sets the `_meta` flag on `search_notes`) and `STARTUP_DELAY_MS` (a slow server). |
| `bench/claude_run.py` | One `claude -p` launch with one MCP server, parsed from stream-json. |
| `bench/run.py` | 12 questions x 2 phrasings x 7 loading conditions, every tool call recorded. |
| `bench/conditions.json` | The seven conditions: names (clear or vague) x loading mode. |
| `bench/overhead.py` | Context cost of each loading mode on a one-turn prompt. |
| `bench/slow.py` | A server that takes 0, 10 or 25 s to connect, in `-p` mode and in stream-json input mode. |
| `bench/early_calls.py` | Counts calls to a still-deferred tool before it was loaded, from Claude Code's session transcripts. |
| `bench/summarize.py` | Builds the tables from `results/`. |
| `results/` | Raw runs (local paths replaced with `~` and `<work>`) and `summary.md`. |

## Mark a tool to load upfront

With the TypeScript SDK (`@modelcontextprotocol/sdk` 1.32.1), `registerTool` passes `_meta` through to `tools/list`:

```js
server.registerTool("search_notes", {
  description: "Full-text search over Markdown notes...",
  inputSchema: { query: z.string() },
  _meta: { "anthropic/alwaysLoad": true },
}, handler);
```

Or, as the person adding the server, load all of its tools upfront in the MCP config:

```json
{ "mcpServers": { "notes": { "command": "node", "args": ["server.mjs", "/path/to/notes"], "alwaysLoad": true } } }
```

`"alwaysLoad": false` in the config overrides a tool's own flag (measured: back to +23 tokens).

## Rerun the experiment

Needs Node 20 or later, Python 3 and Claude Code (I used 2.1.304 with Claude Sonnet 5.5 on a subscription; the
336 main runs reported USD 8.98 at API prices and took 56 minutes). The questions in `bench/questions.json` are about
the first seven posts of the blog, which were the corpus; with your own notes, write your own questions.

```bash
npm install
python3 bench/run.py <corpus> <empty-work-folder> results/rep1.jsonl sonnet
python3 bench/run.py <corpus> <empty-work-folder> results/rep2.jsonl sonnet
python3 bench/overhead.py <corpus> <another-empty-folder> results/overhead.json sonnet
python3 bench/slow.py <corpus> <another-empty-folder> results/slow.json sonnet
CONDITIONS=good,good+tool,vague,vague+tool python3 bench/run.py <corpus> <empty-work-folder> results/haiku.jsonl haiku
python3 bench/summarize.py results/rep1.jsonl results/rep2.jsonl results/haiku.jsonl
```

Every run sets `CLAUDE_CODE_DISABLE_CLAUDE_MDS=1` and `CLAUDE_CODE_DISABLE_AUTO_MEMORY=1` and blocks Claude Code's
file and shell tools (`Bash`, `Read`, `Grep`, `Glob`) and subagents, so the MCP server is the only way to the notes.
My first attempt left them on and a run grepped the whole home folder for the answer; it was discarded.

License: MIT.
