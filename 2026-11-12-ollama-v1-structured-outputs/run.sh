#!/usr/bin/env bash
# For each model in models.tsv and each Ollama version: start a private Ollama (2 parallel slots, 6 GB memory cap),
# import the GGUF with a Modelfile that has only FROM (no PARAMETER lines, so Ollama's built-in defaults apply),
# record the sampler parameters each condition gets (bench.py --probe), run the response_format probe
# (probe_formats.py), run the benchmark conditions, stop the server.
# ONLY="<model name>" runs one model; VERSIONS="0.40.16" runs one version; LIMIT=<n> sends only the first n
# emails; REPS=<n> repeats per sampled condition.
set -euo pipefail
cd "$(dirname "$0")"
export LC_ALL=C
OHOME=${OHOME:-$PWD/vendor/home}
VERSIONS=${VERSIONS:-"0.40.13 0.40.16"}
ONLY=${ONLY:-}
LIMIT=${LIMIT:-0}
REPS=${REPS:-2}
RESUME=${RESUME:-}  # RESUME=1: skip the probes and the requests already in $OUT
OUT=${OUT:-results/raw.jsonl}
PORT=11500
# 0.40.13 is the last release before the /v1 sampler change (ollama/ollama c1db5f9, first in 0.40.14).
declare -A CONDS=(
  [0.40.13]="t0,none,schema,1.5,1.5-schema"
  [0.40.16]="t0,t0-schema,none,schema,object,schema-only,1.5,1.5-schema"
)
mkdir -p results/logs "$OHOME/models"
[ -f results/server.tsv ] || printf "version\tmodel\tstatus\tbackground_cpu_pct\tpeak_rss_mib\tminutes\n" > results/server.tsv

cpu_busy() {  # percent of all CPU time busy over 5 s
  read -r _ u n s i w q sq _ < /proc/stat; sleep 5
  read -r _ u2 n2 s2 i2 w2 q2 sq2 _ < /proc/stat
  local busy=$(( (u2+n2+s2+q2+sq2) - (u+n+s+q+sq) )) idle=$(( (i2+w2) - (i+w) ))
  echo $(( 100 * busy / (busy + idle) ))
}
cleanup() { pkill -f "$PWD/vendor/ollama-.*/bin/ollama serve" 2>/dev/null || true; }
trap cleanup EXIT
peak() { [ -r "/proc/$1/status" ] && awk '/VmHWM/ {printf "%.0f", $2/1024}' "/proc/$1/status"; }

tail -n +2 models.tsv | while IFS=$'\t' read -r name file _; do
  [ -n "$ONLY" ] && [ "$name" != "$ONLY" ] && continue
  slug=$(echo "$name" | tr 'A-Z .' 'a-z--')
  omodel=$(echo "$slug" | tr -d '-')
  for v in $VERSIONS; do
    OLLAMA=$PWD/vendor/ollama-$v/bin/ollama
    if curl -s "http://127.0.0.1:$PORT/" >/dev/null; then echo "port $PORT already in use" >&2; exit 1; fi
    bg=$(cpu_busy); t0=$(date +%s); log=results/logs/ollama-$v-$slug.log
    systemd-run --user --scope --quiet -p MemoryMax=6G -p MemorySwapMax=0 \
      --setenv=HOME="$OHOME" --setenv=OLLAMA_MODELS="$OHOME/models" --setenv=OLLAMA_HOST=127.0.0.1:$PORT \
      --setenv=OLLAMA_NUM_PARALLEL=2 "$OLLAMA" serve > "$log" 2>&1 &
    wrapper=$!; status=ok; pk=
    until curl -sf "http://127.0.0.1:$PORT/api/version" >/dev/null; do
      kill -0 "$wrapper" 2>/dev/null || { status=died; break; }; sleep 0.5
    done
    if [ "$status" = ok ]; then
      echo "FROM $PWD/models/$file" > "$OHOME/Modelfile-$omodel"
      OLLAMA_HOST=127.0.0.1:$PORT "$OLLAMA" create "$omodel" -f "$OHOME/Modelfile-$omodel" >/dev/null 2>&1
      if [ -z "$RESUME" ]; then
        python3 bench.py --base "http://127.0.0.1:$PORT" --model "$name" --served-model "$omodel" --version "$v" \
          --conds "${CONDS[$v]}" --probe --log "$log"
        vendor/venv/bin/python probe_formats.py --base "http://127.0.0.1:$PORT" --served-model "$omodel" --version "$v" \
          | tee -a results/format-probe.md
      fi
      python3 bench.py --base "http://127.0.0.1:$PORT" --model "$name" --served-model "$omodel" --version "$v" \
        --conds "${CONDS[$v]}" --out "$OUT" --limit "$LIMIT" --reps "$REPS" ${RESUME:+--resume} || status=bench-error
      rpid=$(pgrep -n -f "ollama-$v/lib/ollama/llama-server|ollama-$v/bin/ollama runner" || true)
      [ -n "$rpid" ] && pk=$(peak "$rpid")
      OLLAMA_HOST=127.0.0.1:$PORT "$OLLAMA" stop "$omodel" >/dev/null 2>&1 || true
      opid=$(pgrep -n -f "ollama-$v/bin/ollama serve" || true)
      [ -n "$opid" ] && kill "$opid"
      wait "$wrapper" 2>/dev/null || true
    fi
    while curl -s "http://127.0.0.1:$PORT/" >/dev/null; do sleep 0.5; done
    [ "$LIMIT" = 0 ] && printf "%s\t%s\t%s\t%s\t%s\t%.1f\n" "$v" "$name" "$status" "$bg" "$pk" \
      "$(echo "($(date +%s) - $t0) / 60" | bc -l)" | tee -a results/server.tsv
  done
done
sed -i -e "s#$OHOME#<ollama-home>#g" -e "s#$PWD/##g" -e "s#$HOME#~#g" results/logs/*.log results/*.txt \
  results/*.md 2>/dev/null || true
echo DONE
