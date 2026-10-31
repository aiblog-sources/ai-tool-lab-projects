#!/usr/bin/env python3
"""Copy run rows into results/ with local paths replaced: the work folder becomes <work>, the home folder ~.
Usage: python3 bench/export.py <work-folder> <in.jsonl> <out.jsonl>"""
import getpass, json, socket, sys
from pathlib import Path
work, src, dst = str(Path(sys.argv[1]).resolve()), sys.argv[2], sys.argv[3]
subs = [(work, "<work>"), (work.replace(str(Path.home()), "~"), "<work>"),
        (str(Path(work).parent.parent), "<repo>"), (str(Path(work).parent.parent).replace(str(Path.home()), "~"), "<repo>"), (str(Path.home()), "~"), (getpass.getuser(), "<user>"), (socket.gethostname(), "<host>")]
with open(dst, "w") as out:
    for line in open(src):
        for a, b in subs: line = line.replace(a, b).replace(json.dumps(a)[1:-1], b)
        out.write(line)
