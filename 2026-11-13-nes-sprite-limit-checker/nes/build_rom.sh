#!/usr/bin/env bash
# build_rom.sh <out.nes> <main.c> [more .c files]
# Compiles a C program against nes/ (crt0, nes.c, CHR) with cc65 inside the
# gcc:14 image. cc65 is built by checker/build.sh into $WORK/cc65.
set -euo pipefail
NES="$(cd "$(dirname "$0")" && pwd)"
WORK="${WORK:-$HOME/.cache/nessprites}"
OUT="$(realpath -m "$1")"; shift
B="$(mktemp -d)"
cp "$NES"/crt0.s "$NES"/chr.s "$NES"/nes.c "$NES"/nes.h "$NES"/nrom.cfg "$B"/
mkdir -p "$B/nes" && cp "$NES/nes.h" "$B/nes/"
python3 "$NES/make_chr.py" "$B/chr.bin"
srcs=()
for f in "$@"; do cp "$f" "$B/$(basename "$f")"; srcs+=("$(basename "$f")"); done
mkdir -p "$(dirname "$OUT")"
docker run --rm --network none --user "$(id -u):$(id -g)" -v "$WORK/cc65:/cc65:ro" -v "$B:/b" -w /b gcc:14 \
  sh -c "CC65_HOME=/cc65 /cc65/bin/cl65 -t nes -Oirs -C nrom.cfg -m map.txt -o rom.nes crt0.s chr.s nes.c ${srcs[*]} /cc65/lib/nes.lib"
cp "$B/rom.nes" "$OUT"
cp "$B/map.txt" "${OUT%.nes}.map"
rm -rf "$B"
echo "built $OUT"
