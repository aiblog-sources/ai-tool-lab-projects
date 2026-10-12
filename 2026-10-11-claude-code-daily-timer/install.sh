#!/usr/bin/env bash
# Installs a systemd user timer that runs ./run.sh every day at 09:00 local time.
set -euo pipefail
NAME=daily-agent
UNIT_DIR="$HOME/.config/systemd/user"
RUN="$(cd "$(dirname "$0")" && pwd)/run.sh"
mkdir -p "$UNIT_DIR/$NAME.service.d"

cat > "$UNIT_DIR/$NAME.service" <<UNIT
[Unit]
Description=$NAME: unattended Claude Code run

[Service]
Type=oneshot
ExecStart=$RUN
TimeoutStartSec=4h
UNIT

# Memory guard: past MemoryHigh the kernel reclaims aggressively; past MemoryMax it kills the
# largest process in this unit only. OOMPolicy=continue keeps the agent alive to report it.
cat > "$UNIT_DIR/$NAME.service.d/limits.conf" <<UNIT
[Service]
MemoryHigh=8G
MemoryMax=10G
OOMPolicy=continue
UNIT

cat > "$UNIT_DIR/$NAME.timer" <<UNIT
[Unit]
Description=Run $NAME daily at 09:00

[Timer]
OnCalendar=*-*-* 09:00:00
Persistent=true
RandomizedDelaySec=5min

[Install]
WantedBy=timers.target
UNIT

systemctl --user daemon-reload
systemctl --user enable --now "$NAME.timer"
systemctl --user list-timers "$NAME.timer" --no-pager
