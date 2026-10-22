#!/usr/bin/env bash
# For each model in models.tsv: start llama-server, run the llama-server conditions, stop it; then start Ollama,
# import the same GGUF (no PARAMETER lines, so Ollama's built-in defaults apply), run the Ollama conditions, stop it.
# One server at a time, 2 parallel slots on both, 2 threads, 6 GB memory cap. Before each bench, bench.py --probe
# records the sampler parameters each server logs for each condition in results/effective-params.txt.
# ONLY="<model name>" runs one model; LIMIT=<n> sends only the first n emails; REPS=<n> repeats per sampled condition.
set -euo pipefail
cd "$(dirname "$0")"
export LC_ALL=C
BIN=$PWD/vendor/llama.cpp/build/bin
OLLAMA=$PWD/vendor/ollama/bin/ollama
OHOME=${OHOME:-$HOME/.cache/ai-tool-lab/samplers-ollama}
ONLY=${ONLY:-}
LIMIT=${LIMIT:-0}
REPS=${REPS:-2}
OUT=${OUT:-results/raw.jsonl}
LPORT=11711
OPORT=11500
mkdir -p results/logs "$OHOME"
[ -f results/server.tsv ] || printf "model\tserver\tstatus\tbackground_cpu_pct\tpeak_rss_mib\tminutes\n" > results/server.tsv

cpu_busy() {  # percent of all CPU time busy over 5 s
  read -r _ u n s i w q sq _ < /proc/stat; sleep 5
  read -r _ u2 n2 s2 i2 w2 q2 sq2 _ < /proc/stat
  local busy=$(( (u2+n2+s2+q2+sq2) - (u+n+s+q+sq) )) idle=$(( (i2+w2) - (i+w) ))
  echo $(( 100 * busy / (busy + idle) ))
}
wait_up() {  # url wrapper_pid
  until curl -sf "$1" >/dev/null; do kill -0 "$2" 2>/dev/null || return 1; sleep 0.5; done
}
row() { [ "$LIMIT" = 0 ] && printf "%s\t%s\t%s\t%s\t%s\t%.1f\n" "$1" "$2" "$3" "$4" "$5" \
  "$(echo "($(date +%s) - $6) / 60" | bc -l)" | tee -a results/server.tsv || true; }
peak() { [ -r "/proc/$1/status" ] && awk '/VmHWM/ {printf "%.0f", $2/1024}' "/proc/$1/status"; }

tail -n +2 models.tsv | while IFS=$'\t' read -r name file _; do
  [ -n "$ONLY" ] && [ "$name" != "$ONLY" ] && continue
  slug=$(echo "$name" | tr 'A-Z .' 'a-z--')
  for p in $LPORT $OPORT; do
    if curl -s "http://127.0.0.1:$p/" >/dev/null; then echo "port $p already in use" >&2; exit 1; fi
  done

  # llama-server (upstream build)
  bg=$(cpu_busy); t0=$(date +%s); log=results/logs/llama-server-$slug.log
  systemd-run --user --scope --quiet -p MemoryMax=6G -p MemorySwapMax=0 \
    env LD_LIBRARY_PATH="$BIN" "$BIN/llama-server" -m "models/$file" --port $LPORT -c 8192 -np 2 -t 2 \
    --reasoning off --cache-ram 0 --log-verbosity 4 > "$log" 2>&1 &
  wrapper=$!; status=ok; pk=
  if wait_up "http://127.0.0.1:$LPORT/health" "$wrapper"; then
    pid=$(pgrep -n -x llama-server)
    python3 bench.py --server llama --base "http://127.0.0.1:$LPORT" --model "$name" --probe --log "$log"
    python3 bench.py --server llama --base "http://127.0.0.1:$LPORT" --model "$name" --out "$OUT" \
      --limit "$LIMIT" --reps "$REPS" || status=bench-error
    pk=$(peak "$pid") || status=killed
    kill "$pid" 2>/dev/null || true
    wait "$wrapper" 2>/dev/null || true
  else
    status=died
  fi
  while curl -s "http://127.0.0.1:$LPORT/" >/dev/null; do sleep 0.5; done
  row "$name" llama-server "$status" "$bg" "$pk" "$t0"

  # Ollama (bundled llama-server), private data folder
  bg=$(cpu_busy); t0=$(date +%s); log=results/logs/ollama-$slug.log
  HOME=$OHOME OLLAMA_MODELS=$OHOME/models OLLAMA_HOST=127.0.0.1:$OPORT OLLAMA_NUM_PARALLEL=2 \
    systemd-run --user --scope --quiet -p MemoryMax=6G -p MemorySwapMax=0 \
    --setenv=HOME="$OHOME" --setenv=OLLAMA_MODELS="$OHOME/models" --setenv=OLLAMA_HOST=127.0.0.1:$OPORT \
    --setenv=OLLAMA_NUM_PARALLEL=2 "$OLLAMA" serve > "$log" 2>&1 &
  wrapper=$!; status=ok; pk=
  if wait_up "http://127.0.0.1:$OPORT/api/version" "$wrapper"; then
    omodel=$(echo "$slug" | tr -d '-')
    echo "FROM $PWD/models/$file" > "$OHOME/Modelfile-$omodel"
    OLLAMA_HOST=127.0.0.1:$OPORT "$OLLAMA" create "$omodel" -f "$OHOME/Modelfile-$omodel" >/dev/null 2>&1
    OLLAMA_HOST=127.0.0.1:$OPORT "$OLLAMA" show "$omodel" --parameters > "results/logs/ollama-params-$slug.txt" 2>&1 || true
    python3 bench.py --server ollama --base "http://127.0.0.1:$OPORT" --model "$name" --served-model "$omodel" \
      --probe --log "$log"
    python3 bench.py --server ollama --base "http://127.0.0.1:$OPORT" --model "$name" --served-model "$omodel" \
      --out "$OUT" --limit "$LIMIT" --reps "$REPS" || status=bench-error
    rpid=$(pgrep -n -f "lib/ollama/llama-server" || true)
    [ -n "$rpid" ] && pk=$(peak "$rpid")
    OLLAMA_HOST=127.0.0.1:$OPORT "$OLLAMA" stop "$omodel" >/dev/null 2>&1 || true
    opid=$(pgrep -n -x ollama || true)
    [ -n "$opid" ] && kill "$opid"
    wait "$wrapper" 2>/dev/null || true
  else
    status=died
  fi
  while curl -s "http://127.0.0.1:$OPORT/" >/dev/null; do sleep 0.5; done
  row "$name" ollama "$status" "$bg" "$pk" "$t0"
done
sed -i -e "s#$OHOME#<ollama-home>#g" -e "s#$PWD/##g" -e "s#$HOME#~#g" results/logs/*.log results/logs/*.txt \
  results/effective-params.txt 2>/dev/null || true
echo DONE
