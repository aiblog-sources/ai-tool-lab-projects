#!/usr/bin/env python3
"""Scrub result files before publishing: the home folder becomes ~, the login name (ls -l output) becomes `user`,
home paths cut short by the 300-character result limit become `~`, and paths under ~ keep only their part after
the scratch folder (`<scratch>/work/...`) or become `<path>`. Usage: python3 bench/scrub.py results/*.jsonl"""
import getpass, os, re, sys
home, user = os.path.expanduser("~"), getpass.getuser()
for f in sys.argv[1:]:
    text = open(f).read()
    new = text.replace(home, "~")
    new = re.sub(r"/home/[A-Za-z0-9_.-]*", "~", new)
    new = re.sub(rf"\b{re.escape(user)}\b", "user", new)
    # the folder layout under ~ is private too: keep only the part after the bench's work/ or guards/ folder
    new = re.sub(r"~/[^\s\"'\\]*?/(work|guards)/", r"<scratch>/\1/", new)
    new = re.sub(r"~/(?!\.npm)[^\s\"'\\]*", "<path>", new)
    open(f, "w").write(new)
    print(f, "changed" if new != text else "unchanged")
