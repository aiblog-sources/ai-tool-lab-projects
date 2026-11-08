# Does a tsc hook make Claude Code leave fewer TypeScript errors?

**Finding:** on six ordinary refactors of a 53-file strict TypeScript project, Claude Code left type errors in 1 of
240 runs (that run had no hook): Haiku 5.5 and Sonnet 5.5 found the call sites themselves. The hook mattered on a
bigger request, all six changes in one prompt with no shell: Sonnet edited only the core files and left 148 errors in
9 of 9 runs without a blocking hook, and 0 in 6 of 6 runs with a PostToolUse or Stop hook that exits 2. A hook that
exits 1 never reached Claude: all 1,078 of its error reports were stored as non-blocking and not shown to the model.

Write-up: https://aitoollabs.dev/posts/claude-code-tsc-hook/

All six changes in one prompt, 3 runs per cell (tsc errors left in each run):

| Model, tools | none | claude-md | post-exit1 | post-exit2 | stop-exit2 |
| --- | --- | --- | --- | --- | --- |
| Sonnet 5.5, no shell | 148, 148, 148 | 148, 148, 148 | 148, 148, 148 | 0, 0, 0 | 0, 0, 0 |
| Haiku 5.5, no shell | 0, 0, 0 | 0, 148, 0 | 0, 0, 8 | 0, 0, 0 | 0, 0, 0 |
| Sonnet 5.5, Bash allowed | 0, 0, 0 | 0, 0, 0 | 0, 0, 66 | 0, 0, 0 | 0, 0, 0 |
| Haiku 5.5, Bash allowed | 0, 0, 0 | 0, 0, 0 | 0, 0, 0 | 0, 0, 0 | 0, 0, 0 |

On the six single tasks (240 runs, Bash allowed and no shell, 2 runs per cell) every run passed its hidden tests and
239 ended with 0 type errors. Full tables: `results/summary.md`. `tsc --noEmit` on this project takes 0.18 s with
TypeScript 7.0.2 and 1.29 s with 5.9.3 (`results/tsc-timing.txt`). Note that tsc 7.0.2 exits 1 on type errors and
5.9.3 exits 2, so a hook that passes tsc's exit status through stops reaching Claude after the TypeScript 7 upgrade.

## What is here

| Path | What it is |
| --- | --- |
| `hooks/tsc-check.sh` | The hook. `tsc-check.sh post 2` for PostToolUse (only runs when the edited file is `.ts`), `tsc-check.sh stop 2` for Stop. Prints the first 30 lines of `tsc --noEmit` to stderr and exits with the code you pass. |
| `fixture/` | The test project: 13 hand-written core files (types, store, pricing, reports, CLI) and 40 modules generated from templates by `bench/gen_fixture.py`, TypeScript 7.0.2, `strict` and `noUncheckedIndexedAccess`. |
| `bench/tasks.json`, `bench/task-combo.json` | The six refactor prompts, and the combo prompt that asks for all six at once. |
| `hidden/` | Acceptance tests per task (`node --test`), never copied into the work folder. |
| `bench/conditions.json` | none, claude-md (an instruction to run `npx tsc --noEmit`), post-exit1, post-exit2, stop-exit2. |
| `bench/run.py` | Copies the fixture, adds the condition, runs `claude -p`, then grades: tsc errors left and the hidden tests. Reads the session transcript only to count hook attachment types. |
| `bench/ripple.sh` | How many tsc errors each task's core change causes before any call site is fixed. |
| `bench/summarize.py` | Writes `results/summary.md`. |
| `bench/scrub.py` | Replaces the home folder path with `~` in result files before publishing. |
| `results/` | `runs.jsonl` (240 runs), `combo-runs.jsonl` (60 runs; Stop hook feedback is counted from `hook_runs`, the transcript counter only matches PostToolUse attachments), `summary.md`, `ripple.txt`, `tsc-timing.txt`. |

## Install the hook in your project

Copy `hooks/tsc-check.sh` to `.claude/hooks/`, make it executable, and add to `.claude/settings.json`:

```json
{
  "hooks": {
    "Stop": [
      { "hooks": [{ "type": "command", "command": "\"$CLAUDE_PROJECT_DIR\"/.claude/hooks/tsc-check.sh stop 2", "timeout": 60 }] }
    ]
  }
}
```

The Stop version gave the same result as the PostToolUse version in these runs at about half the cost (see the
post). The script calls `./node_modules/.bin/tsc`, so TypeScript must be a dev dependency. `HOOK_LOG` is only used by
the benchmark.

## Run the benchmark

Needs Node 22, Python 3 (standard library only) and Claude Code logged in. Each run is a `claude -p` call billed to
your account or subscription.

```bash
cd fixture && npm install && cd ..
bench/ripple.sh /tmp/tsc-ripple
# The scratch folder must be outside your home folder, or Claude Code loads ~/.claude/CLAUDE.md into every run
# even with --setting-sources project.
WORKERS=5 python3 bench/run.py /tmp/tsc-hook-main results/runs.jsonl
TASKS=combo REPS=3 WORKERS=4 python3 bench/run.py /tmp/tsc-hook-combo results/combo-runs.jsonl
python3 bench/summarize.py results/runs.jsonl results/combo-runs.jsonl > results/summary.md
```

`MODELS`, `CONDITIONS`, `TASKS`, `SHELLS` (`shell`, `noshell`), `REPS` and `WORKERS` limit or size a run.

## Versions

Claude Code 2.1.319, Claude Haiku 5.5 and Sonnet 5.5, TypeScript 7.0.2 (5.9.3 for one timing), Node 22.14, a laptop
with a 4-thread CPU and 19 GB of RAM.
