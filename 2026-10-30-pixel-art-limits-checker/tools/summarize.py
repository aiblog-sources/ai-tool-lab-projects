#!/usr/bin/env python3
"""Re-check every image of the AI test with the current pixcheck, copy the scripts, images and reports into
ai-test/, and write results/ai-test.json and results/ai-test-summary.md.

The runs used pixcheck as it was during the run for the fix-round feedback; this script re-scores every stage with
the final version (exact packing fallback, NES colors taken as given), so the tables use one checker throughout.
usage: summarize.py    MIT license."""
import json
import os
import shutil
import sys
from collections import defaultdict
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(HERE / "pixcheck"))
import pixcheck  # noqa: E402

WORK = Path(os.environ.get("WORK", Path.home() / ".cache/pixcheck")) / "ai"
SPRITE = {"nes": 16, "sms": 16, "snes": 32, "md": 32}
STAGES = ["v0", "plain1", "check1", "check2"]

runs = []
for variant in ("limits", "naive"):
    f = HERE / "results" / f"ai-test-{variant}.json"
    if not f.exists():
        continue
    for r in json.loads(f.read_text()):
        r["variant"] = variant
        if not r["tag"].startswith(variant + "-"):
            r["tag"] = f"{variant}-{r['tag']}"
        runs.append(r)

out_dir = HERE / "ai-test"
for r in runs:
    src = WORK / r["tag"]
    dst = out_dir / r["tag"]
    sprite = (SPRITE[r["console"]],) * 2 if r["task"] == "knight" else None
    for st in STAGES:
        if st not in r:
            continue
        sd = src / st
        dd = dst / st
        dd.mkdir(parents=True, exist_ok=True)
        shutil.copy(sd / "script.py", dd / "script.py")
        png = sd / "art.png"
        r[st]["recheck"] = None
        if png.exists():
            shutil.copy(png, dd / "art.png")
            rep, _, _ = pixcheck.check(r["console"], str(png), sprite)
            rep["file"] = "art.png"
            (dd / "pixcheck.txt").write_text(pixcheck.text_report(rep) + "\n")
            (dd / "pixcheck.json").write_text(json.dumps(rep, indent=1))
            if not rep["ok"]:
                pixcheck.overlay(str(png), rep, str(dd / "overlay.png"))
            r[st]["recheck"] = {"ok": rep["ok"], "errors": rep["errors"], "warnings": rep["warnings"],
                                "info": rep["info"]}
        elif (sd / "run.log").exists():
            shutil.copy(sd / "run.log", dd / "run.log")
    if (src / "v0").exists():
        # the prompt and the model's full reply are not kept; the script is what the model wrote
        pass

(HERE / "results" / "ai-test.json").write_text(json.dumps(runs, indent=1))


def ok(r, st):
    return bool(r.get(st) and r[st].get("recheck") and r[st]["recheck"]["ok"])


lines = ["# AI test summary (re-checked with the final pixcheck)", ""]
lines.append("First try (v0) passes, per variant and model, out of 12 (3 tasks x 4 consoles):")
lines.append("")
lines.append("| Variant | Model | Ran | Pass v0 | Fixed by plain round | Fixed by pixcheck rounds | Cost USD | Calls |")
lines.append("| --- | --- | --- | --- | --- | --- | --- | --- |")
groups = defaultdict(list)
for r in runs:
    groups[(r["variant"], r["model"])].append(r)
for (v, m), rs in sorted(groups.items()):
    ran = sum(1 for r in rs if r["v0"].get("recheck"))
    p0 = sum(ok(r, "v0") for r in rs)
    failed = [r for r in rs if not ok(r, "v0")]
    plain = sum(ok(r, "plain1") for r in failed)
    check = sum(ok(r, "check1") or ok(r, "check2") for r in failed)
    lines.append(f"| {v} | {m.replace('claude-', '')} | {ran}/{len(rs)} | {p0} | {plain}/{len(failed)} | "
                 f"{check}/{len(failed)} | {sum(r['cost_usd'] for r in rs):.2f} | {sum(r['calls'] for r in rs)} |")
lines += ["", "Per job (v0 re-check; colors, palettes used of allowed, unique tiles of budget; first error):", "",
          "| Job | v0 | Colors | Palettes | Tiles | First error | Fix stages (plain1 / check1 / check2) |",
          "| --- | --- | --- | --- | --- | --- | --- |"]
for r in sorted(runs, key=lambda r: r["tag"]):
    rc = r["v0"].get("recheck")
    if not rc:
        lines.append(f"| {r['tag']} | did not run | | | | {r['v0'].get('error')} | |")
        continue
    i = rc["info"]
    fixes = " / ".join(("pass" if ok(r, s) else ("fail" if r[s].get("recheck") else "no image")) if s in r else "-"
                       for s in ("plain1", "check1", "check2"))
    lines.append(f"| {r['tag']} | {'pass' if rc['ok'] else 'FAIL'} | {i['console_colors']} | "
                 f"{i.get('palettes_used')}/{i.get('palettes_allowed')} | {i.get('unique_tiles')}/{i.get('tile_budget')} | "
                 f"{(rc['errors'] or [''])[0]} | {fixes} |")
(HERE / "results" / "ai-test-summary.md").write_text("\n".join(lines) + "\n")
print("\n".join(lines))
