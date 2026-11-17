#!/usr/bin/env bash
# Builds cc65, the FCEUmm libretro core with checker/fceumm-nesprof.patch
# (a cycle counter and a per-instruction hook), and nesprof. Sources are
# cloned at pinned commits into $WORK; everything compiles inside the gcc:14
# Docker image with no network.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
WORK="${WORK:-$HOME/.cache/nesprof}"
CC65_WORK="${CC65_WORK:-$HOME/.cache/nessprites}"   # cc65 shared with nes/build_rom.sh
FCEUMM_COMMIT=e0f3086a8104f8fbbe49a2615b10401504a9533f  # libretro/libretro-fceumm, 2026-11-02
CC65_COMMIT=eebf54af545f4ae52f8489a2f6d4d5f93e7942d2    # cc65/cc65, 2026-11-08
mkdir -p "$WORK" "$CC65_WORK"
clone() { [ -d "$2" ] || git clone -q "$1" "$2"; git -C "$2" fetch -q origin "$3" 2>/dev/null || true; git -C "$2" checkout -q "$3"; }
clone https://github.com/libretro/libretro-fceumm "$WORK/fceumm" "$FCEUMM_COMMIT"
clone https://github.com/cc65/cc65 "$CC65_WORK/cc65" "$CC65_COMMIT"
git -C "$WORK/fceumm" apply --check "$HERE/fceumm-nesprof.patch" 2>/dev/null && git -C "$WORK/fceumm" apply "$HERE/fceumm-nesprof.patch"
grep -q nesprof_hook "$WORK/fceumm/src/x6502.c" || { echo "patch not applied"; exit 1; }
cp "$HERE/nesprof.c" "$WORK/nesprof.c"
run() { docker run --rm --network none --user "$(id -u):$(id -g)" -v "$WORK:/w" -v "$CC65_WORK:/c" gcc:14 sh -c "$1"; }
[ -x "$CC65_WORK/cc65/bin/cl65" ] || run 'cd /c/cc65 && make -s -j4 >/dev/null'
[ -f "$WORK/fceumm/fceumm_libretro.so" ] || run 'cd /w/fceumm && make -s -f Makefile.libretro -j4 SHARED="-shared -Wl,--no-undefined" >/dev/null'
run 'cd /w && gcc -O2 -Wall -Wextra -Ifceumm/src/drivers/libretro/libretro-common/include nesprof.c -o nesprof -ldl'
echo "built $WORK/nesprof, $WORK/fceumm/fceumm_libretro.so"
