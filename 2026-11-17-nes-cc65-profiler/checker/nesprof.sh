#!/usr/bin/env bash
# nesprof.sh rom.nes frames [nesprof options]
# Makes rom.sym from rom.dbg (next to the ROM) if needed, then runs nesprof
# inside the gcc:14 image with no network. The current directory is mounted
# at /p, so use paths under it.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
WORK="${WORK:-$HOME/.cache/nesprof}"
ROM="$1"; shift
SYM="${ROM%.nes}.sym"
[ -f "$SYM" ] && [ "$SYM" -nt "${ROM%.nes}.dbg" ] || python3 "$HERE/../tools/symbols.py" "${ROM%.nes}.dbg" "$SYM"
exec docker run --rm --network none --user "$(id -u):$(id -g)" --tmpfs /tmp:size=256m \
  -v "$WORK:/w:ro" -v "$PWD:/p" -w /p gcc:14 /w/nesprof /w/fceumm/fceumm_libretro.so "$ROM" "$@" --sym "$SYM"
