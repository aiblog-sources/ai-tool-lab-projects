#!/usr/bin/env bash
# Starts three schedulers that all run stamp.sh every minute, so their firing times can be compared:
#   1. Hermes Agent cron (no-agent script job, executed by `hermes gateway run`)
#   2. a transient systemd user timer (OnCalendar=*:*:00, AccuracySec=1s)
#   3. classic cron: busybox crond in a throwaway Alpine container (leaves your own crontab alone)
# Then samples RAM and CPU of the long-running daemons with sample.sh.
# Stop everything with ./stop-schedulers.sh
set -euo pipefail

BASE="${BASE:-$HOME/.cache/agent-schedulers}"
HERE="$(cd "$(dirname "$0")" && pwd)"
export HERMES_HOME="$BASE/hermes-home"
mkdir -p "$BASE/results" "$HERMES_HOME/scripts"
cp "$HERE/stamp.sh" "$BASE/stamp.sh"

# Hermes only runs scripts that live under $HERMES_HOME/scripts.
cat > "$HERMES_HOME/scripts/stamp-hermes.sh" <<EOF
#!/bin/sh
STAMP_LOG=$BASE/results/stamps.log $BASE/stamp.sh hermes
EOF
chmod +x "$HERMES_HOME/scripts/stamp-hermes.sh"
"$BASE/venv/bin/hermes" cron create "* * * * *" --no-agent --script stamp-hermes.sh --deliver local --name stamp

nohup "$BASE/venv/bin/hermes" gateway run > "$BASE/results/gateway.log" 2>&1 &
echo $! > "$BASE/gateway.pid"

systemd-run --user --unit=ats-stamp --on-calendar='*:*:00' --timer-property=AccuracySec=1s \
  --setenv=STAMP_LOG="$BASE/results/stamps.log" "$BASE/stamp.sh" systemd

# busybox `date` has no %N, so the container installs coreutils first.
docker run -d --name ats-cron -v "$BASE/results:/out" -v "$BASE/stamp.sh:/stamp.sh:ro" alpine:3.20 \
  sh -c 'apk add -q coreutils && echo "* * * * * /stamp.sh cron" > /etc/crontabs/root && exec crond -f -l 8'

nohup "$HERE/sample.sh" "$BASE" > /dev/null 2>&1 &
echo $! > "$BASE/sample.pid"
echo "Running. Let it go for 20 minutes, then: python3 analyze.py $BASE/results"
