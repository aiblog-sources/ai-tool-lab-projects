#!/usr/bin/env bash
# Stops and removes everything the tests started. Safe to run more than once.
BASE="${BASE:-$HOME/.cache/agent-schedulers}"
UNITS="$HOME/.config/systemd/user"

[ -f "$BASE/gateway.pid" ] && kill "$(cat "$BASE/gateway.pid")" 2>/dev/null
[ -f "$BASE/llama-server.pid" ] && kill "$(cat "$BASE/llama-server.pid")" 2>/dev/null
[ -f "$BASE/sample.pid" ] && kill "$(cat "$BASE/sample.pid")" 2>/dev/null
docker rm -f ats-cron > /dev/null 2>&1
systemctl --user stop ats-stamp.timer ats-plain.timer ats-persist.timer 2>/dev/null
rm -f "$UNITS"/ats-plain.* "$UNITS"/ats-persist.*
systemctl --user daemon-reload
systemctl --user reset-failed 'ats-*' 2>/dev/null
echo "stopped"
