#!/usr/bin/env bash
# Rebuilds only the mdprof frontend (with --fncsv) against the Genesis Plus GX core that the 2026-10-28 project's
# profiler/build.sh already built in $WORK/gpgx (HOOK_CPU=1, pinned commit 60bee4c). Writes $WORK/mdprof-loops and
# leaves the older $WORK/mdprof alone.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
WORK="${WORK:-$HOME/.cache/mdprof}"
[ -f "$WORK/gpgx/genesis_plus_gx_libretro.so" ] || { echo "build the core first: ../2026-10-28-sgdk-frame-profiler/profiler/build.sh"; exit 1; }
cp "$HERE/mdprof.c" "$WORK/mdprof-loops.c"
docker run --rm --network none --user "$(id -u):$(id -g)" -v "$WORK:/w" -w /w gcc:14 sh -c '
  INC="-Igpgx/core -Igpgx/core/m68k -Igpgx/core/z80 -Igpgx/core/sound -Igpgx/core/input_hw -Igpgx/core/cd_hw \
       -Igpgx/core/cart_hw -Igpgx/core/cart_hw/svp -Igpgx/core/ntsc -Igpgx/core/debug -Igpgx/libretro \
       -Igpgx/libretro/libretro-common/include"
  DEFS="-DHOOK_CPU -DUSE_16BPP_RENDERING -DLSB_FIRST -DBYTE_ORDER=LITTLE_ENDIAN -DHAVE_ZLIB -D__LIBRETRO__ \
        -DM68K_OVERCLOCK_SHIFT=20 -DZ80_OVERCLOCK_SHIFT=20 -DHAVE_YM3438_CORE -DHAVE_OPLL_CORE \
        -DMAXROMSIZE=33554432 -DHAVE_CDROM -DUSE_LIBTREMOR -DUSE_LIBCHDR -DUSE_PER_SOUND_CHANNELS_CONFIG \
        -fcommon"
  gcc -O2 -Wall -Wno-unused-function $DEFS $INC mdprof-loops.c -o mdprof-loops -ldl gpgx/genesis_plus_gx_libretro.so -Wl,-rpath,/w/gpgx
'
echo "built $WORK/mdprof-loops"
