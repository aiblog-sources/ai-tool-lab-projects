# Does Claude Code's Explore subagent save tokens? 156 headless runs

**Finding:** on 15 "how does this code work" questions about three real repos, asking Claude Code (Sonnet 5.5) to use
its built-in Explore subagent cost **2.7x as much** ($2.81 vs $1.03 for 30 runs) and took **2.9x as long** (median
23 s vs 8 s) as letting it answer on its own, and the main conversation did **not** end up smaller (median 20,380 vs
20,446 tokens). Explore ran on Sonnet, not Haiku: in Claude Code 2.1.325 it inherits the main model. Forcing it onto
Haiku 5.5 (`CLAUDE_CODE_SUBAGENT_MODEL` + `CLAUDE_CODE_SUBAGENT_MODEL_FORCE=1`) brought the cost back to about the
direct run ($1.13) but was the slowest (median 32 s). Left alone, Claude never chose Explore: 0 of 39 runs. Answers
with Explore scored a little higher (135 and 134 of 150 checklist points vs 130 and 128), with 0 wrong claims vs 2.

Write-up: https://aitoollabs.dev/posts/claude-code-explore-subagent-cost/

## Results

Main set: 15 architecture questions (5 each on ky, requests and diff-tour, the question set and 5-point checklists
from [2026-10-24-repomix-compress-architecture](../2026-10-24-repomix-compress-architecture/)), 2 runs per question
and condition, main model `claude-sonnet-5-5`. Costs are Claude Code's own `total_cost_usd` (API list prices; the runs
used a subscription). "Main context at end" is the input of the main conversation's last model call.

| Condition | Explore used | Cost, 30 runs | Median wall time | Main context at end | Checklist points | Wrong claims |
| --- | --- | --- | --- | --- | --- | --- |
| `no-subagent` (`--disallowedTools Agent`) | 0/30 | $0.91 | 8 s | 17,973 | 130/150 | 2 |
| `default` (Claude decides) | 0/30 | $1.03 | 8 s | 20,446 | 128/150 | 2 |
| `explore` (prompt asks for Explore) | 30/30 | $2.81 | 23 s | 20,380 | 135/150 | 0 |
| `explore-haiku` (same, subagent forced to Haiku 5.5) | 30/30 | $1.13 | 32 s | 20,118 | 134/150 | 0 |

- The Agent tool's definition costs about **2,900 tokens on every main-model call**: the first call's input was
  14,556 tokens (median) with `--disallowedTools Agent` and 17,447 with it available, whether or not it was used.
- The Explore subagent read more than the main agent did on its own: 3.9 tool calls per run on Sonnet and 7.9 on Haiku,
  against 2.1 for the main agent answering directly. In 4 of 60 direct runs Claude answered the requests question
  from memory without opening a file.
- `explore-haiku` spent $0.97 on Sonnet (the main conversation) and only $0.16 on Haiku for all 30 runs.
- In 57 of 60 Explore runs the subagent ran in the background: the main turn ended first ("still searching"), and a
  second turn wrote the answer when the task notification arrived. `claude -p` waits for it.

Broad set: three list questions on Django (`f24f38c`, 909 Python files), every middleware class, management command
and Signal in `django/`, 3 runs per condition, scored against `truth.sh` (grep over the checkout).

| Condition | Items found | Extra items | Cost, 9 runs | Median wall time | Main context at end |
| --- | --- | --- | --- | --- | --- |
| `no-subagent` | 237/237 | 0 | $0.17 | 8 s | 15,457 |
| `default` | 237/237 | 0 | $0.20 | 8 s | 18,267 |
| `explore` | 236/237 | 4 | $0.50 | 16 s | 19,694 |
| `explore-haiku` | 237/237 | 3 | $0.30 | 21 s | 19,693 |

The extras were `MiddlewareMixin` and the private `_EnsureCsrfToken` / `_EnsureCsrfCookie` subclasses in
`django/views/decorators/csrf.py`, plus one line where the main agent "corrected" the subagent from memory
(`MiddlewareMixin` in `django.utils.deprecation`; at this commit it is in `django/middleware/__init__.py`, as the
subagent had said).

Full tables: `results/summary.md`, `results/summary-broad.md`. Grades: `results/grades.psv` (one line per answer,
with a note on each missed point) and `results/grading-key.json` (label to run).

## How to run it

Needs Claude Code (tested with 2.1.325) logged in, Python 3.10+, git.

```sh
mkdir -p repos && cd repos
git clone https://github.com/sindresorhus/ky.git && git -C ky checkout 3541888
git clone https://github.com/psf/requests.git && git -C requests checkout 611c616
git clone https://github.com/init-co-jp/diff-tour.git && git -C diff-tour checkout 268ffd0
git clone https://github.com/django/django.git && git -C django checkout f24f38c
cd ..

python3 run.py repos streams results/runs.jsonl                     # 120 runs, about 12 minutes with 3 workers
python3 run.py repos streams-broad results/runs-broad.jsonl --questions questions-broad.json --reps 3
./truth.sh repos/django > results/truth-broad.json
python3 blind.py results/runs.jsonl results/grading-sheet.md results/grading-key.json   # hand grading, condition hidden
python3 summarize.py results/runs.jsonl results/grades.psv results/grading-key.json
python3 score_broad.py results/runs-broad.jsonl results/truth-broad.json
```

`run.py` keeps each raw `stream-json` transcript in the streams folder (not published here: the init event lists local
paths) and appends one summary line per run, so a stopped run can be restarted.

Every run is `claude -p --model claude-sonnet-5-5 --strict-mcp-config --setting-sources "" --output-format stream-json
--verbose`, with `CLAUDE_CODE_DISABLE_CLAUDE_MDS=1` and `CLAUDE_CODE_DISABLE_AUTO_MEMORY=1`, in the repo checkout,
default permission mode (read-only shell commands are allowed without prompts).

## Limits

- One main model (Sonnet 5.5), one version (2.1.325), single-turn questions. Explore's selling point is keeping a long
  session's context clean; a one-question run can only show what one delegation costs, not what it saves twenty turns
  later.
- Small-to-medium repos and focused questions. Answering directly needed about 2 shell calls, so there was little to
  keep out of the main context.
- One grader (the agent that ran the benchmark), blind to the condition label, but Explore answers often say "the
  Explore agent read..." so the blinding is partial. The point differences (5 to 7 of 150) are within what one
  grader's judgment can move.
- See [env.md](results/env.md) for versions and dates.

MIT license. Question checklists reuse the 2026-10-24 project. ky (MIT), requests (Apache 2.0), diff-tour (MIT) and
Django (BSD-3-Clause) were only read, not redistributed.
