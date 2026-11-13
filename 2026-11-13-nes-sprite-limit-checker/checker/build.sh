#!/usr/bin/env bash
# Builds cc65, the FCEUmm libretro core (with its symbols exported), the Mesen
# libretro core (for the overflow-flag cross-check) and nessprites. Everything
# compiles inside the gcc:14 Docker image; sources are cloned (not vendored)
# into $WORK at pinned commits.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
WORK="${WORK:-$HOME/.cache/nessprites}"
FCEUMM_COMMIT=e0f3086a8104f8fbbe49a2615b10401504a9533f  # libretro/libretro-fceumm, 2026-11-02
CC65_COMMIT=eebf54af545f4ae52f8489a2f6d4d5f93e7942d2    # cc65/cc65, 2026-11-08
MESEN_COMMIT=0102910c39ad1a62bc3f784466f3f67ca9eae335   # libretro/Mesen, 2026-04-20
mkdir -p "$WORK"
clone() { [ -d "$WORK/$2" ] || git clone -q "$1" "$WORK/$2"; git -C "$WORK/$2" fetch -q origin "$3" 2>/dev/null || true; git -C "$WORK/$2" checkout -q "$3"; }
clone https://github.com/libretro/libretro-fceumm fceumm "$FCEUMM_COMMIT"
clone https://github.com/cc65/cc65 cc65 "$CC65_COMMIT"
clone https://github.com/libretro/Mesen mesen "$MESEN_COMMIT"
cp "$HERE/nessprites.c" "$WORK/nessprites.c"
run() { docker run --rm --network none --user "$(id -u):$(id -g)" -v "$WORK:/w" gcc:14 sh -c "$1"; }
[ -x "$WORK/cc65/bin/cl65" ] || run 'cd /w/cc65 && make -s -j4 >/dev/null'
# No version script: SPRAM, PPU and XBuf stay visible to dlsym.
[ -f "$WORK/fceumm/fceumm_libretro.so" ] || run 'cd /w/fceumm && make -s -f Makefile.libretro -j4 SHARED="-shared -Wl,--no-undefined" >/dev/null'
[ -f "$WORK/mesen/Libretro/mesen_libretro.so" ] || run 'cd /w/mesen/Libretro && make -s -j4 >/dev/null 2>&1'
run 'cd /w && gcc -O2 -Wall -Wextra -Ifceumm/src/drivers/libretro/libretro-common/include nessprites.c -o nessprites -ldl'
echo "built $WORK/nessprites, $WORK/fceumm/fceumm_libretro.so, $WORK/mesen/Libretro/mesen_libretro.so"
