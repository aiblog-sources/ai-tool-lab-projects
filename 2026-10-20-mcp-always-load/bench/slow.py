#!/usr/bin/env python3
"""A slow server: STARTUP_DELAY_MS holds back the stdio connection. Does a per-tool alwaysLoad still load upfront?
Usage: python3 bench/slow.py <corpus> <work> <out.json> [model]"""
import json, sys
from pathlib import Path
from claude_run import claude_retry

corpus, work, out = sys.argv[1:4]
model = sys.argv[4] if len(sys.argv) > 4 else "sonnet"
conds = json.loads((Path(__file__).parent / "conditions.json").read_text())
q = "What generation speed in tokens per second did Gemma 4 E2B reach on the 2-core laptop? Answer in one or two sentences."
rows = []
for rep in range(2):
    for delay in ["0", "10000", "25000"]:
        for stream in [False, True]:
            for name in ["good", "good+tool", "good+server"]:
                c = {**conds[name], "env": {**conds[name]["env"], "STARTUP_DELAY_MS": delay}, "stream_input": stream}
                r = {"delay_ms": int(delay), "stream_input": stream, **claude_retry(q, name, c, corpus, work, model)}
                rows.append(r)
                print(delay, stream, name, r["mcp_status"], r["first_input_tokens"], [t["name"] for t in r["tools"]],
                      r["wall_s"], r["answer"][:80].replace("\n", " "), flush=True)
Path(out).write_text(json.dumps(rows, indent=2))
