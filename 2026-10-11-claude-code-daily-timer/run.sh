#!/usr/bin/env bash
# Runs Claude Code headless once, with guards that make an unattended run safe to repeat.
# Configure the variables below, then install the timer with ./install.sh.
set -euo pipefail

# --- configuration -----------------------------------------------------------
NAME=daily-agent                                   # used for the state dir and notifications
WORKDIR="$HOME/projects/my-repo"                   # repo the agent starts in
EXTRA_DIRS=()                                      # more repos it may edit, e.g. ("$HOME/projects/other")
PROMPT_FILE="$WORKDIR/scripts/agent/prompt.md"     # instructions for each run, kept in the repo
MODEL=claude-opus-5-5
# ------------------------------------------------------------------------------

STATE="${XDG_STATE_HOME:-$HOME/.local/state}/$NAME"
LOG="$STATE/logs/$(date +%F_%H%M).jsonl"
# systemd starts with a minimal PATH: add wherever node, claude and gh live on your machine.
export PATH="$HOME/.local/bin:/usr/local/bin:/usr/bin:/bin:$PATH"
mkdir -p "$STATE/logs"
exec > >(tee -a "$LOG") 2>&1

# One run at a time.
exec 9>"$STATE/run.lock"
flock -n 9 || { echo "Another run is in progress, skipping."; exit 0; }

notify() { command -v notify-send >/dev/null && notify-send -a "$NAME" "$1" "$2" || true; }

# Never clobber human work: skip the day if any repo has local changes.
for repo in "$WORKDIR" "${EXTRA_DIRS[@]}"; do
  if [ -n "$(git -C "$repo" status --porcelain)" ]; then
    notify "Run skipped" "Uncommitted changes in $repo"
    echo "Uncommitted changes in $repo, skipping."
    exit 0
  fi
  git -C "$repo" checkout -q main
  git -C "$repo" pull -q --ff-only
done

add_dirs=()
for d in "${EXTRA_DIRS[@]}"; do add_dirs+=(--add-dir "$d"); done

cd "$WORKDIR"
status=0
# stream-json writes every step as it happens, so a run that dies still leaves a useful log.
claude -p "$(cat "$PROMPT_FILE")" \
  --model "$MODEL" \
  --permission-mode auto \
  "${add_dirs[@]}" \
  --output-format stream-json --verbose || status=$?

if [ "$status" -eq 0 ]; then notify "Run finished" "Log: $LOG"; else notify "Run failed ($status)" "Log: $LOG"; fi
exit "$status"
