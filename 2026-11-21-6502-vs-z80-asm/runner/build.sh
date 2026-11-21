#!/usr/bin/env bash
# Builds nesrun against the FCEUmm core from the 2026-11-17 nesprof project
# (run ../2026-11-17-nes-cc65-profiler/checker/build.sh first: it builds cc65
# and the patched core), smsrun against the Genesis Plus GX core from the
# 2026-11-19 smsvdp project (../2026-11-19-sms-vdp-timing-checker/checker/build.sh),
# and the sjasmplus Z80 assembler at a pinned tag. All compiling happens inside
# the gcc:14 Docker image with no network.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
NESW="${NESW:-$HOME/.cache/nesprof}"
SMSW="${SMSW:-$HOME/.cache/smsvdp}"
WORK="${WORK:-$HOME/.cache/asmtest}"
SJASM_TAG=v1.24.1   # z00m128/sjasmplus, commit ccc7c1747cead633fe82ae9a0922254683e5bc90
mkdir -p "$WORK/bin"
[ -d "$WORK/sjasmplus" ] || git clone -q https://github.com/z00m128/sjasmplus "$WORK/sjasmplus"
git -C "$WORK/sjasmplus" checkout -q "$SJASM_TAG"
[ -f "$NESW/fceumm/fceumm_libretro.so" ] || { echo "build the nesprof core first"; exit 1; }
[ -f "$SMSW/gpgx/genesis_plus_gx_libretro.so" ] || { echo "build the smsvdp core first"; exit 1; }
cp "$HERE/nesrun.c" "$HERE/smsrun.c" "$WORK/"
run() { docker run --rm --network none --user "$(id -u):$(id -g)" -v "$WORK:/w" -v "$NESW:/n" -v "$SMSW:/s" gcc:14 sh -c "$1"; }
[ -x "$WORK/sjasmplus/sjasmplus" ] || run 'cd /w/sjasmplus && make -j4 USE_LUA=0 > build.log 2>&1'
run 'cd /w && gcc -O2 -Wall -Wextra -I/n/fceumm/src/drivers/libretro/libretro-common/include nesrun.c -o bin/nesrun -ldl'
run 'cd /w && INC="-I/s/gpgx/core -I/s/gpgx/core/m68k -I/s/gpgx/core/z80 -I/s/gpgx/core/sound -I/s/gpgx/core/input_hw \
  -I/s/gpgx/core/cd_hw -I/s/gpgx/core/cart_hw -I/s/gpgx/core/cart_hw/svp -I/s/gpgx/core/ntsc -I/s/gpgx/core/debug \
  -I/s/gpgx/libretro -I/s/gpgx/libretro/libretro-common/include" && \
  DEFS="-DHOOK_CPU -DUSE_16BPP_RENDERING -DLSB_FIRST -DBYTE_ORDER=LITTLE_ENDIAN -DHAVE_ZLIB -D__LIBRETRO__ \
  -DM68K_OVERCLOCK_SHIFT=20 -DZ80_OVERCLOCK_SHIFT=20 -DHAVE_YM3438_CORE -DHAVE_OPLL_CORE -DMAXROMSIZE=33554432 \
  -DHAVE_CDROM -DUSE_LIBTREMOR -DUSE_LIBCHDR -DUSE_PER_SOUND_CHANNELS_CONFIG -fcommon" && \
  gcc -O2 -Wall -Wno-unused-function $DEFS $INC smsrun.c -o bin/smsrun -ldl /s/gpgx/genesis_plus_gx_libretro.so -Wl,-rpath,/s/gpgx'
echo "built $WORK/bin/nesrun, $WORK/bin/smsrun, $WORK/sjasmplus/sjasmplus"
