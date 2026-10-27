#!/usr/bin/env bash
# Starts one Ollama server (private data folder, 2 parallel slots, 6 GB memory cap) and runs bench.py for each model
# in MODELS, recording background CPU load, peak runner memory and minutes per model in results/server.tsv.
# CACHE_RAM sets llama-server's host-RAM prompt cache in MiB (LLAMA_ARG_CACHE_RAM; unset = its 8192 default).
# Models must already be pulled into $OHOME/home/models (see README). LIMIT=<n> sends only n prompts per model.
set -euo pipefail
cd "$(dirname "$0")"
export LC_ALL=C
OHOME=${OHOME:-$HOME/.cache/ai-tool-lab/ollama-library-v1}
OLLAMA=${OLLAMA:-$OHOME/ollama/bin/ollama}
MODELS=${MODELS:-"llama-guard3:1b shieldgemma:2b granite3-guardian:2b"}
LIMIT=${LIMIT:-0}
OUT=${OUT:-results/raw.jsonl}
PORT=11510
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
log=results/logs/ollama-serve-$(date +%H%M).log
systemd-run --user --scope --quiet -p MemoryMax=6G -p MemorySwapMax=0 \
  --setenv=HOME="$OHOME/home" --setenv=OLLAMA_MODELS="$OHOME/home/models" --setenv=OLLAMA_HOST=127.0.0.1:$PORT \
  --setenv=OLLAMA_NUM_PARALLEL=2 --setenv=OLLAMA_MAX_LOADED_MODELS=1 ${CACHE_RAM:+--setenv=LLAMA_ARG_CACHE_RAM=$CACHE_RAM} \
  "$OLLAMA" serve > "$log" 2>&1 &
wrapper=$!
until curl -sf "$BASE/api/version" >/dev/null; do kill -0 "$wrapper" 2>/dev/null || { echo "ollama died" >&2; exit 1; }; sleep 0.5; done
curl -s "$BASE/api/version" > results/ollama-version.json

for model in $MODELS; do
  bg=$(cpu_busy); t0=$(date +%s)
  slug=$(echo "$model" | tr ':.' '--')
  ol show "$model" --modelfile 2>&1 | sed -n '/^TEMPLATE/,/^"""$/p; /^PARAMETER/p' > "results/logs/template-$slug.txt" || true
  python3 bench.py --base "$BASE" --model "$model" --out "$OUT" --limit "$LIMIT"
  rpid=$(pgrep -n -f "lib/ollama/llama-server" || true)
  pk=; [ -n "$rpid" ] && pk=$(peak "$rpid")
  ol stop "$model" >/dev/null 2>&1 || true
  [ "$LIMIT" = 0 ] && printf "%s\t%s\t%s\t%.1f\n" "$model" "$bg" "$pk" \
    "$(echo "($(date +%s) - $t0) / 60" | bc -l)" | tee -a results/server.tsv
done

opid=$(ps -eo pid,args | awk -v b="$OLLAMA" '$2==b && $3=="serve" {print $1}' | tail -1)
[ -n "$opid" ] && kill "$opid"
wait "$wrapper" 2>/dev/null || true
sed -i -e "s#$OHOME#<ollama-home>#g" -e "s#$PWD/##g" -e "s#$HOME#~#g" results/logs/* 2>/dev/null || true
echo DONE
