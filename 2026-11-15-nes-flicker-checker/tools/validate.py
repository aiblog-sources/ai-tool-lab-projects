"""Score flicker.py against the test ROM's own truth table (MIT).

usage: validate.py frames.csv truth.csv tracks.csv [--md out.md] [--json out.json]

For every frame, the truth buffer of the table that was shown (SHOWN_TAG & 1)
lists each object, whether it exists and whether the ROM put it in OAM. Each
existing 16x16 object is 4 sprites. A sprite the ROM left out is a truth "G";
the tracker's G frames (interpolated position) are matched to those within 3 px.
"""
import argparse
import csv
import json
from collections import defaultdict

NAMES = ["calm", "row6", "row6rot", "alt2", "alt3", "budget", "random", "spawn", "blink", "fast"]
TOL = 3


def longest(seq):
    best = cur = 0
    for v in seq + [0]:
        cur = cur + 1 if v else 0
        best = max(best, cur)
    return best


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("frames"); ap.add_argument("truth"); ap.add_argument("tracks")
    ap.add_argument("--md"); ap.add_argument("--json")
    a = ap.parse_args()
    tag = {int(r["frame"]): int(r["tag"]) for r in csv.DictReader(open(a.frames))}
    truth = {int(r["frame"]): bytes.fromhex(r["hex"]) for r in csv.DictReader(open(a.truth))}
    trk = defaultdict(list)          # frame -> [(state, x, y, track)]
    for r in csv.DictReader(open(a.tracks)):
        trk[int(r["frame"])].append((r["state"], float(r["x"]), float(r["y"]), int(r["track"])))

    L = defaultdict(lambda: defaultdict(int))
    skipped = defaultdict(lambda: defaultdict(list))     # layout -> object -> per-frame skipped flags
    trk_run = defaultdict(dict)                          # layout -> track -> states string
    for f in sorted(tag):
        if f < 10:
            continue
        lay = tag[f] >> 1
        if lay >= len(NAMES):
            continue
        buf = truth[f][(tag[f] & 1) * 64:(tag[f] & 1) * 64 + 64]
        n = buf[0]
        if n > 16:
            continue
        tg = []
        tin = 0
        for i in range(n):
            fl, x, y = buf[1 + i], buf[17 + i], buf[33 + i]
            if not fl & 2:
                continue
            skipped[lay][i].append(0 if fl & 1 else 1)
            for dx, dy in ((0, 0), (8, 0), (0, 8), (8, 8)):
                if fl & 1:
                    tin += 1
                else:
                    tg.append((x + dx, y + dy))
        L[lay]["sprite_frames"] += tin + len(tg)
        L[lay]["truth_skipped"] += len(tg)
        mine = [(x, y) for s, x, y, t in trk[f] if s == "G"]
        for s, x, y, t in trk[f]:
            trk_run[lay][t] = trk_run[lay].get(t, "") + s
        L[lay]["tracker_G"] += len(mine)
        L[lay]["tracker_PPU"] += sum(1 for s, *_ in trk[f] if s == "D")
        used = set()
        for (tx, ty) in tg:
            hit = None
            for j, (mx, my) in enumerate(mine):
                if j not in used and abs(mx - tx) <= TOL and abs(my - ty) <= TOL:
                    hit = j
                    break
            if hit is None:
                L[lay]["missed"] += 1
            else:
                used.add(hit)
                L[lay]["caught"] += 1
        L[lay]["false"] += len(mine) - len(used)

    # skips before an object's first frame in OAM or after its last (in that
    # layout) cannot be seen from OAM: count how many of the misses are those
    for lay in skipped:
        for i, seq in skipped[lay].items():
            shown_at = [k for k, v in enumerate(seq) if not v]
            if not shown_at:
                L[lay]["edge"] += 4 * len(seq)
                continue
            L[lay]["edge"] += 4 * (shown_at[0] + (len(seq) - 1 - shown_at[-1]))
    rows = []
    for lay in sorted(L):
        d = L[lay]
        tl = max([longest(v) for v in skipped[lay].values()] + [0])
        ml = 0
        for s in trk_run[lay].values():
            cur = 0
            for c in s + "S":
                cur = cur + 1 if c in "G" else 0
                ml = max(ml, cur)
        rows.append(dict(layout=NAMES[lay], sprite_frames=d["sprite_frames"], truth_skipped=d["truth_skipped"],
                         caught=d["caught"], missed=d["missed"], missed_edge=min(d["edge"], d["missed"]), false=d["false"], ppu_dropped=d["tracker_PPU"],
                         truth_longest=tl, tracker_longest=ml))
    hdr = ["layout", "sprite_frames", "truth_skipped", "caught", "missed", "missed_edge", "false", "ppu_dropped", "truth_longest", "tracker_longest"]
    out = ["| " + " | ".join(hdr) + " |", "|" + " --- |" * len(hdr)]
    for r in rows:
        out.append("| " + " | ".join(str(r[h]) for h in hdr) + " |")
    tot = {h: sum(r[h] for r in rows) for h in ("sprite_frames", "truth_skipped", "caught", "missed", "missed_edge", "false", "ppu_dropped")}
    out.append(f"\ntotal: {json.dumps(tot)}")
    print("\n".join(out))
    if a.md:
        open(a.md, "w").write("\n".join(out) + "\n")
    if a.json:
        json.dump(dict(layouts=rows, total=tot), open(a.json, "w"), indent=1)


main()
