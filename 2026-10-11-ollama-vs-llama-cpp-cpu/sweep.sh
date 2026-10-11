#!/usr/bin/env bash
# Thread sweep with llama-bench (llama.cpp build only): 346-token prompt and 64 generated tokens,
# 1 to 4 threads, 3 repetitions each. Run after run_all.sh, with nothing else of ours loaded.
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p results
for gguf in models/qwen2.5-1.5b-instruct-q4_k_m.gguf models/llama-3.2-3b-instruct-q4_k_m.gguf; do
  vendor/llama.cpp/build/bin/llama-bench -m "$gguf" -p 346 -n 64 -t 1,2,3,4 -r 3 -o md
done | tee results/thread-sweep.md
