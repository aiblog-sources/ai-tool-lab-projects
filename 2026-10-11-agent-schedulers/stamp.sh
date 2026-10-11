#!/bin/sh
# Appends "<scheduler> <epoch with ns>" so we can measure how late each run fired.
echo "$1 $(date +%s.%N)" >> "${STAMP_LOG:-/out/stamps.log}"
