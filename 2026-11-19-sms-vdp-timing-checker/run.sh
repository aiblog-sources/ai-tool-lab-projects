#!/usr/bin/env bash
# Reproduces results/: the hand-counted test ROM (NTSC and PAL), the SMSlib call
# table, and devkitSMS's hello_sms example. Run sms/setup.sh and checker/build.sh first.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
WORK="${WORK:-$HOME/.cache/smsvdp}"
OUT="$HERE/results"
mkdir -p "$OUT"
smsvdp() { docker run --rm --network none --user "$(id -u):$(id -g)" -v "$WORK:/w" -w /w gcc:14 ./smsvdp gpgx/genesis_plus_gx_libretro.so "$@"; }

"$HERE/sms/build-rom.sh" "$HERE/testrom" "$WORK/testrom.sms"
smsvdp testrom.sms 130 --from 10 --sym testrom.noi --csv testrom-ntsc.csv > "$OUT/testrom-ntsc.txt" || true
smsvdp testrom.sms 130 --from 10 --sym testrom.noi --pal --csv testrom-pal.csv > "$OUT/testrom-pal.txt" || true

"$HERE/sms/build-rom.sh" "$HERE/libtest" "$WORK/libtest.sms"
TEST_ADDR=$(awk '/_test_id/ {print substr($3, 3)}' "$WORK/libtest.noi")
smsvdp libtest.sms 300 --from 60 --sym libtest.noi --csv lib.csv --events lib-ev.csv --ram "$TEST_ADDR:test" > "$OUT/libtest.txt" || true
python3 "$HERE/tools/libtable.py" "$WORK/lib.csv" "$WORK/lib-ev.csv" 60 > "$OUT/smslib-table.md"

mkdir -p "$WORK/hello_sms" && cp "$WORK/devkitSMS/examples/hello_sms/main.c" "$WORK/hello_sms/"
"$HERE/sms/build-rom.sh" "$WORK/hello_sms" "$WORK/hello.sms"
smsvdp hello.sms 300 --sym hello.noi --sites hello-sites.csv > "$OUT/hello_sms.txt" || true

cp "$WORK"/testrom-ntsc.csv "$WORK"/testrom-pal.csv "$WORK"/lib.csv "$OUT/"
sha256sum "$WORK/testrom.sms" "$WORK/libtest.sms" | sed "s|$WORK/||" > "$OUT/roms.sha256"
head -6 "$OUT/testrom-ntsc.txt"; head -6 "$OUT/testrom-pal.txt"; cat "$OUT/smslib-table.md"
