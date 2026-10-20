#!/usr/bin/env python3
"""Context cost: input tokens of a one-turn prompt under each loading mode, 3 launches each.
Usage: python3 bench/overhead.py <corpus> <work> <out.json> [model]"""
import json, sys
from pathlib import Path
from claude_run import claude_retry

corpus, work, out = sys.argv[1:4]
model = sys.argv[4] if len(sys.argv) > 4 else "sonnet"
conds = json.loads((Path(__file__).parent / "conditions.json").read_text())
conds = {"none": {"server": None}, "none+off": {"server": None, "claude_env": {"ENABLE_TOOL_SEARCH": "false"}},
         **{k: conds[k] for k in ["good", "good+tool", "good+server", "good+off"]},
         "good+both": {**conds["good"], "env": {"ALWAYS_LOAD": "search"}, "config": {"alwaysLoad": True}},
         "good+tool+server-false": {**conds["good+tool"], "config": {"alwaysLoad": False}}}
rows = []
for rep in range(3):
    for name, c in conds.items():
        r = claude_retry("Reply with the single word OK.", name, c, corpus, work, model)
        rows.append(r)
        print(name, r["mcp_status"], r["init_tools"], r["first_input_tokens"], r["wall_s"], r["answer"][:40], flush=True)
Path(out).write_text(json.dumps(rows, indent=2))
