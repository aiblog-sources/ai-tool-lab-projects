# Claude Code on a daily timer

Run Claude Code headless once a day from a systemd user timer, with the guards that the first real run of this
setup showed were missing. This is the setup behind [AI Tool Labs](https://aitoollabs.dev)'s daily routine.

Write-up: https://aitoollabs.dev/posts/claude-code-daily-timer-day-one/

## What it does

- `install.sh` creates a systemd **user** timer: every day at 09:00, `Persistent=true` (a day missed while the
  machine was off runs right after the next login), plus a memory limit for the whole run.
- `run.sh` runs Claude Code once with `--permission-mode auto`, after these checks:
  - **one run at a time** (`flock`)
  - **skip the day if any repo has uncommitted changes**, so it never mixes its work with yours
  - **fresh `main`** in every repo (`git pull --ff-only`)
  - **streamed log** (`--output-format stream-json`), so a run that dies still leaves a trace
  - a desktop notification at the end
- `prompt.example.md` is a starting point for the instructions each run receives.

## Use it

Requirements: Linux with systemd, Claude Code logged in on the machine (`claude` works in a terminal), git.

1. Read `run.sh` and `install.sh`. They are short, and they start an agent that acts without you watching.
2. Edit the configuration block at the top of `run.sh` (`WORKDIR`, `EXTRA_DIRS`, `PROMPT_FILE`, `MODEL`) and make
   sure the `PATH` line includes where `node`, `claude` and `gh` live on your machine.
3. Put your prompt at `PROMPT_FILE`, starting from `prompt.example.md`.
4. Try one run by hand: `./run.sh`
5. Install the timer: `./install.sh`

Stop it any time: `systemctl --user disable --now daily-agent.timer`

## Notes from running it

- `--permission-mode auto` lets routine work through and blocks risky actions. In this setup it blocked the
  agent from committing its own scheduling config, merging its own pull requests, and rewriting git history,
  until the owner either ran those steps or added an explicit allow rule. That is the behavior you want.
- The memory limit exists because the first run was killed by the kernel's OOM killer after 42 minutes: a local
  inference server started with default settings reserved far more memory than the model needed.

License: MIT.
