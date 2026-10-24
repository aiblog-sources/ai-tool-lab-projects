#!/usr/bin/env bash
# Pack the source of the three repos two ways with Repomix: full and --compress.
# usage: ./pack.sh REPOS_DIR OUT_DIR   (REPOS_DIR holds ky/, requests/, diff-tour/ checkouts)
set -euo pipefail
repos=$1; out=$2; mkdir -p "$out"
RM=${REPOMIX:-npx repomix}
declare -A INC=([ky]="source/**" [requests]="src/**" [diff-tour]="**/*.go")
declare -A IGN=([ky]="" [requests]="" [diff-tour]="**/*_test.go")
for r in ky requests diff-tour; do
  ign=()
  [ -n "${IGN[$r]}" ] && ign=(--ignore "${IGN[$r]}")
  for mode in full compress; do
    case $mode in
      full) flags=() ;;
      compress) flags=(--compress) ;;
    esac
    $RM "$repos/$r" --include "${INC[$r]}" "${ign[@]}" "${flags[@]}" \
      --style xml --no-security-check --quiet -o "$out/$r-$mode.xml"
    echo "$r $mode $(wc -c < "$out/$r-$mode.xml") bytes"
  done
done
