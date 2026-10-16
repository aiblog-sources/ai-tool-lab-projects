# Discarded first pass

`claude_counts_no_prefix.csv` was measured without the fixed first line in every prompt. Six of the 29 files start
with `/` (a `//` or `//!` comment), and `claude -p` then sends about 300 fewer tokens of its own context, so
`file run - baseline run` came out too low for them. Example: cobra's `powershell_completions_test.go` came out at
122 tokens here and 427 with the prefix (Sonnet 5.5).

Manual checks run while finding the cause (Claude Code 2.1.301, Sonnet 5.5, total input tokens of one run):

| Prompt | Input tokens | Answer |
| --- | --- | --- |
| `hello there` + `Reply with OK.` | 548 | OK |
| `// hello there` + `Reply with OK.` | 244 | OK |
| `// hello there` + `What was the first line of my message, verbatim?` | 251 | `// hello there` |

The model still receives the text; only Claude Code's own context around it changes. With the license header of
that Go file alone, the subtraction gave -87 tokens (Sonnet 5.5) and -85 (Haiku 5.5).
