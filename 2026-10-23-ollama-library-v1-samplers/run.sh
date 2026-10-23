#!/usr/bin/env bash
# Starts one Ollama server (private data folder, 2 parallel slots, 6 GB memory cap) and, for each model in MODELS,
# records the sampler parameters the server logs per condition (bench.py --probe), runs the JSON bench, then the
# llama-guard3 consistency test. Models must already be pulled into $OHOME/models (ollama pull, see README).
# LIMIT=<n> sends only the first n emails; MODELS="a b" overrides the model list; GUARD=0 skips the guard test.
# CONDS picks the bench conditions (default: the two that answer the question; the probe records all four).
set -euo pipefail
cd "$(dirname "$0")"
export LC_ALL=C
OHOME=${OHOME:-$HOME/.cache/ai-tool-lab/ollama-library-v1}
OLLAMA=${OLLAMA:-$OHOME/ollama/bin/ollama}
MODELS=${MODELS:-"ministral-3:3b qwen3.5:2b"}
CONDS=${CONDS:-api-default,v1-default}
GUARD_REPS=${GUARD_REPS:-10}
LIMIT=${LIMIT:-0}
GUARD=${GUARD:-1}
OUT=${OUT:-results/raw.jsonl}
PORT=11500
BASE=http://127.0.0.1:$PORT
mkdir -p results/logs
[ -f results/server.tsv ] || printf "model\tbackground_cpu_pct\tpeak_rss_mib\tminutes\n" > results/server.tsv

cpu_busy() {  # percent of all CPU time busy over 5 s
  read -r _ u n s i w q sq _ < /proc/stat; sleep 5
  read -r _ u2 n2 s2 i2 w2 q2 sq2 _ < /proc/stat
  local busy=$(( (u2+n2+s2+q2+sq2) - (u+n+s+q+sq) )) idle=$(( (i2+w2) - (i+w) ))
  echo $(( 100 * busy / (busy + idle) ))
}
peak() { [ -r "/proc/$1/status" ] && awk '/VmHWM/ {printf "%.0f", $2/1024}' "/proc/$1/status"; }
ol() { OLLAMA_HOST=127.0.0.1:$PORT "$OLLAMA" "$@"; }

if curl -s "$BASE/" >/dev/null; then echo "port $PORT already in use" >&2; exit 1; fi
log=results/logs/ollama-serve.log
systemd-run --user --scope --quiet -p MemoryMax=6G -p MemorySwapMax=0 \
  --setenv=HOME="$OHOME/home" --setenv=OLLAMA_MODELS="$OHOME/home/models" --setenv=OLLAMA_HOST=127.0.0.1:$PORT \
  --setenv=OLLAMA_NUM_PARALLEL=2 --setenv=OLLAMA_MAX_LOADED_MODELS=1 "$OLLAMA" serve > "$log" 2>&1 &
wrapper=$!
until curl -sf "$BASE/api/version" >/dev/null; do kill -0 "$wrapper" 2>/dev/null || { echo "ollama died" >&2; exit 1; }; sleep 0.5; done
ol --version 2>&1 | tail -1 > results/ollama-version.txt

for model in $MODELS; do
  bg=$(cpu_busy); t0=$(date +%s)
  slug=$(echo "$model" | tr ':.' '--')
  ol show "$model" --parameters > "results/logs/params-$slug.txt" 2>&1 || true
  python3 bench.py --base "$BASE" --model "$model" --probe --log "$log"
  python3 bench.py --base "$BASE" --model "$model" --out "$OUT" --limit "$LIMIT" --conds "$CONDS"
  rpid=$(pgrep -n -f "lib/ollama/llama-server" || true)
  pk=; [ -n "$rpid" ] && pk=$(peak "$rpid")
  ol stop "$model" >/dev/null 2>&1 || true
  [ "$LIMIT" = 0 ] && printf "%s\t%s\t%s\t%.1f\n" "$model" "$bg" "$pk" \
    "$(echo "($(date +%s) - $t0) / 60" | bc -l)" | tee -a results/server.tsv
  if [ "$GUARD" = 1 ] && [ "$model" = "ministral-3:3b" ]; then  # guard test right after the first model
    bg=$(cpu_busy); t0=$(date +%s)
    python3 bench.py --base "$BASE" --model llama-guard3:1b --probe --log "$log" >/dev/null || true
    python3 guard.py --base "$BASE" --reps "$GUARD_REPS" --out results/guard-raw.jsonl
    ol stop llama-guard3:1b >/dev/null 2>&1 || true
    printf "%s\t%s\t%s\t%.1f\n" llama-guard3:1b "$bg" "" "$(echo "($(date +%s) - $t0) / 60" | bc -l)" \
      | tee -a results/server.tsv
  fi
done

opid=$(pgrep -n -x ollama || true)
[ -n "$opid" ] && kill "$opid"
wait "$wrapper" 2>/dev/null || true
sed -i -e "s#$OHOME#<ollama-home>#g" -e "s#$PWD/##g" -e "s#$HOME#~#g" results/logs/*.log results/logs/*.txt \
  results/effective-params.txt 2>/dev/null || true
echo DONE
