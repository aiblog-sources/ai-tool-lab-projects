#!/usr/bin/env bash
# Creates the test folder for the agent task: 7 Markdown files and 2 other files in notes/.
# The correct answer is 7. Removes any count.txt left by a previous run.
set -euo pipefail
T="$1"
mkdir -p "$T/notes"
rm -f "$T/count.txt"
for n in alpha beta gamma delta epsilon fig grape; do echo "# $n" > "$T/notes/$n.md"; done
echo x > "$T/notes/readme.txt"
echo y > "$T/notes/data.json"
