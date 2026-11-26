#!/usr/bin/env python3
"""regex_guard.py: the common "file guard" PreToolUse hook on Bash. Block a command when its text names a protected
path AND contains a write operator or a file-changing command. This is the pattern most blog posts and gists use;
it is written here from that description, not copied from any one of them.
"""
import json, re, sys

PROTECTED = re.compile(r"(^|[\s'\"=/.(])tests/")
WRITE = re.compile(
    r">|\btee\b|\bsed\b[^|;&]*\s-i|\bperl\b[^|;&]*\s-\w*i|\brm\b|\bmv\b|\bcp\b|\btruncate\b|\bdd\b|\binstall\b"
    r"|\bln\b|\btouch\b|\bchmod\b")

data = json.load(sys.stdin)
cmd = (data.get("tool_input") or {}).get("command") or ""
if PROTECTED.search(cmd) and WRITE.search(cmd):
    print("Blocked: this command writes to tests/, which is protected.", file=sys.stderr)
    sys.exit(2)
sys.exit(0)
