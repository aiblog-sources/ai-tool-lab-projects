#!/usr/bin/env bash
# Builds cc65 and the FCEUmm libretro core (with its symbols exported)
# and nesflicker. The build folder is shared with nessprites (same pinned commits). Everything
# compiles inside the gcc:14 Docker image; sources are cloned (not vendored)
# into $WORK at pinned commits.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
WORK="${WORK:-$HOME/.cache/nessprites}"
FCEUMM_COMMIT=e0f3086a8104f8fbbe49a2615b10401504a9533f  # libretro/libretro-fceumm, 2026-11-02
CC65_COMMIT=eebf54af545f4ae52f8489a2f6d4d5f93e7942d2    # cc65/cc65, 2026-11-08
mkdir -p "$WORK"
clone() { [ -d "$WORK/$2" ] || git clone -q "$1" "$WORK/$2"; git -C "$WORK/$2" fetch -q origin "$3" 2>/dev/null || true; git -C "$WORK/$2" checkout -q "$3"; }
clone https://github.com/libretro/libretro-fceumm fceumm "$FCEUMM_COMMIT"
clone https://github.com/cc65/cc65 cc65 "$CC65_COMMIT"
cp "$HERE/nesflicker.c" "$WORK/nesflicker.c"
run() { docker run --rm --network none --user "$(id -u):$(id -g)" -v "$WORK:/w" gcc:14 sh -c "$1"; }
[ -x "$WORK/cc65/bin/cl65" ] || run 'cd /w/cc65 && make -s -j4 >/dev/null'
# No version script: SPRAM, PPU and XBuf stay visible to dlsym.
[ -f "$WORK/fceumm/fceumm_libretro.so" ] || run 'cd /w/fceumm && make -s -f Makefile.libretro -j4 SHARED="-shared -Wl,--no-undefined" >/dev/null'
run 'cd /w && gcc -O2 -Wall -Wextra -Ifceumm/src/drivers/libretro/libretro-common/include nesflicker.c -o nesflicker -ldl'
echo "built $WORK/nesflicker, $WORK/fceumm/fceumm_libretro.so"
