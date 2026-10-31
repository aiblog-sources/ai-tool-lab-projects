| Model | Claude Code env | Server delay | Loading | Typed after | Runs | Turn 1: search called | Turn 1: right | Turn 1 answer (no number / wrong) | Turn 2: right | Prompt sent (s) | Server connected (s) | Turn 1 first request (s) |
| --- | --- | ---: | --- | ---: | ---: | ---: | ---: | --- | ---: | --- | --- | --- |
| sonnet | - | 0 s | deferred | 0 s | 2 | 2/2 | 2/2 | 0 / 0 | 0/0 | 2.2, 1.9 | 1.9, 1.6 | 1.8, 1.6 |
| sonnet | - | 0 s | per-server | 0 s | 2 | 2/2 | 2/2 | 0 / 0 | 0/0 | 2.8, 2.7 | 2.3, 2.3 | 2.4, 2.2 |
| sonnet | - | 0 s | per-tool | 0 s | 2 | 2/2 | 2/2 | 0 / 0 | 0/0 | 1.8, 1.9 | 1.6, 1.5 | 1.6, 1.5 |
| sonnet | - | 3 s | deferred | 0 s | 2 | 0/1 | 0/1 | 1 / 0 | 1/1 | 1.7 | 4.5 | 1.4 |
|  | error: input box never appeared |
| sonnet | - | 3 s | per-server | 0 s | 2 | 1/2 | 1/2 | 1 / 0 | 1/1 | 1.7, 1.6 | 4.5, 4.3 | 1.5, 1.4 |
| sonnet | - | 5 s | deferred | 0 s | 2 | 0/2 | 0/2 | 2 / 0 | 2/2 | 3.4, 1.6 | 8.0, 6.4 | 3.0, 1.4 |
| sonnet | - | 5 s | per-server | 0 s | 2 | 0/2 | 0/2 | 2 / 0 | 2/2 | 2.6, 2.3 | 7.4, 7.0 | 2.2, 1.9 |
| sonnet | - | 5 s | per-tool | 0 s | 2 | 1/2 | 1/2 | 1 / 0 | 1/1 | 2.4, 1.9 | 7.1, 6.7 | 2.0, 1.7 |
| sonnet | - | 15 s | deferred | 0 s | 2 | 0/2 | 0/2 | 2 / 0 | 2/2 | 4.5, 6.7 | 17.4, 19.5 | 4.2, 6.2 |
| sonnet | - | 15 s | deferred | 20 s | 2 | 2/2 | 2/2 | 0 / 0 | 0/0 | 21.6, 21.8 | 16.6, 16.3 | 21.4, 21.5 |
| sonnet | - | 15 s | per-server | 0 s | 2 | 0/2 | 0/2 | 2 / 0 | 2/2 | 2.0, 7.0 | 16.6, 20.3 | 1.7, 6.6 |
| sonnet | - | 15 s | per-tool | 0 s | 2 | 0/2 | 0/2 | 2 / 0 | 1/2 | 2.4, 9.0 | 17.1, 22.3 | 2.0, 8.5 |
| sonnet | - | 40 s | deferred | 0 s | 2 | 0/2 | 0/2 | 2 / 0 | 0/2 | 4.8, 6.8 | not yet, not yet | 4.5, 6.5 |
| sonnet | - | 40 s | deferred | 45 s | 2 | 0/2 | 0/2 | 2 / 0 | 0/2 | 47.2, 46.6 | not yet, not yet | 47.0, 46.4 |
| sonnet | - | 40 s | per-server | 0 s | 2 | 0/2 | 0/2 | 2 / 0 | 0/2 | 2.8, 3.7 | not yet, not yet | 2.5, 2.9 |
| sonnet | - | 40 s | per-tool | 0 s | 2 | 0/2 | 0/2 | 2 / 0 | 0/2 | 3.0, 5.9 | not yet, not yet | 2.6, 5.5 |
| sonnet | CLAUDE_CODE_MCP_ALWAYS_LOAD_JOIN_BEFORE_INPUT=1 | 15 s | per-server | 0 s | 2 | 0/2 | 0/2 | 2 / 0 | 1/2 | 1.6, 1.6 | 16.3, 16.4 | 1.3, 1.4 |
| sonnet | CLAUDE_CODE_MCP_ALWAYS_LOAD_JOIN_BEFORE_INPUT=1 | 15 s | per-tool | 0 s | 2 | 0/2 | 0/2 | 2 / 0 | 2/2 | 2.0, 1.8 | 16.7, 16.5 | 1.6, 1.5 |
| sonnet | CLAUDE_CODE_MCP_STARTUP_WAIT_MS=30000 | 15 s | deferred | 0 s | 2 | 0/2 | 0/2 | 2 / 0 | 2/2 | 2.1, 2.2 | 16.8, 16.7 | 1.8, 1.8 |
| sonnet | CLAUDE_CODE_MCP_STARTUP_WAIT_MS=30000 | 15 s | per-tool | 0 s | 2 | 0/2 | 0/2 | 2 / 0 | 2/2 | 2.0, 2.1 | 16.8, 16.8 | 1.8, 1.8 |
| sonnet | MCP_CONNECTION_NONBLOCKING=0 | 15 s | deferred | 0 s | 2 | 0/2 | 0/2 | 2 / 0 | 1/2 | 1.6, 1.7 | not yet, 16.4 | 1.4, 1.4 |
| sonnet | MCP_CONNECTION_NONBLOCKING=0 | 15 s | per-server | 0 s | 2 | 0/2 | 0/2 | 2 / 0 | 1/2 | 1.5, 1.8 | 16.3, not yet | 1.3, 1.5 |
| sonnet | MCP_CONNECTION_NONBLOCKING=0;MCP_CONNECT_TIMEOUT_MS=30000 | 15 s | deferred | 0 s | 2 | 0/2 | 0/2 | 2 / 0 | 1/2 | 1.5, 1.8 | 16.3, 16.4 | 1.3, 1.5 |
| sonnet | MCP_CONNECTION_NONBLOCKING=0;MCP_CONNECT_TIMEOUT_MS=30000 | 15 s | per-server | 0 s | 2 | 0/2 | 0/2 | 2 / 0 | 2/2 | 1.7, 1.8 | 16.4, 16.6 | 1.4, 1.6 |
| sonnet | MCP_CONNECTION_NONBLOCKING=false | 15 s | deferred | 0 s | 2 | 0/2 | 0/2 | 2 / 0 | 2/2 | 5.1, 2.0 | 17.9, 16.8 | 4.9, 1.7 |
| sonnet | MCP_CONNECTION_NONBLOCKING=false | 15 s | per-tool | 0 s | 2 | 0/2 | 0/2 | 2 / 0 | 2/2 | 2.0, 2.3 | 16.7, 17.0 | 1.7, 2.0 |
| sonnet | MCP_TIMEOUT=60000 | 40 s | deferred | 45 s | 2 | 2/2 | 2/2 | 0 / 0 | 0/0 | 46.6, 46.6 | 41.3, 41.5 | 46.4, 46.4 |
| haiku | - | 15 s | deferred | 0 s | 1 | 0/1 | 0/1 | 1 / 0 | 1/1 | 1.5 | 16.3 | 1.5 |
| haiku | - | 15 s | per-tool | 0 s | 1 | 0/1 | 0/1 | 1 / 0 | 1/1 | 1.7 | 16.4 | 1.5 |
| opus | - | 15 s | deferred | 0 s | 1 | 0/1 | 0/1 | 1 / 0 | 1/1 | 1.6 | 16.3 | 1.4 |
| opus | - | 15 s | per-tool | 0 s | 1 | 0/1 | 0/1 | 1 / 0 | 1/1 | 1.7 | 16.3 | 1.4 |

| `claude -p` | Server delay | Loading | Runs | Search called | Right | Server connected (s) | Wall time (s) |
| --- | ---: | --- | ---: | ---: | ---: | --- | --- |
| headless | 0 s | deferred | 2 | 2/2 | 2/2 | 0.9, 0.9 | 6.4, 7.1 |
| headless | 0 s | per-tool | 2 | 2/2 | 2/2 | 0.9, 0.9 | 5.0, 5.2 |
| headless | 15 s | deferred | 2 | 2/2 | 2/2 | 15.9, 15.9 | 21.7, 22.5 |
| headless | 15 s | per-tool | 2 | 2/2 | 2/2 | 16.0, 16.0 | 19.9, 19.7 |
| headless | 40 s | deferred | 2 | 0/2 | 0/2 | not yet, not yet | 36.3, 36.1 |
| headless | 40 s | per-tool | 2 | 0/2 | 0/2 | not yet, not yet | 37.0, 36.2 |

Headless cost: USD 0.28 for 12 runs (Claude Code's own figure, API prices).

"not yet": the server had not connected when the session ended (each session ends after its last turn). The 40 s servers never connect because Claude Code's MCP_TIMEOUT (30 s by default) stops them first.
