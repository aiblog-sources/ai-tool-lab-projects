# An MCP server in under 100 lines, tested end to end with Claude Code

**Finding:** a 97-line stdio MCP server that searches a folder of Markdown files worked in Claude Code on the first try,
but whether Claude *used* it depended on the names. With tool search on (the default), Claude only sees tool names and
server instructions until it loads a tool. Asked plain questions with no "check my notes", it called a well-named tool in
19 of 24 runs, a vaguely named one (`query`, "Runs a query.") in 3 of 24, and the same vague tool plus two sentences of
server instructions in 22 of 24.

Write-up: https://aitoollabs.dev/posts/mcp-server-for-claude-code/

| Tool naming (12 questions x 2 phrasings x 2 runs) | Called the MCP tool | Right answer |
| --- | --- | --- |
| `search_notes` with a clear description | 43 / 48 | 47 / 48 |
| `query`, "Runs a query." | 20 / 48 | 24 / 48 |
| `query` plus server instructions | 46 / 48 | 48 / 48 |
| `search_notes`, tool search off (`ENABLE_TOOL_SEARCH=false`) | 48 / 48 | 48 / 48 |

| Plain questions only (no "check my notes") | Called the MCP tool |
| --- | --- |
| `search_notes` | 19 / 24 |
| `query` | 3 / 24 |
| `query` plus server instructions | 22 / 24 |
| `search_notes`, tool search off | 24 / 24 |

Other numbers from the same runs (Claude Code 2.1.300, Claude Sonnet 5.5, a 2-core laptop with background load):

- Claude Code connected to the server in a median 638 ms (534 to 1170 ms, 192 launches). A search call takes about 9 ms.
- Context cost of the two tools: 22 input tokens while deferred, 348 tokens loaded upfront. Turning tool search off also
  loads about 14,500 tokens of Claude Code's own deferred tools, with or without this server.
- A `console.log` line on stdout did not break anything: Claude Code logs "Ignoring non-JSON line" and carries on (12 of
  12 runs fine). A `process.stdout.write("loading... ")` with no newline did: the first launch connected after 3.2 s, and
  every later launch timed out after 30 s.

## What is here

| Path | What it is |
| --- | --- |
| `server.mjs` | The server: `search_notes` (BM25 over every `.md` file in a folder, with snippets) and `read_note`. |
| `bench/handshake.mjs` | Spawns the server and times `initialize`, `tools/list` and one search over raw JSON-RPC. |
| `bench/run.py` | Asks `claude -p` 12 questions under 4 naming conditions and 2 phrasings, records every tool call. |
| `bench/questions.json`, `bench/conditions.json` | The questions with expected answers, and the naming variants. |
| `bench/overrides.json` | Two answers I graded by hand (guesses that the pattern counted as right). |
| `bench/overhead.py`, `bench/noisy-server.mjs` | Context cost of the tools, and four deliberate stdio mistakes. |
| `bench/summarize.py` | Builds the tables above from `results/`. |
| `results/` | Raw runs (local paths replaced with `~` and `<work>`), the summary, handshake and connect times. |

## Run the server

Needs Node 20 or later.

```bash
npm install
claude mcp add notes -- node "$PWD/server.mjs" /path/to/your/markdown/folder
```

Then ask Claude Code something your notes answer. To check the protocol without Claude:

```bash
node bench/handshake.mjs /path/to/your/markdown/folder 20
```

## Rerun the experiment

The benchmark calls `claude -p` 192 times (about 31 minutes; Claude Code reported USD 6.27 at API prices, it ran on a
subscription). It points the server at a folder of Markdown files and uses an empty working folder so the built-in file
tools find nothing there. I used the seven posts of the blog as the corpus; the questions in `bench/questions.json`
are about them, so with your own notes, write your own questions.

```bash
python3 bench/run.py <corpus> <empty-work-folder> results/rep1.jsonl sonnet
python3 bench/run.py <corpus> <empty-work-folder> results/rep2.jsonl sonnet
NOISY_REPS=3 python3 bench/overhead.py <corpus> <another-empty-folder> results/overhead.json sonnet
python3 bench/summarize.py results/rep1.jsonl results/rep2.jsonl
```

Both scripts set `CLAUDE_CODE_DISABLE_CLAUDE_MDS=1` and `CLAUDE_CODE_DISABLE_AUTO_MEMORY=1`, so personal CLAUDE.md
files and memory do not leak into the runs (my first full run had them on; it was discarded).

License: MIT.
