#!/usr/bin/env bash
# Runs nessprites inside the gcc:14 image with no network. The current
# directory is mounted at /p (use paths relative to it); the build folder
# ($WORK) at /w, so cores are /w/fceumm/fceumm_libretro.so and
# /w/mesen/Libretro/mesen_libretro.so. /tmp in the container is a tmpfs.
set -euo pipefail
WORK="${WORK:-$HOME/.cache/nessprites}"
exec docker run --rm --network none --user "$(id -u):$(id -g)" --tmpfs /tmp:size=2g \
  -v "$WORK:/w" -v "$PWD:/p" -w /p gcc:14 /w/nessprites "$@"
