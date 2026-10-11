# Scheduling AI agents: Hermes Agent cron vs systemd timers vs cron

The same jobs scheduled three ways on one Linux laptop (2-core CPU, 19 GB RAM, no GPU), measured side by side:

1. **Hermes Agent's built-in cron** (Nous Research, MIT), run by its gateway daemon
2. **a systemd user timer**
3. **classic cron** (busybox `crond` in a throwaway Alpine container, so your own crontab is never touched)

Write-up: https://aitoollabs.dev/posts/hermes-agent-cron-vs-systemd-timers/

## What is measured

| Test | Script | What it shows |
| --- | --- | --- |
| Firing time | `start-schedulers.sh`, `stamp.sh`, `analyze.py` | How many seconds after the scheduled minute each job really runs (20 minutes, one job per minute) |
| Idle cost | `sample.sh` | RAM and CPU of the daemons that must stay running |
| Downtime | `downtime-test.sh` | What happens to runs that were due while the scheduler was down (4 minutes), with systemd tested with and without `Persistent=true` |
| Agent job, cloud model | `agent-claude.sh` | A small file task run by Claude Code headless from `systemd-run` |
| Agent job, local model | `agent-hermes.sh` | The same task as a Hermes cron job on llama.cpp (CPU only) |
| Hybrid | see below | Hermes schedules and stores the output, a script calls Claude Code |

The agent task (`agent-prompt.txt`, `make-task.sh`): count the `.md` files in a folder with 7 Markdown files and 2
other files, and write the number to `count.txt`. The right answer is 7, so success is checkable.

## Results from the run (2026-10-11)

Hermes Agent commit `97a9ae8` (reports version 2026.9.24), Python 3.14 via uv, systemd 255, busybox crond from
Alpine 3.20. Raw data is in `results/`.

| | systemd timer | cron | Hermes cron |
| --- | --- | --- | --- |
| Runs in 20 to 21 minutes | 21 of 21 | 21 of 21 | 20 of 20 |
| Median delay after the scheduled minute | 0.42 s | 0.91 s | 49.11 s |
| Extra daemon RAM | none (uses the user manager already running) | 0.8 MB | 230.5 MB median |
| Extra daemon CPU | none | 0.1 CPU-seconds per hour | 54 CPU-seconds per hour |
| Missed runs after 4 min down | 1 catch-up run with `Persistent=true`, 0 without | 0 | 1 catch-up run |

The Hermes delay is not random: its gateway checks for due jobs every 60 seconds, counted from when the gateway
started. After a restart the same job fired 27.8 s after the minute instead of 49.1 s.

Agent job:

| | Result | Time |
| --- | --- | --- |
| `claude -p` (Haiku) from `systemd-run`, 4 runs | 4 of 4 correct | 7.2 to 8.0 s |
| Hermes cron, Qwen 2.5 1.5B on llama.cpp at 32K context | refused before starting: Hermes needs at least 64K | n/a |
| Hermes cron, same model stretched to 64K with YaRN | task not done (wrong path), but the run was recorded as `ok` | 624 s, of which 583 s reading a 13,407-token prompt |
| Hermes cron, Llama 3.2 3B at 64K (8-bit KV cache) | not run: llama-server reached 7.3 GB, over this project's 6 GB budget | n/a |
| Hermes `--no-agent` job that calls `claude -p` | correct | 12.6 s |

## Run it yourself

Requirements: Linux with systemd (user session), Docker, git, curl, bc, Python 3. For the agent tests: Claude Code
logged in, and/or a llama.cpp build plus a GGUF model.

```bash
./setup-hermes.sh          # isolated install under ~/.cache/agent-schedulers (override with BASE=...)
./start-schedulers.sh      # starts all three, plus the resource sampler; wait about 20 minutes
python3 analyze.py ~/.cache/agent-schedulers/results
./downtime-test.sh         # about 8 minutes
./agent-claude.sh
LLAMA_SERVER=/path/to/llama-server MODEL=/path/to/model.gguf ./agent-hermes.sh
./stop-schedulers.sh       # stops and removes everything the tests started
```

Hybrid job (Hermes runs any script under `$HERMES_HOME/scripts/`):

```bash
cat > "$HERMES_HOME/scripts/claude-count.sh" <<'EOF'
#!/bin/bash
cd /path/to/agent-task
claude -p "$(cat /path/to/agent-prompt.txt)" --model claude-haiku-5-5 --permission-mode auto --output-format text
EOF
chmod +x "$HERMES_HOME/scripts/claude-count.sh"
hermes cron create "0 9 * * *" --no-agent --script claude-count.sh --deliver local --name hybrid
```

## Notes

- `pkill -f <pattern>` inside a shell command that contains the same pattern kills that shell too. The stop script
  uses PID files for that reason.
- busybox `date` has no `%N`, so the cron container installs coreutils.
- `systemd-run -p StandardOutput=file:` overwrites without truncating; use `truncate:` for files read as JSON.
- llama-server caps each slot at the model's training context (32,768 for Qwen 2.5 1.5B) even when `-c` asks for
  more; the 64K run needed `--rope-scaling yarn --rope-scale 2 --yarn-orig-ctx 32768` and a context length override.

License: MIT.
