| Condition | Runs | First search: MCP | First search: project script | First search: plugin script | No search | Project Skill invoked | Plugin Skill invoked | Tried to browse files | Right | Median turns | Median s | USD |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| sonnet-project | 48 | 19 | 25 | 0 | 4 | 36 | 0 | 3 | 47 | 5.0 | 11.9 | 1.64 |
| sonnet-plugin-notes | 48 | 27 | 0 | 16 | 5 | 0 | 21 | 3 | 47 | 4.0 | 13.4 | 1.57 |
| sonnet-plugin-kb | 48 | 20 | 0 | 23 | 5 | 0 | 27 | 3 | 47 | 4.0 | 12.4 | 1.47 |
| sonnet-both-skills | 48 | 8 | 36 | 0 | 4 | 36 | 0 | 1 | 48 | 5.0 | 12.1 | 1.62 |
| sonnet-plugin-own-mcp | 48 | 39 | 0 | 5 | 4 | 0 | 5 | 2 | 48 | 4.0 | 12.6 | 1.95 |
| sonnet-plugin-kb-nonstrict | 48 | 23 | 0 | 21 | 4 | 0 | 27 | 0 | 48 | 4.0 | 15.4 | 1.81 |
| opus-project | 48 | 0 | 46 | 0 | 2 | 46 | 0 | 0 | 48 | 5.0 | 8.8 | 3.40 |
| opus-plugin-notes | 48 | 0 | 0 | 46 | 2 | 0 | 46 | 0 | 48 | 5.0 | 9.1 | 3.36 |
| opus-plugin-kb | 48 | 0 | 0 | 46 | 2 | 0 | 46 | 0 | 48 | 5.0 | 8.4 | 3.14 |

By phrasing (hinted = the prompt starts with "Check my notes.")

| Condition | Phrasing | Runs | First search: MCP | First search: project script | First search: plugin script | No search | Project Skill invoked | Plugin Skill invoked | Tried to browse files | Right | Median turns | Median s | USD |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| sonnet-project | hinted | 24 | 17 | 7 | 0 | 0 | 18 | 0 | 1 | 24 | 5.0 | 12.1 | 0.79 |
| sonnet-project | bare | 24 | 2 | 18 | 0 | 4 | 18 | 0 | 2 | 23 | 5.0 | 11.3 | 0.85 |
| sonnet-plugin-notes | hinted | 24 | 24 | 0 | 0 | 0 | 0 | 5 | 0 | 24 | 4.0 | 13.7 | 0.77 |
| sonnet-plugin-notes | bare | 24 | 3 | 0 | 16 | 5 | 0 | 16 | 3 | 23 | 4.0 | 12.7 | 0.80 |
| sonnet-plugin-kb | hinted | 24 | 20 | 0 | 4 | 0 | 0 | 8 | 0 | 24 | 4.0 | 12.6 | 0.66 |
| sonnet-plugin-kb | bare | 24 | 0 | 0 | 19 | 5 | 0 | 19 | 3 | 23 | 5.0 | 12.3 | 0.81 |
| sonnet-both-skills | hinted | 24 | 8 | 16 | 0 | 0 | 16 | 0 | 0 | 24 | 4.0 | 12.2 | 0.70 |
| sonnet-both-skills | bare | 24 | 0 | 20 | 0 | 4 | 20 | 0 | 1 | 24 | 5.0 | 12.0 | 0.92 |
| sonnet-plugin-own-mcp | hinted | 24 | 20 | 0 | 4 | 0 | 0 | 4 | 0 | 24 | 4.0 | 12.3 | 0.98 |
| sonnet-plugin-own-mcp | bare | 24 | 19 | 0 | 1 | 4 | 0 | 1 | 2 | 24 | 4.0 | 12.9 | 0.97 |
| sonnet-plugin-kb-nonstrict | hinted | 24 | 16 | 0 | 8 | 0 | 0 | 14 | 0 | 24 | 4.0 | 16.2 | 0.89 |
| sonnet-plugin-kb-nonstrict | bare | 24 | 7 | 0 | 13 | 4 | 0 | 13 | 0 | 24 | 4.0 | 14.8 | 0.92 |
| opus-project | hinted | 24 | 0 | 24 | 0 | 0 | 24 | 0 | 0 | 24 | 5.0 | 8.9 | 1.75 |
| opus-project | bare | 24 | 0 | 22 | 0 | 2 | 22 | 0 | 0 | 24 | 5.0 | 8.2 | 1.65 |
| opus-plugin-notes | hinted | 24 | 0 | 0 | 24 | 0 | 0 | 24 | 0 | 24 | 5.0 | 9.1 | 1.67 |
| opus-plugin-notes | bare | 24 | 0 | 0 | 22 | 2 | 0 | 22 | 0 | 24 | 5.0 | 9.3 | 1.68 |
| opus-plugin-kb | hinted | 24 | 0 | 0 | 24 | 0 | 0 | 24 | 0 | 24 | 5.0 | 8.9 | 1.55 |
| opus-plugin-kb | bare | 24 | 0 | 0 | 22 | 2 | 0 | 22 | 0 | 24 | 4.5 | 8.2 | 1.59 |

