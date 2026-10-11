#!/usr/bin/env bash
# What happens to runs that were due while the scheduler was down (laptop off, daemon crashed)?
# Stops every scheduler for DOWN seconds, starts them again, and logs what fired in the following minutes.
# systemd is tested twice: a plain timer and one with Persistent=true.
# Run after start-schedulers.sh. Results go to $BASE/results/downtime.txt and stamps.log.
set -euo pipefail

BASE="${BASE:-$HOME/.cache/agent-schedulers}"
DOWN="${DOWN:-250}"
export HERMES_HOME="$BASE/hermes-home"
UNITS="$HOME/.config/systemd/user"
out="$BASE/results/downtime.txt"

# Two file-based timers (transient ones are deleted when stopped, so they can't be "restarted").
for kind in plain persist; do
  persistent=false; [ "$kind" = persist ] && persistent=true
  cat > "$UNITS/ats-$kind.service" <<EOF
[Service]
Type=oneshot
Environment=STAMP_LOG=$BASE/results/stamps.log
ExecStart=$BASE/stamp.sh systemd-$kind
EOF
  cat > "$UNITS/ats-$kind.timer" <<EOF
[Timer]
OnCalendar=*:*:00
AccuracySec=1s
Persistent=$persistent
EOF
done
systemctl --user daemon-reload
systemctl --user stop ats-stamp.timer 2>/dev/null || true
systemctl --user start ats-plain.timer ats-persist.timer

# Let both timers fire at least once, so the persistent one has a "last triggered" stamp on disk.
until [ "$(grep -c systemd-persist "$BASE/results/stamps.log")" -ge 2 ]; do sleep 5; done
sleep 5

down_at=$(date +%s.%N)
kill "$(cat "$BASE/gateway.pid")"
docker stop -t 2 ats-cron > /dev/null
systemctl --user stop ats-plain.timer ats-persist.timer
echo "down_at $down_at" > "$out"

sleep "$DOWN"

up_at=$(date +%s.%N)
nohup "$BASE/venv/bin/hermes" gateway run >> "$BASE/results/gateway.log" 2>&1 &
echo $! > "$BASE/gateway.pid"
docker start ats-cron > /dev/null
systemctl --user start ats-plain.timer ats-persist.timer
echo "up_at $up_at" >> "$out"

sleep 150
"$BASE/venv/bin/hermes" cron runs stamp --limit 10 >> "$out" 2>&1 || true
echo "Done. Stamps between down_at and up_at+150 s show who caught up."
