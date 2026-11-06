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

# Round three: the runs without --strict-mcp-config also listed the account's claude.ai connectors, and init lists every
# skill Claude Code ships. Keep only what the analysis needs: the notes servers, and where the search-notes Skills sat.
if dest.endswith(".jsonl"):
    import json
    out = []
    for line in text.splitlines():
        r = json.loads(line)
        keep = [i for i, s in enumerate(r["mcp_servers"]) if not s.startswith("claude.ai")]
        r["other_mcp_servers"] = len(r["mcp_servers"]) - len(keep)
        r["mcp_servers"] = [r["mcp_servers"][i] for i in keep]
        r["mcp_status"] = [r["mcp_status"][i] for i in keep]
        skills = r.pop("skills_all") or []
        r["skill_positions"] = {s: skills.index(s) for s in skills if "search-notes" in s}
        r["skills_listed_total"] = len(skills)
        out.append(json.dumps(r))
    open(dest, "w").write("\n".join(out) + "\n")
