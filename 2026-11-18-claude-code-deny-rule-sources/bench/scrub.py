#!/usr/bin/env python3
"""Copy result files into results/ without the canary or anything about the machine they ran on.

Usage: python3 bench/scrub.py <scratch-folder> <in.jsonl> <out.jsonl>
The canary becomes <CANARY>; tool results that held it only encoded (base64, od) are replaced by a note; the
user name, host name and home folder become placeholders. Token prefixes that Claude wrote into its own search
regexes during the secrets audit (GitHub and Anthropic key prefixes) become <token-prefix>, so secret scanners stay quiet.
"""
import getpass, json, socket, sys
from pathlib import Path

scratch, src, dst = Path(sys.argv[1]), Path(sys.argv[2]), Path(sys.argv[3])
canary = (scratch / "canary.txt").read_text().strip()
subs = [(p, "<token-prefix>") for p in ("gh" + "p_", "gh" + "o_", "github" + "_pat_", "sk-" + "ant-")] + [(canary, "<CANARY>"), (str(Path.home()), "<home>"), (getpass.getuser(), "<user>"), (socket.gethostname(), "<host>")]

def clean(s):
    for a, b in subs:
        s = s.replace(a, b)
    return s

with dst.open("w") as out:
    for line in src.read_text().splitlines():
        r = json.loads(line)
        for s in r["steps"]:
            if s["leaked"] and canary not in s["result"]:
                s["result"] = "[the .env contents, encoded; omitted]"
        out.write(clean(json.dumps(r)) + "\n")
print(dst, "written")
