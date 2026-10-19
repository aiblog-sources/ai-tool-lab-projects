# Does Repomix --compress hurt Claude's answers?

**Finding:** on three real repos, Repomix `--compress` cut Claude's input by 12% to 61%, and Claude Sonnet 5.5
answered every question whose answer survived compression (34 of 34 runs). When compression removed the answer, it
never guessed wrong: it said "NOT IN CONTEXT" in 16 of 26 runs and worked the right answer out of surviving comments
in the other 10. Along the way I found three ways Repomix 1.18.1 loses or repeats code: Python functions with a
multi-line signature vanish under `--compress`, TypeScript doc comments are emitted twice, and `--remove-comments`
deleted 11 Python functions from requests.

Write-up: https://aitoollabs.dev/posts/repomix-compress-accuracy/

| Condition (30 questions x 2 runs) | Right | Wrong | "NOT IN CONTEXT" / "NOT SURE" |
| --- | --- | --- | --- |
| No repo at all (model memory) | 55 | 1 | 4 |
| Full pack | 60 | 0 | 0 |
| `--remove-comments --remove-empty-lines` | 58 | 0 | 2 |
| `--compress` | 44 | 0 | 16 |

| Repo (source only) | Full pack, Claude tokens | `--remove-comments` | `--compress` |
| --- | --- | --- | --- |
| ky (TypeScript) | 78,282 | 45,177 (0.58) | 69,082 (0.88) |
| requests (Python) | 81,097 | 59,232 (0.73) | 50,985 (0.63) |
| cobra (Go) | 89,558 | 72,393 (0.81) | 34,523 (0.39) |

Token counts are the median input tokens Claude Code reported per run, so they include about 650 tokens of prompt
and question. Full tables: [`results/summary.md`](results/summary.md) and [`results/packs.md`](results/packs.md).

## What it does

1. `pack.sh` packs the source folders of ky, requests and cobra three ways with Repomix 1.18.1: full, without
   comments and empty lines, and `--compress`.
2. `evidence.py` checks, for each of the 30 questions in [`questions.json`](questions.json), whether the text that
   answers it survives in each pack ([`results/evidence.csv`](results/evidence.csv)).
3. `run.py` asks Claude each question with each pack (and once with no repo, to see what the model already knows),
   twice, through `claude -p` with tools off. The prompt tells it to answer only from the repository and to reply
   "NOT IN CONTEXT" otherwise.
4. `summarize.py` grades the answers with a regex per question plus hand corrections in
   [`overrides.json`](overrides.json) (2 corrections, both grader misses), and prints the tables.
5. `analyze_packs.py` measures the packs themselves: o200k token counts, files that `--compress` made larger,
   repeated chunks, and how many function definitions survive.

The questions are short factual ones about the code: a default value, an error message, a constant, a branch. Half
of them have their answer in a declaration, signature or comment; the other half only in a function body.

## The Repomix problems found

Minimal inputs are in [`repro/`](repro/):

- `repro/multi.py`: a Python function whose `def` spans more than one line disappears from `--compress` output
  (only its docstring stays). The Python strategy matches the signature with a one-line regex. In requests, 70 of
  the 268 function definitions have a multi-line signature: 68 vanish, including `Session.request`, `get` and
  `post`, and 2 keep only their first line (`def get(  # type: ignore[override]`).
- `repro/opts.ts`: in TypeScript type literals and interfaces, `--compress` prints each member's doc comment a
  second time. ky's `source/types/*.ts` files came out 1.5 to 1.9 times *larger* than the originals; 27% of ky's
  compressed tokens are repeated chunks.
- `repro/client.py`: one-line Python signatures keep their default values (a control for the first case).
- Not reduced to a small input: `--remove-comments` on requests' `models.py` deleted the body of `iter_content`, all
  three `iter_lines` definitions and the `content` property header, while keeping the docstrings around them. In
  the whole package, 11 function definitions went missing. Go (cobra) and TypeScript (ky) lost none.

## How to run it

Needs Node 18+, Python 3.10+, `git`, and Claude Code logged in (the runs used a subscription; `claude -p` reported
USD 23.52 at API prices for all 240 runs). `analyze_packs.py` needs `tiktoken`.

```bash
npm install                      # repomix 1.18.1
mkdir -p repos && cd repos
git clone https://github.com/sindresorhus/ky && git -C ky checkout 3541888
git clone https://github.com/psf/requests && git -C requests checkout 611c616
git clone https://github.com/spf13/cobra && git -C cobra checkout adbc881
cd ..
./pack.sh repos packs
python3 evidence.py packs > results/evidence.csv
python3 run.py packs results/runs.jsonl --reps 2 --workers 2      # 240 claude -p runs, about 12 minutes
python3 summarize.py results/runs.jsonl > results/summary.md
python3 analyze_packs.py packs > results/packs.md
```

`run.py` appends to its output and skips runs already there, so it can be restarted. Claude Code updated itself
during the run; the launches that exited with no output were retried.

## Limits

- One model (Sonnet 5.5), two runs per question, 30 questions on three repos. Small, and the questions are lookup
  questions, not "explain this architecture" ones, which is where `--compress` is meant to shine.
- The model already knew most of requests and cobra (40 of 40 right with no repo). The "answer only from the
  repository" instruction held: with `--compress` it said "NOT IN CONTEXT" for facts it had answered from memory.
- Packs cover source folders only (no tests, docs or lockfiles).

License: MIT. The packed repos are not included; `pack.sh` rebuilds them from the commits above.
