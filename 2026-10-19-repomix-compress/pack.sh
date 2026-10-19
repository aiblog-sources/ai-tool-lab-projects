#!/usr/bin/env bash
# Pack the source folders of the three repos three ways with Repomix.
# usage: ./pack.sh REPOS_DIR OUT_DIR   (REPOS_DIR holds ky/, requests/, cobra/ checkouts)
set -euo pipefail
repos=$1; out=$2; mkdir -p "$out"
RM=${REPOMIX:-npx repomix}
declare -A INC=([ky]="source/**" [requests]="src/**" [cobra]="*.go")
declare -A IGN=([ky]="" [requests]="" [cobra]="*_test.go")
for r in ky requests cobra; do
  ign=()
  [ -n "${IGN[$r]}" ] && ign=(--ignore "${IGN[$r]}")
  for mode in full nocomments compress; do
    case $mode in
      full) flags=() ;;
      nocomments) flags=(--remove-comments --remove-empty-lines) ;;
      compress) flags=(--compress) ;;
    esac
    $RM "$repos/$r" --include "${INC[$r]}" "${ign[@]}" "${flags[@]}" \
      --style xml --no-security-check --quiet -o "$out/$r-$mode.xml"
    echo "$r $mode $(wc -c < "$out/$r-$mode.xml") bytes"
  done
done
