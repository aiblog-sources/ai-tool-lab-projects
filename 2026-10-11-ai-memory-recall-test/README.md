# Recall test for an MCP memory server

Does the right note come back when a coding agent asks its memory a plain question? This folder measures it
for [ai-memory](https://github.com/akitaonrails/ai-memory), the MCP server the AI Tool Labs agent uses to keep
notes between sessions.

**Finding:** asked 24 plain questions (12 in English, 12 in Portuguese), ai-memory put the right note first about
half the time and in the top 5 for 21 of 24, in about 30 ms per query. The misses were generic questions and notes
whose titles lacked the words people ask with. One of 14 notes had been saved into another project.

Results (2026-10-11):

| Language | Questions | Hit@1 | Hit@3 | Hit@5 | MRR | Median latency |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| en | 12 | 6 | 8 | 10 | 0.61 | 27 ms |
| pt | 12 | 7 | 11 | 11 | 0.74 | 32 ms |

Write-up: https://aitoollabs.dev/posts/mcp-memory-recall-test/

## What it does

- `questions.json`: 12 facts from one project's notes, each asked in English and in Portuguese, with the page
  that holds the answer.
- `eval.py`: speaks MCP over Streamable HTTP (plain JSON-RPC, no SDK), calls the server's search tool once per
  question with explicit workspace and project, and records the rank of the expected page in the top 5 and the
  latency.
- `results.json`: the run used in the post.

## Run it against your own notes

1. Write your own `questions.json` (the facts above only exist in the original project's memory).
2. Run:

```bash
MCP_URL=http://127.0.0.1:<port>/mcp MEMORY_WORKSPACE=<workspace> MEMORY_PROJECT=<project> python3 eval.py
```

`MEMORY_TOOL` changes the tool name if your memory server calls its search tool something else; the script
expects the tool to return JSON with a `hits` list of objects that have a `path`.

Python 3.10+ standard library only.

License: MIT.
