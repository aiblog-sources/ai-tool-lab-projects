"""Tables for the README and the post, from the saved runs (MIT).

Recomputes every flicker summary from the gzipped OAM logs with the current
tools/flicker.py, so the tables always match the tracker in this folder.
usage: summary.py  (writes results/claude-scenes-table.md and results/ai-fix-table.md)
"""
import csv
import gzip
import json
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent
OLD = HERE.parent / "2026-11-13-nes-sprite-limit-checker" / "ai-test" / "table.json"
FROM = 60


def flick(gz, rep_out=None):
    with tempfile.NamedTemporaryFile("w", suffix=".csv") as t, tempfile.NamedTemporaryFile("r", suffix=".json") as j:
        t.write(gzip.open(gz, "rt").read())
        t.flush()
        subprocess.run([sys.executable, str(HERE / "tools/flicker.py"), t.name, "--from", str(FROM), "--json", j.name],
                       check=True, capture_output=True)
        if rep_out:
            Path(rep_out).with_suffix(".json").write_text(Path(j.name).read_text())
            r = subprocess.run([sys.executable, str(HERE / "tools/flicker.py"), t.name, "--from", str(FROM), "--report"],
                               check=True, capture_output=True, text=True).stdout
            Path(rep_out).write_text(r)
        return json.load(open(j.name))["summary"]


def lag(frames_csv):
    v = [int(r["lag"]) for r in csv.DictReader(open(frames_csv)) if int(r["frame"]) >= FROM]
    return sum((b - a) % 256 for a, b in zip(v, v[1:]))    # one-byte counter: sum the deltas


def md(hdr, rows):
    return "\n".join(["| " + " | ".join(hdr) + " |", "|" + " --- |" * len(hdr)] +
                     ["| " + " | ".join(str(c) for c in r) + " |" for r in rows]) + "\n"


def main():
    old = {(r["id"], r["stage"]): r for r in json.load(open(OLD))}
    rows, data = [], {}
    for d in sorted((HERE / "claude-scenes").iterdir()):
        if not d.is_dir():
            continue
        for st in ("v0", "v1"):
            gz = d / f"{st}-oam.csv.gz"
            if not gz.exists():
                continue
            s = flick(gz, d / f"{st}-flicker.txt")
            lg = lag(d / f"{st}-frames.csv")
            data[f"{d.name}/{st}"] = dict(s, lag=lg)
            rows.append([d.name, st, f"{old[(d.name, st)]['pct_lost']}%", f"{s['shown_pct']}%", f"{s['best_shown_pct']}%",
                         f"{s['ppu_pct']}%", f"{s['game_pct']}%", s["longest_hidden"],
                         f"{s['twin_pct']}%", lg])
    hdr = ["scene", "version", "11-13 checker: frames losing pixels", "shown", "best case", "hidden by PPU",
           "hidden by program", "longest hidden (frames)", "identical sprite within 4 px", "lag frames"]
    (HERE / "results/claude-scenes-table.md").write_text(md(hdr, rows))
    (HERE / "results/claude-scenes-summary.json").write_text(json.dumps(data, indent=1))

    rows, data, usd = [], {}, 0.0
    for d in sorted((HERE / "ai-fix").iterdir()):
        if not (d / "result.json").exists():
            continue
        r = json.loads((d / "result.json").read_text())
        usd += r["cost_usd"]
        b = data.get(f"{r['scene']}/before") or dict(
            flick(HERE / "claude-scenes" / r["scene"] / f"{r['from_stage']}-oam.csv.gz"),
            lag=lag(HERE / "claude-scenes" / r["scene"] / f"{r['from_stage']}-frames.csv"))
        data[f"{r['scene']}/before"] = b
        a = dict(flick(d / "fix-oam.csv.gz", d / "fix-flicker.txt"), lag=lag(d / "fix-frames.csv"))
        data[d.name] = dict(a, cost_usd=r["cost_usd"], build_rounds=r["build_rounds"])
        rows.append([r["scene"], r["rep"], f"{b['shown_pct']}% -> {a['shown_pct']}%", f"{a['best_shown_pct']}%",
                     f"{b['longest_hidden']} -> {a['longest_hidden']}", f"{b['lag']} -> {a['lag']}",
                     f"{a['twin_pct']}%", r["build_rounds"], r["cost_usd"]])
    hdr = ["scene", "run", "shown", "best case", "longest hidden (frames)", "lag frames", "identical within 4 px",
           "build fix rounds", "USD"]
    (HERE / "results/ai-fix-table.md").write_text(md(hdr, rows) + f"\ntotal USD {usd:.2f}\n")
    (HERE / "results/ai-fix-summary.json").write_text(json.dumps(data, indent=1))
    print((HERE / "results/claude-scenes-table.md").read_text())
    print((HERE / "results/ai-fix-table.md").read_text())


main()
