#!/usr/bin/env python3
"""Replace the home folder in result files with ~ before publishing (python3 bench/scrub.py results/*.jsonl)."""
import os, sys
from pathlib import Path

home = os.path.expanduser("~")
user = os.path.basename(home)
for name in sys.argv[1:]:
    p = Path(name)
    text = p.read_text().replace(home, "~").replace(f" {user} ", " user ").replace(f" {user} ", " user ")
    assert user not in text, f"{name}: user name still present"
    p.write_text(text)
