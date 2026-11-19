#!/usr/bin/env bash
# build-rom.sh <src-dir> <out.sms>: compiles every .c in src-dir with SDCC and
# links it against SMSlib and crt0 built by sms/setup.sh (Docker, no network).
set -euo pipefail
WORK="${WORK:-$HOME/.cache/smsvdp}"
SRC="$(cd "$1" && pwd)"; OUT="$2"
B="$WORK/build/$(basename "$SRC")"
mkdir -p "$B" && cp "$SRC"/*.c "$B"/ && cp "$SRC"/*.h "$B"/ 2>/dev/null || true
docker run --rm --network none --user "$(id -u):$(id -g)" -v "$WORK:/w" -w "/w/build/$(basename "$SRC")" smsvdp-sdcc sh -c '
  set -e
  L=/w/smslib-build
  for c in *.c; do sdcc -c -mz80 --peep-file $L/peep-rules.txt -I$L $c; done
  sdcc -o rom.ihx -mz80 --no-std-crt0 --data-loc 0xC000 $L/crt0_sms.rel *.rel $L/SMSlib.lib
  /w/bin/makesms rom.ihx rom.sms >/dev/null
' > "$B/build.log" 2>&1 || { cat "$B/build.log"; exit 1; }
cp "$B/rom.sms" "$OUT"; cp "$B/rom.noi" "${OUT%.sms}.noi"
