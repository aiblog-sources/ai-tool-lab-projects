# Does .claudeignore keep Claude Code out of package-lock.json?

**Finding:** no. Claude Code 2.1.306 does not read `.claudeignore` at all: with one listing the lockfiles, Claude
still pulled lockfile text in 12 of 12 version questions, the same as with no file. A `permissions.deny` Read rule
blocked every route (Read, Grep, Glob and Bash `grep`), but then Claude answered 0 of 12, and in 6 of them told the
user the project had **no lockfile**. Deny plus an allowed `npm ls --package-lock-only` answered 12 of 12 at half
the cost of reading the lockfile.

Write-up: https://aitoollabs.dev/posts/claude-code-claudeignore-lockfiles/

| Condition (Sonnet 5.5, 4 lockfile questions x 3 runs) | Runs that got lockfile text | Right answers | Mean cost per run (API prices) |
| --- | --- | --- | --- |
| Nothing | 12/12 | 12/12 | $0.040 |
| `.claudeignore` listing the lockfiles | 12/12 | 12/12 | $0.035 |
| `.ignore` (ripgrep's ignore file) | 12/12 | 12/12 | $0.029 |
| `permissions.deny` Read rules | 0/12 | 0/12 (6 said "no lockfile") | $0.036 |
| "Never read lockfiles" in the system prompt | 1/12 | 1/12 | $0.034 |
| Deny + `--allowedTools "Bash(npm ls:*)"` + one-line hint | 0/12 | 12/12 | $0.020 |

Two code questions (6 runs each) were answered right in every condition. Full tables:
[`results/summary.md`](results/summary.md), one row per run in [`results/runs.csv`](results/runs.csv), every
lockfile access in [`results/touches.jsonl`](results/touches.jsonl), every answer in
[`results/answers.md`](results/answers.md).

## What a lockfile costs in Claude tokens

Counted with `claude -p` usage subtraction (`bench/lock_tokens.py`), no API key needed:

| Lockfile | Size | Claude tokens | o200k (GPT-5) | chars / 4 |
| --- | --- | --- | --- | --- |
| package-lock.json (fixture, 288 packages) | 128 KB | 73,135 | 52,351 | 32,771 |
| yarn.lock (same dependencies) | 88 KB | 61,605 | 44,269 | 22,511 |
| pnpm-lock.yaml (same dependencies) | 73 KB | 50,297 | 36,467 | 18,780 |
| uv.lock (pallets/flask) | 273 KB | 160,388 | 130,291 | 69,799 |
| poetry.lock (python-poetry/poetry) | 253 KB | 155,499 | 122,957 | 64,704 |
| go.sum (cli/cli) | 58 KB | 45,612 | 31,961 | 14,863 |
| composer.lock (composer/composer) | 91 KB | 31,275 | 22,398 | 23,271 |
| Gemfile.lock (rails/rails) | 43 KB | 27,022 | 21,775 | 10,942 |
| Cargo.lock (BurntSushi/ripgrep) | 13 KB | 7,157 | 5,024 | 3,387 |

Sources and commits: [`results/lockfile_sources.md`](results/lockfile_sources.md).

## What's here

- `fixture/`: a small Express API (`orders-api`) with 10 direct dependencies. Its `package-lock.json` is not
  committed (this repo's push guard reads version strings that start with `10.` as private IP addresses); run
  `./make-fixture.sh` to create one. A lockfile made on another day can resolve newer versions, so check the
  expected answers in `bench/questions.json`.
- `bench/run.py`: copies the fixture to a fresh temp folder per run, applies a condition, asks one question with
  `claude -p --output-format stream-json`, saves the events.
- `bench/questions.json`: 4 questions only the lockfile can answer (exact express version, the two `debug`
  versions, the shipped `cookie` version for CVE-2024-47764, the `vite` that `vitest` brings) and 2 code questions.
- `bench/analyze.py`: per-run rows, lockfile accesses, automatic grading (answers were also read by hand).
- `bench/lock_tokens.py`: Claude, o200k and chars/4 counts for any file.
- `results/trust_check.md`: why project `allow` rules did nothing in the first `npm ls` batch.

## How to run it

Needs Node 22, Python 3.12 and a logged-in Claude Code (`claude -p` works with a subscription).

```bash
./make-fixture.sh
python3 bench/run.py /tmp/lockbench-runs --reps 3 --jobs 3
python3 bench/run.py /tmp/lockbench-runs --reps 3 --jobs 3 --only deny_npmls
python3 bench/analyze.py /tmp/lockbench-runs results-mine

# token counts (needs tiktoken)
pip install tiktoken==0.12.0
python3 bench/lock_tokens.py tokens.csv fixture/package-lock.json path/to/uv.lock
```

The 120 runs took 23 minutes of run time (3 in parallel) and would cost USD 3.74 at API prices; they ran on a
subscription. Runs set `CLAUDE_CODE_DISABLE_CLAUDE_MDS=1` and `CLAUDE_CODE_DISABLE_AUTO_MEMORY=1` so no personal
`CLAUDE.md` leaks in, which is also why the instruction condition uses `--append-system-prompt` instead of a
`CLAUDE.md` file.

License: MIT.
