#!/usr/bin/env bash
# Downloads Ollama, builds llama.cpp from source and fetches the two GGUF files.
# Everything lands in vendor/ and models/ (both git-ignored). Nothing is installed system-wide.
set -euo pipefail
cd "$(dirname "$0")"
OLLAMA_VERSION=v0.40.3
LLAMA_CPP_COMMIT=966baae   # 2026-10-11
mkdir -p vendor models

if [ ! -x vendor/ollama/bin/ollama ]; then
  curl -L -o vendor/ollama.tar.zst \
    "https://github.com/ollama/ollama/releases/download/${OLLAMA_VERSION}/ollama-linux-amd64.tar.zst"
  mkdir -p vendor/ollama && tar --zstd -xf vendor/ollama.tar.zst -C vendor/ollama
fi

if [ ! -x vendor/llama.cpp/build/bin/llama-server ]; then
  [ -d vendor/llama.cpp ] || git clone https://github.com/ggml-org/llama.cpp vendor/llama.cpp
  git -C vendor/llama.cpp checkout "$LLAMA_CPP_COMMIT"
  cmake -S vendor/llama.cpp -B vendor/llama.cpp/build -DCMAKE_BUILD_TYPE=Release -DGGML_NATIVE=ON -DLLAMA_CURL=OFF
  cmake --build vendor/llama.cpp/build -j"$(nproc)" --target llama-server llama-bench
fi

[ -f models/qwen2.5-1.5b-instruct-q4_k_m.gguf ] || curl -L -o models/qwen2.5-1.5b-instruct-q4_k_m.gguf \
  https://huggingface.co/bartowski/Qwen2.5-1.5B-Instruct-GGUF/resolve/main/Qwen2.5-1.5B-Instruct-Q4_K_M.gguf
[ -f models/llama-3.2-3b-instruct-q4_k_m.gguf ] || curl -L -o models/llama-3.2-3b-instruct-q4_k_m.gguf \
  https://huggingface.co/bartowski/Llama-3.2-3B-Instruct-GGUF/resolve/main/Llama-3.2-3B-Instruct-Q4_K_M.gguf
sha256sum models/*.gguf
