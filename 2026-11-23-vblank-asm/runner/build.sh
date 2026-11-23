#!/usr/bin/env bash
# Builds nesvb against the FCEUmm core from the 2026-11-17 nesprof project
# (../../2026-11-17-nes-cc65-profiler/checker/build.sh builds cc65 and the
# patched core) and smsvb against the Genesis Plus GX core from the 2026-11-19
# smsvdp project (../../2026-11-19-sms-vdp-timing-checker/checker/build.sh).
# sjasmplus v1.24.1 comes from ../../2026-11-21-6502-vs-z80-asm/runner/build.sh.
# All compiling happens inside the gcc:14 Docker image with no network.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
NESW="${NESW:-$HOME/.cache/nesprof}"
SMSW="${SMSW:-$HOME/.cache/smsvdp}"
SJASM_WORK="${SJASM_WORK:-$HOME/.cache/asmtest}"
WORK="${WORK:-$HOME/.cache/vblankasm}"
mkdir -p "$WORK/bin"
[ -f "$NESW/fceumm/fceumm_libretro.so" ] || { echo "build the nesprof core first"; exit 1; }
[ -f "$SMSW/gpgx/genesis_plus_gx_libretro.so" ] || { echo "build the smsvdp core first"; exit 1; }
[ -x "$SJASM_WORK/sjasmplus/sjasmplus" ] || { echo "build sjasmplus first (2026-11-21 runner/build.sh)"; exit 1; }
cp "$HERE/nesvb.c" "$HERE/smsvb.c" "$WORK/"
run() { docker run --rm --network none --user "$(id -u):$(id -g)" -v "$WORK:/w" -v "$NESW:/n" -v "$SMSW:/s" gcc:14 sh -c "$1"; }
run 'cd /w && gcc -O2 -Wall -Wextra -I/n/fceumm/src/drivers/libretro/libretro-common/include nesvb.c -o bin/nesvb -ldl'
run 'cd /w && INC="-I/s/gpgx/core -I/s/gpgx/core/m68k -I/s/gpgx/core/z80 -I/s/gpgx/core/sound -I/s/gpgx/core/input_hw \
  -I/s/gpgx/core/cd_hw -I/s/gpgx/core/cart_hw -I/s/gpgx/core/cart_hw/svp -I/s/gpgx/core/ntsc -I/s/gpgx/core/debug \
  -I/s/gpgx/libretro -I/s/gpgx/libretro/libretro-common/include" && \
  DEFS="-DHOOK_CPU -DUSE_16BPP_RENDERING -DLSB_FIRST -DBYTE_ORDER=LITTLE_ENDIAN -DHAVE_ZLIB -D__LIBRETRO__ \
  -DM68K_OVERCLOCK_SHIFT=20 -DZ80_OVERCLOCK_SHIFT=20 -DHAVE_YM3438_CORE -DHAVE_OPLL_CORE -DMAXROMSIZE=33554432 \
  -DHAVE_CDROM -DUSE_LIBTREMOR -DUSE_LIBCHDR -DUSE_PER_SOUND_CHANNELS_CONFIG -fcommon" && \
  gcc -O2 -Wall -Wno-unused-function $DEFS $INC smsvb.c -o bin/smsvb -ldl /s/gpgx/genesis_plus_gx_libretro.so -Wl,-rpath,/s/gpgx'
echo "built $WORK/bin/nesvb, $WORK/bin/smsvb"
