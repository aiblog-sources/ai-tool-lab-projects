# Does a slow MCP server miss the first prompt in Claude Code?

**Finding:** in an interactive Claude Code session (2.1.316), a stdio MCP server that took 5 seconds or more to
connect was not usable on the first prompt in 43 of 45 runs (servers 3 s or slower), with `alwaysLoad` or without it. Claude searched for
the tool, found unrelated tools, and answered that it had no data. Headless `claude -p` waited for the same server and
answered right in 4 of 4 runs. A server slower than 30 s timed out in both modes unless `MCP_TIMEOUT` was raised.

Write-up: https://aitoollabs.dev/posts/claude-code-mcp-server-still-connecting/

| Interactive session, prompt typed as soon as the input box appeared | Runs | First prompt used the tool | "Please try again" used it |
| --- | ---: | ---: | ---: |
| Server connects in ~1.5 s (no delay) | 6 | 6 | n/a |
| Server delayed 3 s | 3 | 1 | 2 of 2 |
| Server delayed 5 s | 6 | 1 | 5 of 5 |
| Server delayed 15 s (Sonnet, Haiku, Opus) | 10 | 0 | 9 of 10 |
| Server delayed 15 s, with one of five Claude Code env settings | 20 | 0 | 16 of 20 |
| Server delayed 40 s (times out at 30 s) | 6 | 0 | 0 of 6 |
| Server delayed 15 s, prompt typed 20 s after launch | 2 | 2 | n/a |
| Server delayed 40 s, prompt typed 45 s after launch | 2 | 0 | 0 of 2 |
| Server delayed 40 s, `MCP_TIMEOUT=60000`, typed after 45 s | 2 | 2 | n/a |

| Headless `claude -p`, same question | Runs | Used the tool |
| --- | ---: | ---: |
| Server delayed 0 s or 15 s | 8 | 8 |
| Server delayed 40 s | 4 | 0 |

Why it misses: Claude Code's ToolSearch waits for a server that is still connecting, but only when the search finds
nothing. Claude's searches ("notes search", "notes gemma") matched unrelated built-in tools, so the 75 such searches made
before the server connected returned in a median of 25 ms. A `select:` of tool names that did not exist yet waited until the server
connected or about 5 s had passed (35 calls, median 5,034 ms). Full tables: [`results/summary.md`](results/summary.md)
and [`results/toolsearch.md`](results/toolsearch.md).

Five Claude Code environment settings were tried with a 15 s server: `MCP_CONNECTION_NONBLOCKING=0` (documented:
"make Claude Code wait for servers to connect before the first query"), the same plus `MCP_CONNECT_TIMEOUT_MS=30000`,
`MCP_CONNECTION_NONBLOCKING=false`, `CLAUDE_CODE_MCP_STARTUP_WAIT_MS=30000` (documented for non-interactive sessions)
and `CLAUDE_CODE_MCP_ALWAYS_LOAD_JOIN_BEFORE_INPUT=1` (a name found in the binary, not documented). None of them made
the first turn wait: 0 of 20. The docs also say a server with `"alwaysLoad": true` makes startup wait up to 5 s; in
all 57 sessions the input box was drawn before Claude Code had even spawned the server, and the first API request left
0.07 to 0.49 s after the prompt was submitted.

## What is here

| Path | What it is |
| --- | --- |
| `server.mjs` | The notes search MCP server from [2026-10-20](../2026-10-20-mcp-always-load/) plus `EVENT_LOG` (start, connected and tool-call times as JSON lines). `STARTUP_DELAY_MS` holds the stdio connection back; `ALWAYS_LOAD=search` sets `_meta["anthropic/alwaysLoad"]` on the search tool. |
| `bench/run.py` | Starts interactive `claude` in a detached tmux pane, waits for the input box, types the question, waits for the turn to end (from the session transcript), and sends "Please try again." if the notes tool was not called. |
| `bench/headless.py` | The same question with `claude -p`. |
| `bench/summarize.py` | Builds `results/summary.md`. |
| `bench/toolsearch_stats.py` | Times every ToolSearch call made before the server connected (needs the local transcripts). |
| `bench/toolsearch_timing.py` | The same, run by run. |
| `bench/export.py` | Copies rows into `results/` with local paths replaced by `~` and `<work>`. |
| `results/*.jsonl` | One row per session: timings, what each request carried, tool calls, answers, two screen captures. |

## How the runs were set up

- Claude Code 2.1.316, Claude Sonnet 5.5 (plus two runs each on Haiku 5.5 and Opus 5.5), subscription login.
- 57 interactive sessions and 12 headless runs. One more interactive session never drew its input box within 60 s
  and is left out (its row is in `results/envs2.jsonl` with the error).
- One question whose answer is only in the notes ("What generation speed in tokens per second did Gemma 4 E2B reach
  on the 2-core laptop?"; the notes say 7.1). A turn counts as right when the answer contains 7.1.
- `--mcp-config` with only this server and `--strict-mcp-config`, `--setting-sources project`, `--permission-mode
  default`, `CLAUDE_CODE_DISABLE_CLAUDE_MDS=1`, `CLAUDE_CODE_DISABLE_AUTO_MEMORY=1`. Bash, Read, Grep, Glob, Task,
  Monitor, Workflow, WebSearch and WebFetch were disallowed, so the MCP server was the only way to the notes. In a
  normal session Claude could also grep files, which this test did not measure.
- The prompt is typed as soon as the input box is drawn (median 1.0 s after launch, 0.75 to 4.8 s). Claude Code
  spawned the server a median 1.6 s after launch (1.2 to 7.3 s), after the input box in every session, so "delay 5 s"
  means the server connected 6.4 to 8 s after launch.
- Tool lists per API request, `pendingMcpServers` and ToolSearch results come from Claude Code's session transcript
  (`~/.claude/projects/<folder>/<session>.jsonl`). The transcripts are not committed (they hold the user's own
  settings); only derived fields are.

## Rerun it

Needs Node 22, Python 3.10+, tmux and a logged-in Claude Code.

```bash
npm install
mkdir work      # an empty folder; accept the trust dialog for it once by running `claude` in it
python3 bench/run.py <corpus-folder> work rows.jsonl 2
DELAYS=15000 CONDS=deferred,per-tool CLAUDE_ENV="CLAUDE_CODE_MCP_STARTUP_WAIT_MS=30000" python3 bench/run.py <corpus-folder> work envs.jsonl 2
DELAYS=15000 CONDS=deferred TYPE_DELAY=20 python3 bench/run.py <corpus-folder> work late.jsonl 2
DELAYS=0,15000,40000 python3 bench/headless.py <corpus-folder> work headless.jsonl 2
python3 bench/summarize.py rows.jsonl envs.jsonl late.jsonl headless.jsonl
```

The corpus was the first seven posts of the blog as Markdown files; any folder of `.md` files works if you change
the question. In tmux, an Enter sent in the same burst as the text is taken as part of a paste and dropped, so
`run.py` waits 0.4 s and presses Enter again until the prompt shows up in the transcript.

MIT license.
