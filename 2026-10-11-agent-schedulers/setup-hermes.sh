#!/usr/bin/env bash
# Installs Hermes Agent into an isolated folder: its own uv binary, its own Python 3.14, its own venv and its
# own HERMES_HOME. Nothing is written to ~/.hermes, ~/.local/bin or the system Python.
# Usage: ./setup-hermes.sh            (BASE defaults to ~/.cache/agent-schedulers)
set -euo pipefail

BASE="${BASE:-$HOME/.cache/agent-schedulers}"
UV_VERSION=0.9.5
mkdir -p "$BASE/vendor" "$BASE/results" "$BASE/hermes-home/scripts"
cd "$BASE/vendor"

if [ ! -x uv-x86_64-unknown-linux-gnu/uv ]; then
  curl -fsSL -o uv.tgz "https://github.com/astral-sh/uv/releases/download/$UV_VERSION/uv-x86_64-unknown-linux-gnu.tar.gz"
  tar xzf uv.tgz
fi
UV="$BASE/vendor/uv-x86_64-unknown-linux-gnu/uv"
export UV_PYTHON_INSTALL_DIR="$BASE/vendor/python" UV_CACHE_DIR="$BASE/vendor/uv-cache"

[ -d hermes-agent ] || git clone -q --depth 1 https://github.com/NousResearch/hermes-agent.git
cd hermes-agent
git log -1 --format='Hermes Agent commit %H (%cd)'

start=$(date +%s)
"$UV" venv -q -p 3.14 "$BASE/venv"
VIRTUAL_ENV="$BASE/venv" "$UV" pip install -q -e .
echo "install seconds: $(( $(date +%s) - start ))"
du -sh "$BASE/venv" "$BASE/vendor/python"
echo "packages: $(VIRTUAL_ENV="$BASE/venv" "$UV" pip list 2>/dev/null | tail -n +3 | wc -l)"
HERMES_HOME="$BASE/hermes-home" "$BASE/venv/bin/hermes" --version | head -1
