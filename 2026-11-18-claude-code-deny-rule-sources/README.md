# Claude Code deny rules by settings file: which ones still work from a subfolder?

**Finding:** in Claude Code 2.1.327, a deny rule in the repo's `.claude/settings.json` stops working as soon as you
start Claude Code in a subfolder: it is never loaded (the debug log shows Claude Code looking in `api/.claude/`, not
at the root). The same rule in `.claude/settings.local.json`, or passed with `--settings` (a file anywhere or inline
JSON), blocked every env file from both folders. Outside a git repo both project files fail from a subfolder. And
`Read(/.env)` means something different in every source: the launch folder in `settings.local.json`, the file's own
folder with `--settings <file>`, and nothing I could find with inline `--settings` JSON.

Write-up: https://aitoollabs.dev/posts/claude-code-deny-rule-settings-sources/

## Results

One exact action per `claude -p` run (Sonnet 5.5, default permission mode, nothing pre-approved), 16 probes x 17
conditions x 2 reps = 544 runs. Every rep agreed with the other. Fixture: a project with four env files, each holding
its own fake key: `.env`, `.env.local`, `api/.env`, `api/.env.local`. A probe is `Read` or `cat` on one file, or a
Grep for the key name; "blocked" counts files whose key did not reach the model.

### Where the rule lives: `Read(**/.env*)`

| Where the rule is | Started in root (4 files) | Started in `api/` (2 files) |
| --- | --- | --- |
| nowhere (control) | 0 blocked | 0 blocked |
| root `.claude/settings.json` | 4 | **0** |
| root `.claude/settings.local.json` | 4 | 2 |
| only `api/.claude/settings.json` | **0** | 2 |
| root `.claude/settings.json`, also passed as `--settings <its path>` | 4 | 2 |
| `--settings rules/deny.json` (a file outside the project) | 4 | 2 |
| `--settings '{"permissions":{"deny":[...]}}'` (inline JSON) | 4 | 2 |
| not a git repo: `.claude/settings.json` | 4 | **0** |
| not a git repo: `.claude/settings.local.json` | 4 | **0** |

The Read tool, `cat` and Grep agreed in every cell.

### Where `/` anchors: `Read(/.env)` and friends

| Rule and source | Started in root, blocked | Started in `api/`, blocked |
| --- | --- | --- |
| `Read(/.env)` in root `settings.local.json` | `.env` | `api/.env` |
| `Read(/.env)` in `--settings project/deny.json` | `.env` | `../.env` (deny rule matched) |
| `Read(/.env)` in `--settings project/.claude/deny.json` | nothing | nothing |
| `Read(/.env)` in `--settings rules/deny.json` (outside) | nothing | nothing |
| `Read(/.env)` inline | nothing | nothing |
| `Read(/<full project path>/.env)` inline | nothing | nothing |
| `Read(//<full project path>/.env)` inline | `.env` | `../.env` (deny rule matched) |
| `Read(./.env)` inline | `.env`, `api/.env` | `api/.env` |

From `api/`, a read of `../.env` is outside the working folder, so in `-p` mode it is refused either way (there is
nobody to approve the prompt). The error text tells the two apart: "denied by your permission settings" when a deny
rule matched, "Claude requested permissions to read from ..." when it did not. In an interactive session that second
case is a prompt you can approve.

Full matrix: [`results/summary.md`](results/summary.md). Raw runs: [`results/probes.jsonl`](results/probes.jsonl)
(keys replaced with `<CANARY>`). Settings lookup from the debug log:
[`results/debug-settings-lookup.md`](results/debug-settings-lookup.md).

544 runs cost USD 1.60 at API prices (run on a subscription), median 3.9 s per run, about 6.5 minutes with 7 in
parallel.

## What this does not cover

- User settings (`~/.claude/settings.json`) and managed settings: testing them safely needs a scratch config folder,
  and a scratch `CLAUDE_CONFIG_DIR` is not logged in. I did not copy credentials into one.
- Bash commands that read a file without naming it (`grep -r`, Python, `source`): see
  [`2026-10-26-claude-code-deny-rules`](../2026-10-26-claude-code-deny-rules/). Here `cat` is the only Bash command.
- Spellings (`.env` vs `.env*` vs `**/`): see [`2026-11-10-claude-code-deny-rule-spellings`](../2026-11-10-claude-code-deny-rule-spellings/).
- Windows and macOS. This ran on Linux.

## How to run it

Needs Claude Code (logged in) and Python 3.

```bash
python3 bench/fixture.py /tmp/denysrc                       # one project per condition in /tmp
python3 bench/run.py /tmp/denysrc /tmp/denysrc/probes.jsonl
python3 bench/scrub.py /tmp/denysrc /tmp/denysrc/probes.jsonl results/probes.jsonl
python3 bench/summarize.py results/probes.jsonl > results/summary.md
```

`CONDITIONS=a,b`, `IDS=x,y`, `REPS=n`, `MODEL=...` and `WORKERS=n` (default 7) limit a run. `run.py` resumes: it
skips runs already in the output file. Run it from `/tmp`, not under your home folder, and keep
`CLAUDE_CODE_DISABLE_CLAUDE_MDS=1` (set by `run.py`) so your own CLAUDE.md does not load. `conditions.json` uses
`{project}` for the condition's absolute project path.

MIT license.
