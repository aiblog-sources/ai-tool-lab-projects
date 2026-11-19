#!/usr/bin/env bash
# Builds the Genesis Plus GX libretro core and smsvdp against it, inside the
# gcc:14 Docker image. The emulator source is cloned (not vendored) into $WORK
# at a pinned commit. No emulator patch: smsvdp wraps the core's exported Z80
# port handlers at run time.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
WORK="${WORK:-$HOME/.cache/smsvdp}"
GPGX_COMMIT=60bee4c724859710ce05672239e5b601ad01382e   # libretro/Genesis-Plus-GX, 2026-10-23
mkdir -p "$WORK"
if [ ! -d "$WORK/gpgx" ]; then
  git clone -q https://github.com/libretro/Genesis-Plus-GX "$WORK/gpgx"
fi
git -C "$WORK/gpgx" checkout -q "$GPGX_COMMIT"
cp "$HERE/smsvdp.c" "$WORK/smsvdp.c"
docker run --rm --network none --user "$(id -u):$(id -g)" -v "$WORK:/w" -w /w/gpgx gcc:14 sh -c '
  set -e
  # no version script, so z80_writeport, Z80 and the VDP state are exported;
  # HOOK_CPU=1 only to keep the struct layouts identical to the frontend build
  CFLAGS="-fcommon" make -s -f Makefile.libretro -j4 HOOK_CPU=1 SHARED="-shared -Wl,--no-undefined" >/dev/null
  cd /w
  INC="-Igpgx/core -Igpgx/core/m68k -Igpgx/core/z80 -Igpgx/core/sound -Igpgx/core/input_hw -Igpgx/core/cd_hw \
       -Igpgx/core/cart_hw -Igpgx/core/cart_hw/svp -Igpgx/core/ntsc -Igpgx/core/debug -Igpgx/libretro \
       -Igpgx/libretro/libretro-common/include"
  DEFS="-DHOOK_CPU -DUSE_16BPP_RENDERING -DLSB_FIRST -DBYTE_ORDER=LITTLE_ENDIAN -DHAVE_ZLIB -D__LIBRETRO__ \
        -DM68K_OVERCLOCK_SHIFT=20 -DZ80_OVERCLOCK_SHIFT=20 -DHAVE_YM3438_CORE -DHAVE_OPLL_CORE \
        -DMAXROMSIZE=33554432 -DHAVE_CDROM -DUSE_LIBTREMOR -DUSE_LIBCHDR -DUSE_PER_SOUND_CHANNELS_CONFIG \
        -fcommon"
  gcc -O2 -Wall -Wno-unused-function $DEFS $INC smsvdp.c -o smsvdp -ldl gpgx/genesis_plus_gx_libretro.so -Wl,-rpath,/w/gpgx
'
echo "built $WORK/smsvdp and $WORK/gpgx/genesis_plus_gx_libretro.so"
