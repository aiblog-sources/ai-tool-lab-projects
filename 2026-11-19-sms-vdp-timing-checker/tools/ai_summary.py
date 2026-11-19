"""Markdown table of ai-test/summary.json (MIT)."""
import json, pathlib
HERE = pathlib.Path(__file__).resolve().parent.parent
import sys
rows = json.loads((HERE / (sys.argv[1] if len(sys.argv) > 1 else "ai-test") / "summary.json").read_text())
def cell(v):
    if not v:
        return "", "", "", ""
    if not v.get("builds"):
        return "no build", "", "", ""
    blank = f"{v['frames_blank']} (max {v['max_blank']})" if v.get("frames_blank") else "0"
    return (f"{v['active']}", f"{v['lt23']} / {v['lt26']} / {v['lt29']}", blank,
            ("UNSAFE_ " if v.get("uses_unsafe") else "") + ("raw ports" if v.get("raw_port") else ""))
print("| Scene | Prompt | Active-display writes | Under 23 / 26 / 29 | Frames with blank lines (max lines) | Uses | After fix round |")
print("| --- | --- | --- | --- | --- | --- | --- |")
tot = 0
for r in rows:
    a = cell(r["v0"])
    fix = ""
    if "v1" in r:
        b = cell(r["v1"])
        fix = f"active {b[0]}, under 26: {b[1].split(' / ')[1] if b[1] else '?'}, blank frames {b[2]}"
    print(f"| {r['scene']} | {r['variant']} | {a[0]} | {a[1]} | {a[2]} | {a[3]} | {fix} |")
    tot += r.get("cost_usd", 0)
print(f"\nTotal: {len(rows)} programs, {sum(r['calls'] for r in rows)} model calls, USD {tot:.2f} at API prices")