By repetition (first search MCP / project script / plugin script / none)

- sonnet-project: 10/12/0/2  9/13/0/2
- sonnet-plugin-notes: 14/0/7/3  13/0/9/2
- sonnet-plugin-kb: 10/0/12/2  10/0/11/3
- sonnet-both-skills: 4/18/0/2  4/18/0/2
- sonnet-plugin-own-mcp: 19/0/3/2  20/0/2/2
- sonnet-plugin-kb-nonstrict: 11/0/11/2  12/0/10/2
- opus-project: 0/23/0/1  0/23/0/1
- opus-plugin-notes: 0/0/23/1  0/0/23/1
- opus-plugin-kb: 0/0/23/1  0/0/23/1

Tool errors (runs with at least one, and the first error text of each kind)

- sonnet-project: 11 runs; Permission to use Bash with command cd <work>/work-script/.claude/skills/search-notes && g; Permission to use Bash with command cd <work>/work-script/.claude/skills/search-notes; nod; Permission to use Bash with command grep -i -n -E "idle|RAM|MB|memory" has been denied.; Permission to use Bash with command grep -n -i -E "idle|MB|RAM|RSS" has been denied.; Permission to use Bash with command grep -n -i -E "llama|GB|oom|memory" has been denied.; Permission to use Bash with command head -40 has been denied.; Permission to use Bash with command head -c 3000 has been denied.; Permission to use Bash with command ls -la && grep -rIl -i "json_schema" . has been denied
- sonnet-plugin-notes: 6 runs; Permission to use Bash with command cd <repo>/2026-11-06-plugin-skill-vs-project-skill/plu; Permission to use Bash with command grep -i -n -E "idle|RAM|MB|memory" has been denied.; Permission to use Bash with command ls -la && grep -rIl -i "grammar" . has been denied.; Permission to use Bash with command ls -la && grep -rIl -i -E "json_schema|llama-server" .; Permission to use Bash with command ls -la && grep -ril "grammar" . has been denied.
- sonnet-plugin-kb: 9 runs; Permission to use Bash with command cd <repo>/2026-11-06-plugin-skill-vs-project-skill/plu; Permission to use Bash with command grep -i -n -E "idle|RAM|MB|memory" has been denied.; Permission to use Bash with command grep -n -i -E "87 of|without|every checked" has been d; Permission to use Bash with command grep -n -i -E "idle|RAM|MB|memory" has been denied.; Permission to use Bash with command grep -rniE "oom|out of memory|killed" <work>/work-plai; Permission to use Bash with command head -30 has been denied.; Permission to use Bash with command head -40 has been denied.; Permission to use Bash with command ls -la && grep -rIl -i "grammar" . has been denied.; Permission to use Bash with command ls -la && grep -ril "json_schema\|llama-server" . has 
- sonnet-both-skills: 13 runs; Permission to use Bash with command cd <work>/work-script/.claude/skills/search-notes; gre; Permission to use Bash with command cd <work>/work-script/.claude/skills/search-notes; nod; Permission to use Bash with command grep -i -n -E "llama|GB|OOM|memory" has been denied.; Permission to use Bash with command grep -i -n -E "llama|oom|GB|memory" has been denied.; Permission to use Bash with command grep -inE "idle|MB|RSS|RAM" has been denied.; Permission to use Bash with command grep -n -i -E "second|late|offset" has been denied.; Permission to use Bash with command grep -niE "cache" has been denied.; Permission to use Bash with command head -40 has been denied.; Permission to use Bash with command ls -la && grep -rIl -i "json_schema" . has been denied
- sonnet-plugin-own-mcp: 10 runs; MCP error -32602: Input validation error: Invalid arguments for tool search_notes: Invalid; Permission to use Bash with command grep -iE "second|late|offset" has been denied.; Permission to use Bash with command grep -rIil "ticket" <work>/work-plain has been denied.; Permission to use Bash with command grep -rn -i -E "second|late" $(find / -name hermes-age
- sonnet-plugin-kb-nonstrict: 6 runs; Permission to use Bash with command cd <repo>/2026-11-06-plugin-skill-vs-project-skill/plu; Permission to use Bash with command grep -i -n -E "idle|RAM|MB|memory" has been denied.; Permission to use Bash with command grep -inE "idle|RAM|MB|memory" has been denied.; Permission to use Bash with command head -30 has been denied.; Permission to use Bash with command head -40 has been denied.
- opus-project: 15 runs; Permission to use Bash with command cd /tmp && node "<work>/work-script/.claude/skills/sea; Permission to use Bash with command cd <work>/work-script/.claude/skills/search-notes; nod; Permission to use Bash with command grep -n -i -A3 -B3 "cache" has been denied.; Permission to use Bash with command grep -n -i -B2 -A2 "cold\|first token\|2-core\|1\.7\|0; Permission to use Bash with command grep -n -i -B2 -A6 "prompt cache" has been denied.; Permission to use Bash with command grep -n -i -C3 "cache" has been denied.; Permission to use Bash with command grep -n -i -E "llama|GB|OOM|memory" has been denied.; Permission to use Bash with command grep -n -i -E "second|late|:00|fired" has been denied.; Permission to use Bash with command grep -n -i -E "second|late|on time|:00" has been denie; Permission to use Bash with command head -40 has been denied.; Permission to use Bash with command head -c 3000 has been denied.
- opus-plugin-notes: 20 runs; Permission to use Bash with command cd /tmp && node "<repo>/2026-11-06-plugin-skill-vs-pro; Permission to use Bash with command cd /tmp; S="<repo>/2026-11-06-plugin-skill-vs-project-; Permission to use Bash with command cd <repo>/2026-11-06-plugin-skill-vs-project-skill/plu; Permission to use Bash with command grep -n -i -A3 -B3 "cache" has been denied.; Permission to use Bash with command grep -n -i -B2 -A6 "prompt cache" has been denied.; Permission to use Bash with command grep -n -i -E "GB|llama|oom|memory" has been denied.; Permission to use Bash with command grep -n -i -E "cold|first token|2-core|core" has been ; Permission to use Bash with command grep -n -i -E "cold|first token|2-core|laptop|0\.4|1\.; Permission to use Bash with command grep -n -i -E "cold|first token|TTFT|1\.7|0\.4" has be; Permission to use Bash with command grep -n -i -E "llama|GB|oom|memory" has been denied.; Permission to use Bash with command grep -n -i -E "second|late|:00|on time" has been denie; Permission to use Bash with command grep -n -i -E "second|late|on the minute|:00" has been; Permission to use Bash with command grep -n -i -E "second|on the minute|:00" has been deni; Permission to use Bash with command grep -n -i -E "second|on time|:00|late" has been denie; Permission to use Bash with command head -40 has been denied.; Permission to use Bash with command head -c 3000 has been denied.
- opus-plugin-kb: 15 runs; Permission to use Bash with command grep -n -i -A3 -B3 "cache" has been denied.; Permission to use Bash with command grep -n -i -B2 -A4 "cache" has been denied.; Permission to use Bash with command grep -n -i -B2 -A6 "prompt cache\|cache-ram\|cache" ha; Permission to use Bash with command grep -n -i -C3 "llama\|GB\|memory" has been denied.; Permission to use Bash with command grep -n -i -E "GB|llama|oom|memory" has been denied.; Permission to use Bash with command grep -n -i -E "context|token|local|ollama" has been de; Permission to use Bash with command grep -n -i -E "second|late|:00|49" has been denied.; Permission to use Bash with command grep -n -i -E "second|late|:00|fired" has been denied.; Permission to use Bash with command grep -n -i -E "second|on the minute|:00" has been deni; Permission to use Bash with command grep -n -i -E "second|on the minute|:00|late" has been; Permission to use Bash with command head -40 has been denied.; Permission to use Bash with command head -c 3000 has been denied.

Wrong answers

- sonnet-plugin-notes q5 bare first=None: "I couldn't find the answer. The shell command I used to search the working directory was denied, so I haven't seen any results or data for those 120 replies. I won't guess a number.\n\nIf you point me to the results file o"
- sonnet-project q4 bare first=None: "Ollama's default context window is 2048 tokens in older versions. Newer releases choose it based on available VRAM (4k for under 24 GB, 32k for 24–48 GB, 256k for 48 GB or more). With no GPU there's no VRAM to size from,"
- sonnet-plugin-kb q5 bare first=None: "I can't answer this. I don't have the result, and my attempt to search the working directory for it was denied. I won't guess a pass count out of 120. If you point me to the results file or paste the output, I'll read it"

Runs: 432, total USD 19.97, models ['claude-opus-5-5', 'claude-sonnet-5-5'], MCP servers ['notes', 'plugin:kb:notes'], MCP status ['connected']
