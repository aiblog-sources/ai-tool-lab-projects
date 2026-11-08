#!/usr/bin/env bash
# Type-check hook for Claude Code. Usage: tsc-check.sh <post|stop> <exit code on type errors>
#   post: PostToolUse on Edit|Write|MultiEdit, only runs when the edited file is .ts
#   stop: Stop hook, runs every time Claude tries to finish
# On type errors it prints the first 30 lines of tsc output to stderr and exits with the code given as $2.
event="$1"; fail_code="${2:-2}"
input="$(cat)"
if [ "$event" = post ]; then
  file="$(printf '%s' "$input" | node -e 'let s="";process.stdin.on("data",d=>s+=d).on("end",()=>{const j=JSON.parse(s);console.log((j.tool_input&&j.tool_input.file_path)||"")})')"
  case "$file" in *.ts) ;; *) exit 0 ;; esac
fi
cd "$CLAUDE_PROJECT_DIR" || exit 0
start=$(date +%s%N)
out="$(./node_modules/.bin/tsc --noEmit --pretty false 2>&1)"; status=$?
ms=$(( ($(date +%s%N) - start) / 1000000 ))
errors=$(printf '%s\n' "$out" | grep -c 'error TS')
# The bench sets HOOK_LOG (outside the work folder) to count firings; a normal install can leave it unset.
[ -n "$HOOK_LOG" ] && printf '{"event":"%s","errors":%d,"ms":%d}\n' "$event" "$errors" "$ms" >> "$HOOK_LOG"
if [ $status -ne 0 ]; then
  { echo "TypeScript check failed ($errors errors). Fix them before continuing:"; printf '%s\n' "$out" | head -30; } >&2
  exit "$fail_code"
fi
exit 0
