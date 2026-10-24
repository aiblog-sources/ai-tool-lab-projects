# Can Claude explain how code fits together from a Repomix --compress pack?

**Finding:** on 15 architecture questions ("trace what happens when...", "how does X decide Y") about three repos,
Claude Sonnet 5.5 covered 93% of the checklist points with the full Repomix pack and 75% with `--compress`, which
cut its input by 12% to 50%. It made 1 wrong claim with `--compress` and 0 with the full pack; with no repo at
all it made 17, mostly about a ky design that no longer exists. On ky, Repomix 1.18.1 `--compress` drops every
JavaScript/TypeScript `#private` method (28 of 28 in ky's main class); a one-line query fix brings them back and
raised ky's score from 41 to 45 of 50.

Write-up: https://aitoollabs.dev/posts/repomix-compress-architecture-questions/

| Condition (5 questions per repo x 5 points x 2 runs) | ky (TypeScript) | requests (Python) | diff-tour (Go, new repo) | All | Wrong claims |
| --- | --- | --- | --- | --- | --- |
| No repo (model memory) | 34/50 | 49/50 | 0/50 | 83/150 | 17 |
| Full pack | 48/50 | 49/50 | 42/50 | 139/150 | 0 |
| `--compress` | 41/50 | 35/50 | 36/50 | 112/150 | 1 |
| `--compress`, private methods fixed | 45/50 | | | | 0 |

| Repo | Full pack, Claude input tokens | `--compress` | Saved |
| --- | --- | --- | --- |
| ky | 78,307 | 69,107 | 12% |
| requests | 81,105 | 50,993 | 37% |
| diff-tour | 101,911 | 51,060 | 50% |

Token counts are the median input tokens Claude Code reported per run, so they include about 700 tokens of prompt
and question. Full tables, per question and with every wrong claim: [`results/summary.md`](results/summary.md).

## What it does

1. `pack.sh` packs the source of ky, requests and diff-tour with Repomix 1.18.1, full and `--compress`.
   diff-tour (a Go terminal app, created 2026-10-10) is newer than the model's training data, so it is the repo
   the model has never seen: with no pack, Claude answered "NOT SURE" to all 10 of its runs.
2. `run.py` asks Claude each of the 15 questions in [`questions.json`](questions.json) with each pack and once
   with no repo, twice, through `claude -p` with tools off. The prompt asks for at most 150 words, the functions
   involved, and to say which part is missing instead of guessing.
3. Each question has 5 checklist points written from the source before any run. `blind.py` shuffles the 100
   answers and hides the condition; I graded them by hand ([`results/grading-sheet.md`](results/grading-sheet.md),
   [`results/grades.psv`](results/grades.psv)), then `summarize.py` joins the grades with the runs
   ([`results/grading-key.json`](results/grading-key.json)). A point counts when its core claim is stated and
   matches the current code. A wrong claim is a specific statement the current code contradicts.
4. `analyze_packs.py` counts how many function and method definitions survive in each `--compress` pack.

## The Repomix problem found

| Repo | What is counted | In the source | In the `--compress` pack |
| --- | --- | --- | --- |
| ky | `#private` methods | 28 | 0 |
| requests | `def` (functions and methods) | 268 | 186 |
| diff-tour | `func` declarations | 232 | 231 |

- [`repro/priv.ts`](repro/priv.ts) and [`repro/priv.js`](repro/priv.js): `async #send()` and `static #make()` are
  missing from `--compress` output; public and TypeScript `private` methods are kept. The TypeScript and
  JavaScript tree-sitter queries match `method_definition` only when the name is a `property_identifier`, and a
  `#name` is a `private_property_identifier`. [`repro/private-methods.patch`](repro/private-methods.patch) adds it
  (`[(property_identifier) (private_property_identifier)]`); with it, all 28 ky methods come back for 1.8 KB more.
  The query is the same on Repomix's main branch as of 2026-10-24.
- requests: the multi-line Python signature bug from the
  [2026-10-19 project](../2026-10-19-repomix-compress/) still applies; `Session.request`,
  `merge_environment_settings` and `resolve_redirects` are among the 82 missing definitions.

## How to run it

Needs Node 18+, Python 3.10+, `git`, and Claude Code logged in (the runs used a subscription; `claude -p` reported
USD 10.84 at API prices for all 100 runs).

```bash
npm install                      # repomix 1.18.1
mkdir -p repos && cd repos
git clone https://github.com/sindresorhus/ky && git -C ky checkout 3541888
git clone https://github.com/psf/requests && git -C requests checkout 611c616
git clone https://github.com/init-co-jp/diff-tour && git -C diff-tour checkout 268ffd0
cd ..
./pack.sh repos packs
python3 run.py packs results/runs.jsonl --reps 2 --workers 2          # 90 runs, about 8 minutes
# optional: the patched pack (apply repro/private-methods.patch to node_modules/repomix first)
npx repomix repos/ky --include "source/**" --compress --style xml --no-security-check -o packs/ky-compress-patched.xml
python3 run.py packs results/runs.jsonl --reps 2 --conditions compress-patched
python3 blind.py results/runs.jsonl sheet.md key.json                # then grade sheet.md by hand
python3 summarize.py results > results/summary.md
python3 analyze_packs.py repos packs
```

`run.py` appends to its output and skips runs already there, so it can be restarted.

## Limits

- One model (Sonnet 5.5), two runs per question, 15 questions, one grader. The grader is the agent that wrote the
  questions and ran the benchmark; blinding was partial, because many answers say "the bodies are compressed" or
  "from memory".
- Checklist points are coarse: an answer that names a function in the right place gets the point even when it
  cannot show the code. Some `--compress` answers scored higher than the full pack on one question (diff-tour-flow
  10 vs 8); differences of 1 or 2 points per question are noise.
- Packs cover source folders only (no tests, docs or README). diff-tour's README explains its design and would
  have helped all conditions.

License: MIT. The packed repos are not included; `pack.sh` rebuilds them from the commits above.
