#!/usr/bin/env python3
"""Copy raw run files into results/ with local paths and the user name removed.
Usage: python3 bench/scrub.py <src-file> <dest-file> <work-root>"""
import getpass, os, re, sys
src, dest, root = sys.argv[1:4]
text = open(src).read()
text = text.replace(os.path.abspath(root), "<work>")
# This repo and the cache folder the corpus lived in (seen in Claude's own commands).
text = text.replace(os.path.abspath(os.path.join(os.path.dirname(__file__), "../..")), "<repo>")
text = text.replace(os.path.dirname(os.path.abspath(root)), "<cache>")
home, user = os.path.expanduser("~"), getpass.getuser()
text = text.replace(home, "~").replace(home.replace("/", "-"), "-home-user")
text = re.sub(rf"\b{re.escape(user)}\b", "user", text)
open(dest, "w").write(text)
print(dest, "home left:", text.count(home), "user left:", len(re.findall(rf"\b{re.escape(user)}\b", text)))
