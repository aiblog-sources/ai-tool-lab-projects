#!/usr/bin/env bash
# One-time toolchain setup, all in Docker or under $WORK (no system packages):
#  - image smsvdp-sdcc: gcc:14 plus Debian's sdcc package (SDCC 4.5.0 at the time of the post)
#  - devkitSMS at a pinned commit; SMSlib.lib, crt0_sms.rel and makesms built from source
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
WORK="${WORK:-$HOME/.cache/smsvdp}"
DEVKITSMS_COMMIT=561bc2c   # sverx/devkitSMS, 2026-11-07
mkdir -p "$WORK/bin"
printf 'FROM gcc:14\nRUN apt-get update && apt-get install -y --no-install-recommends sdcc && rm -rf /var/lib/apt/lists/*\n' \
  | docker build -q -t smsvdp-sdcc - >/dev/null
[ -d "$WORK/devkitSMS" ] || git clone -q https://github.com/sverx/devkitSMS "$WORK/devkitSMS"
git -C "$WORK/devkitSMS" checkout -q "$DEVKITSMS_COMMIT"
rm -rf "$WORK/smslib-build" && cp -r "$WORK/devkitSMS/SMSlib/src" "$WORK/smslib-build"
cp "$WORK/devkitSMS/crt0/src/crt0_sms.s" "$WORK/smslib-build/"
docker run --rm --network none --user "$(id -u):$(id -g)" -v "$WORK:/w" -w /w smsvdp-sdcc sh -c '
  set -e
  gcc -O2 -o bin/makesms devkitSMS/tools/makesms/src/makesms.c
  cd smslib-build && make -j4 SMSlib.lib >/dev/null 2>&1 && sdasz80 -g -o crt0_sms.rel crt0_sms.s
  sdcc --version | head -1
'
echo "toolchain ready in $WORK"
