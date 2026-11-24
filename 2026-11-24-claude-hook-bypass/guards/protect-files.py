#!/usr/bin/env python3
# protect-files.py: the "protect files" PreToolUse hook from the Claude Code hooks guide, ported from bash + jq to
# Python (same logic: read the tool input, block with exit 2 if the path matches a protected pattern).
import json, sys

PROTECTED_PATTERNS = ["tests/"]
MESSAGE = "Blocked: {path} matches protected pattern '{pattern}'"

data = json.load(sys.stdin)
path = (data.get("tool_input") or {}).get("file_path") or ""
path = path.replace("\\", "/")
for pattern in PROTECTED_PATTERNS:
    if pattern in path:
        print(MESSAGE.format(path=path, pattern=pattern), file=sys.stderr)
        sys.exit(2)
sys.exit(0)
