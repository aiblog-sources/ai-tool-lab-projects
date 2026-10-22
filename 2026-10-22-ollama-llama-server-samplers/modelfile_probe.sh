#!/usr/bin/env bash
# Which Modelfile PARAMETER values survive on each Ollama route? Imports Qwen 3.5 0.8B with a full set of sampler
# parameters, sends three short requests and copies the sampler parameters Ollama's llama-server logs for each one
# to results/modelfile-params.txt. Uses its own Ollama data folder and port, so it can run next to run.sh.
set -euo pipefail
cd "$(dirname "$0")"
export LC_ALL=C
OLLAMA=$PWD/vendor/ollama/bin/ollama
OHOME=${OHOME:-$HOME/.cache/ai-tool-lab/samplers-ollama-modelfile}
PORT=11502
OUT=results/modelfile-params.txt
LOG=results/logs/ollama-modelfile.log
mkdir -p "$OHOME" results/logs
if curl -s "http://127.0.0.1:$PORT/" >/dev/null; then echo "port $PORT already in use" >&2; exit 1; fi

HOME=$OHOME OLLAMA_MODELS=$OHOME/models OLLAMA_HOST=127.0.0.1:$PORT \
  systemd-run --user --scope --quiet -p MemoryMax=6G -p MemorySwapMax=0 "$OLLAMA" serve > "$LOG" 2>&1 &
wrapper=$!
until curl -sf "http://127.0.0.1:$PORT/api/version" >/dev/null; do kill -0 "$wrapper" || exit 1; sleep 0.5; done

cat > "$OHOME/Modelfile" <<EOF
FROM $PWD/models/Qwen_Qwen3.5-0.8B-Q4_K_M.gguf
PARAMETER temperature 0.2
PARAMETER top_k 20
PARAMETER top_p 0.8
PARAMETER min_p 0.05
PARAMETER presence_penalty 1.5
PARAMETER repeat_penalty 1.05
EOF
export OLLAMA_HOST=127.0.0.1:$PORT
"$OLLAMA" create qwen08-params -f "$OHOME/Modelfile" >/dev/null 2>&1
{ echo "# Modelfile PARAMETER lines"; grep PARAMETER "$OHOME/Modelfile"; echo
  echo "# ollama show --parameters"; "$OLLAMA" show qwen08-params --parameters; echo; } > "$OUT"

MSG='"messages":[{"role":"user","content":"Reply with an empty JSON object."}]'
probe() {  # label route body
  local start; start=$(stat -c %s "$LOG")
  curl -s "http://127.0.0.1:$PORT$2" -d "$3" >/dev/null
  sleep 1
  { echo "## $1"; echo "request: $3" | sed 's/"messages":\[[^]]*\],//'
    tail -c +$((start + 1)) "$LOG" | grep -E "repeat_last_n|top_k =" | tail -2 | sed 's/^\s*//'; echo; } >> "$OUT"
}
probe "/api/chat, no options" /api/chat "{\"model\":\"qwen08-params\",$MSG,\"stream\":false,\"think\":false,\"options\":{\"num_predict\":8}}"
probe "/v1/chat/completions, no sampler fields" /v1/chat/completions "{\"model\":\"qwen08-params\",$MSG,\"max_tokens\":8,\"reasoning_effort\":\"none\"}"
probe "/v1/chat/completions, temperature 0.5 only" /v1/chat/completions "{\"model\":\"qwen08-params\",$MSG,\"max_tokens\":8,\"temperature\":0.5,\"reasoning_effort\":\"none\"}"
probe "/v1/chat/completions, top_p 0.8 only" /v1/chat/completions "{\"model\":\"qwen08-params\",$MSG,\"max_tokens\":8,\"top_p\":0.8,\"reasoning_effort\":\"none\"}"

"$OLLAMA" stop qwen08-params >/dev/null 2>&1 || true
kill "$(pgrep -n -x ollama)"
wait "$wrapper" 2>/dev/null || true
sed -i -e "s#$OHOME#<ollama-home>#g" -e "s#$PWD/##g" -e "s#$HOME#~#g" "$OUT" "$LOG"
cat "$OUT"
