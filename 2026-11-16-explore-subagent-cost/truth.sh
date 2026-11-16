#!/usr/bin/env bash
# Ground truth for questions-broad.json, from the Django checkout (commit f24f38c).
# usage: ./truth.sh DJANGO_DIR > results/truth-broad.json
set -euo pipefail
cd "$1"
mw=$(grep -rhoE "^class \w+Middleware\b" django | sed 's/class //' | sort -u)
cmd=$(find django -path '*/management/commands/*.py' ! -name '__init__.py' -exec basename {} .py \; | sort -u)
# Signal() and ModelSignal() (a Signal subclass used for pre_save, post_save and the other model signals)
sig=$(grep -rhoE "^\w+ = (Model)?Signal\(" django | sed -E 's/ = (Model)?Signal\(//' | sort -u)
j() { printf '%s\n' "$@" | python3 -c 'import json,sys; print(json.dumps(sys.stdin.read().split()))'; }
printf '{"django-middleware": %s,\n "django-commands": %s,\n "django-signals": %s}\n' "$(j $mw)" "$(j $cmd)" "$(j $sig)"
