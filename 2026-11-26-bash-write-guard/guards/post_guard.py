#!/usr/bin/env python3
"""post_guard.py: do not parse the command at all. Before each Bash call (PreToolUse), copy the protected folder
aside; after it (PostToolUse), compare. If anything under it changed, put the copy back and tell Claude (exit 2).

Usage in settings: "post_guard.py pre" on PreToolUse and "post_guard.py post" on PostToolUse, matcher "Bash".
Byte-code caches (__pycache__, *.pyc) are ignored, or every test run would count as a write.
"""
import hashlib, json, os, shutil, sys, tempfile
from pathlib import Path

PROTECTED = "tests"
IGNORE = shutil.ignore_patterns("__pycache__", "*.pyc")


def digest(folder):
    h = hashlib.sha256()
    if folder.is_dir():
        for p in sorted(folder.rglob("*")):
            if "__pycache__" in p.parts or p.suffix == ".pyc":
                continue
            h.update(str(p.relative_to(folder)).encode() + b"\0")
            if p.is_symlink():
                h.update(b"L" + os.readlink(p).encode())
            elif p.is_file():
                h.update(p.read_bytes())
    return h.hexdigest()


data = json.load(sys.stdin)
mode = sys.argv[1]
root = Path(os.environ.get("CLAUDE_PROJECT_DIR") or data.get("cwd") or ".")
target = root / PROTECTED
key = (data.get("session_id") or "s") + "-" + (data.get("tool_use_id") or "t")
store = Path(tempfile.gettempdir()) / "post-guard" / key
if mode == "pre":
    shutil.rmtree(store, ignore_errors=True)
    store.mkdir(parents=True)
    if target.is_dir():
        shutil.copytree(target, store / "copy", ignore=IGNORE, symlinks=True)
    (store / "digest").write_text(digest(target))
    sys.exit(0)
if not (store / "digest").exists():
    sys.exit(0)
before = (store / "digest").read_text()
if digest(target) != before:
    if target.is_symlink() or target.is_file():
        target.unlink()
    elif target.is_dir():
        for p in list(target.iterdir()):
            if p.name == "__pycache__":
                continue
            shutil.rmtree(p) if p.is_dir() and not p.is_symlink() else p.unlink()
    if (store / "copy").is_dir():
        shutil.copytree(store / "copy", target, symlinks=True, dirs_exist_ok=True)
    shutil.rmtree(store, ignore_errors=True)
    print("Blocked: that command changed files in tests/, which is protected. The change was undone.", file=sys.stderr)
    sys.exit(2)
shutil.rmtree(store, ignore_errors=True)
sys.exit(0)
