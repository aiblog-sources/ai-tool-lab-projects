#!/usr/bin/env bash
# Builds llama.cpp from source and downloads the GGUF files listed in models.tsv.
# Everything lands in vendor/ and models/ (both git-ignored). Nothing is installed system-wide.
set -euo pipefail
cd "$(dirname "$0")"
LLAMA_CPP_COMMIT=966baae   # build b11554, 2026-10-11
mkdir -p vendor models

if [ ! -x vendor/llama.cpp/build/bin/llama-server ]; then
  [ -d vendor/llama.cpp ] || git clone https://github.com/ggml-org/llama.cpp vendor/llama.cpp
  git -C vendor/llama.cpp checkout "$LLAMA_CPP_COMMIT"
  cmake -S vendor/llama.cpp -B vendor/llama.cpp/build -DCMAKE_BUILD_TYPE=Release -DGGML_NATIVE=ON -DLLAMA_CURL=OFF
  cmake --build vendor/llama.cpp/build -j"$(nproc)" --target llama-server llama-bench
fi

# Qwen 2.5 1.5B and Llama 3.2 3B come from bartowski's repos under a lowercase file name.
declare -A RENAMED=(
  [qwen2.5-1.5b-instruct-q4_k_m.gguf]=Qwen2.5-1.5B-Instruct-Q4_K_M.gguf
  [llama-3.2-3b-instruct-q4_k_m.gguf]=Llama-3.2-3B-Instruct-Q4_K_M.gguf
)
tail -n +2 models.tsv | while IFS=$'\t' read -r _ file repo; do
  [ -f "models/$file" ] && continue
  remote=${RENAMED[$file]:-$file}
  curl -fL -o "models/$file.part" "https://huggingface.co/$repo/resolve/main/$remote"
  mv "models/$file.part" "models/$file"
done
sha256sum models/*.gguf
