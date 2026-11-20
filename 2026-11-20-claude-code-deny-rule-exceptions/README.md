# Claude Code deny rule exceptions: can you block .env but still read .env.example?

**Finding:** yes, with a `!` rule in the same `deny` list. In Claude Code 2.1.329, `Read(.env*)` followed by
`Read(!.env.example)` blocked `.env`, `.env.local`, `.env.production` and `api/.env` for the Read tool and for `cat`,
and let both `.env.example` files through, in every run. An `allow` rule does not do it, the `!` rule must come after
the rule it carves from and live in the same settings file, Grep still hides the carved-out files, and the
character-class shortcut `.env.[!e]*` does the opposite of what gitignore says.

Write-up: https://aitoollabs.dev/posts/claude-code-deny-rule-exception/

## Results

One exact action per `claude -p` run (Sonnet 5.5, default permission mode, nothing pre-approved), 16 probes x 16
conditions x 2 reps = 512 runs. Every rep agreed with the other. Each file in the fixture holds its own fake key, so
"read" means that file's key reached the model and "blocked" means it did not. Rules live in the project's
`.claude/settings.local.json` (one condition also uses `.claude/settings.json`).

### Getting `.env.example` back (7 secret probes, 5 example probes, Read and `cat`)

| Rules | Secrets blocked | Examples readable |
| --- | --- | --- |
| deny `Read(**/.env*)` | 7/7 | 0/5 |
| + allow `Read(**/.env.example)` | 7/7 | 0/5 |
| deny `Read(.env*)`, `Read(!.env.example)` | 7/7 | **5/5** |
| deny `Read(**/.env*)`, `Read(!**/.env.example)` | 7/7 | **5/5** |
| same two rules, `!` listed first | 7/7 | 0/5 |
| `Read(.env*)` in local, `Read(!.env.example)` in settings.json | 7/7 | 0/5 |
| deny `Read(/.env*)`, `Read(!.env.example)` | 6/7 (`api/.env` read: `/` anchors at the launch folder only) | 2/5 (only the `api/` copy) |
| deny `Read(.env)`, `Read(.env.[!e]*)` | 4/7 (`.env.local`, `.env.production` read) | 0/5 |
| ask `Read(.env*)`, `Read(!.env.example)` | 5/7 (`cat .env`, `cat .env.production` read) | 5/5 |

Secret probes: Read of `.env`, `.env.local`, `.env.production`, `api/.env`, and of `.env` started from `api/`;
`cat .env`; `cat .env.production`. Example probes: Read and `cat` of `.env.example` and `api/.env.example`, and Read
of `.env.example` started from `api/`. An ask rule refuses in `-p` mode (nobody can approve); interactively it is a
prompt.

### What else showed up

- **Grep ignores the carve-out.** With the `!` rule, a Grep for the key name over the project returned no env file at
  all, examples included. That is the safe direction, but the exception only works for Read and `cat`.
- **Character classes are read literally.** `Read(.env.[!x]*)` blocked nothing; `Read(.env.[^e]*)` blocked only the
  example files; `Read(.env.[lp]*)` blocked `.env.local` and `.env.production` as expected. So `[!e]` and `[^e]` match
  the characters `!`, `^` and `e`, not "anything but e".
- **An ask rule on Read does not cover `cat`.** With only `ask: ["Read(.env*)"]`, the Read tool was refused (nobody to
  approve in `-p` mode) but `cat .env` and Grep returned every secret. The docs say Read *deny* rules apply to `cat`;
  ask rules do not.
- **Directories:** `Read(secrets/**)` + `Read(!secrets/public/**)` still blocked `secrets/public/readme.txt` (as the
  docs warn). `Read(secrets/*)` + `Read(!secrets/public.txt)` reopened `public.txt`, kept `key.txt` blocked, and still
  blocked everything under `secrets/public/`.

Full matrix: [`results/summary.md`](results/summary.md). Raw runs: [`results/probes.jsonl`](results/probes.jsonl)
(keys replaced with `<CANARY>`).

512 runs cost USD 1.41 at API prices (run on a subscription), median 4.1 s per run, about 7 minutes of wall time with
7 in parallel.

## What this does not cover

- Interactive sessions: an ask rule there shows a prompt instead of a refusal. The `cat` result (no prompt at all)
  should not change, but I did not drive an interactive session.
- User and managed settings (a scratch `CLAUDE_CONFIG_DIR` is not logged in, and I did not copy credentials).
- Bash routes that read a file without naming it (`grep -r`, Python, `source`): see
  [`2026-10-26-claude-code-deny-rules`](../2026-10-26-claude-code-deny-rules/).
- Edit rules, Windows and macOS. This ran on Linux.

## How to run it

Needs Claude Code (logged in) and Python 3.

```bash
python3 bench/fixture.py /tmp/denyex                       # one project per condition in /tmp
python3 bench/run.py /tmp/denyex /tmp/denyex/probes.jsonl
python3 bench/scrub.py /tmp/denyex /tmp/denyex/probes.jsonl results/probes.jsonl
python3 bench/summarize.py results/probes.jsonl > results/summary.md
```

`CONDITIONS=a,b`, `IDS=x,y`, `REPS=n`, `MODEL=...` and `WORKERS=n` (default 7) limit a run. `run.py` resumes: it
skips any (probe, condition, rep) already in the output file. The conditions live in `bench/conditions.json`, the
probes in `bench/probes.json`.

MIT license, like the rest of this repo.
