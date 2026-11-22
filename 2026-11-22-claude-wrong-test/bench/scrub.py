#!/usr/bin/env python3
"""Replace the home folder in result files with ~ before publishing. Usage: python3 bench/scrub.py results/*.jsonl"""
import os, sys
home = os.path.expanduser("~")
for f in sys.argv[1:]:
    text = open(f).read()
    n = text.count(home)
    open(f, "w").write(text.replace(home, "~"))
    print(f, n, "replaced")
