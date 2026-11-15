"""flicker.py: how long each NES sprite stays invisible, and who hid it (MIT).

Input: the --oam log of nesflicker (one row per OAM entry with an opaque row on
screen, per frame). The tracker sees only OAM, never the game's own object list:

  - a sprite is identified by its tile and attribute bits (palette, flips) and
    followed from frame to frame by its predicted position (last position +
    last velocity); each frame, sprites and live tracks with the same key are
    paired by a minimum-cost assignment (Hungarian method) on the distance to
    the prediction, within RADIUS + (STEP + |v|) * frames_since_seen pixels
    (the |v| term lets a sprite reverse direction, as at a bounce)
  - a sprite that is missing from OAM and comes back within GAP_MAX frames
    near where it should be was skipped by the game in between (software
    flicker); if it does not come back, it left (died, scrolled off)
  - a gap only counts if nothing identical was drawn where the sprite should
    be: on every gap frame, no sprite with the same key may be in OAM within
    NEAR px of the interpolated position. Otherwise the gap is an identity
    mix-up (identical sprites that jumped further than the radius, as in a
    30 fps orbit), and the track is split there instead
  - a sprite that is in OAM but has none of its opaque rows drawn was dropped
    by the PPU (more than 8 on its lines); some rows drawn = partly drawn

Limit: identity comes from position only. Identical sprites closer than CROWD
px (a snake's segments, stacked sprites) can swap identities, which makes
per-sprite hidden stretches meaningless there; the report gives the share of
sprite-frames with such a twin and warns when it is high.

Every frame of every sprite's life gets one state:
  S shown, P partly drawn, D dropped by the PPU, G skipped by the game.

usage: flicker.py oam.csv [--from F] [--to T] [--tracks out.csv] [--json out.json] [--report]
"""
import argparse
import csv
import json
import sys
from collections import defaultdict

GAP_MAX = 8      # frames a sprite may be missing and still be the same sprite
RADIUS = 8       # px, match radius for a sprite seen in the previous frame (6 left false gaps in a 30 fps orbit)
STEP = 2         # px added per frame since it was last seen
NEAR = 6         # px; an identical sprite this close means the 'hidden' one was there
CROWD = 4        # px; an identical sprite this close makes identity ambiguous
MIN_LIFE = 10    # frames; shorter tracks are left out of the stats
VMAX = 8         # px per frame, clamp on the velocity estimate


class Track:
    __slots__ = ("id", "key", "x", "y", "vx", "vy", "last", "first", "obs")

    def __init__(self, tid, key, f, x, y):
        self.id, self.key = tid, key
        self.x, self.y, self.vx, self.vy = x, y, 0.0, 0.0
        self.first = self.last = f
        self.obs = []    # (frame, x, y, state)


TALL = {}         # frame -> 1 if 8x16 sprites


def load(path, f0, f1):
    frames = defaultdict(list)
    last = -1
    for r in csv.DictReader(open(path)):
        f = int(r["frame"])
        last = max(last, f)
        if f < f0 or f > f1:
            continue
        rows, drawn = int(r["rows"]), int(r["drawn"])
        st = "S" if drawn == rows else ("D" if drawn == 0 else "P")
        TALL[f] = int(r.get("tall", 0) or 0)
        frames[f].append(((int(r["tile"]), int(r["attr"]) & 0xC3), int(r["x"]), int(r["y"]), st))
    return frames, last


BIG = 1e9
NEW = 1000.0     # cost of leaving a sprite unmatched (starts a new track)


def hungarian(cost):
    """min-cost assignment of every row to a distinct column (rows <= columns); returns col per row"""
    n, m = len(cost), len(cost[0])
    u, v, p, way = [0.0] * (n + 1), [0.0] * (m + 1), [0] * (m + 1), [0] * (m + 1)
    for i in range(1, n + 1):
        p[0], j0 = i, 0
        minv, used = [float("inf")] * (m + 1), [False] * (m + 1)
        while True:
            used[j0] = True
            i0, delta, j1 = p[j0], float("inf"), 0
            for j in range(1, m + 1):
                if not used[j]:
                    cur = cost[i0 - 1][j - 1] - u[i0] - v[j]
                    if cur < minv[j]:
                        minv[j], way[j] = cur, j0
                    if minv[j] < delta:
                        delta, j1 = minv[j], j
            for j in range(m + 1):
                if used[j]:
                    u[p[j]] += delta; v[j] -= delta
                else:
                    minv[j] -= delta
            j0 = j1
            if p[j0] == 0:
                break
        while j0:
            j1 = way[j0]; p[j0] = p[j1]; j0 = j1
    res = [0] * n
    for j in range(1, m + 1):
        if p[j]:
            res[p[j] - 1] = j - 1
    return res


def track(frames, f0, f1):
    tracks, live, nid = [], [], 0
    for f in range(f0, f1 + 1):
        live = [t for t in live if f - t.last <= GAP_MAX + 1]
        pieces = frames.get(f, [])
        used_p = set()
        for key in {pc[0] for pc in pieces}:
            pis = [i for i, pc in enumerate(pieces) if pc[0] == key]
            tis = [i for i, t in enumerate(live) if t.key == key]
            if not tis:
                continue
            # rows: sprites; columns: live tracks, then one "new track" column per sprite
            cost = []
            for pi in pis:
                _, x, y, _ = pieces[pi]
                row = []
                for ti in tis:
                    t = live[ti]
                    dt = f - t.last
                    d = max(abs(x - (t.x + t.vx * dt)), abs(y - (t.y + t.vy * dt)))
                    lim = RADIUS + (STEP + max(abs(t.vx), abs(t.vy))) * dt
                    row.append(d + 0.5 * (dt - 1) if d <= lim else BIG)
                row += [NEW] * len(pis)
                cost.append(row)
            for r, c in enumerate(hungarian(cost)):
                if c >= len(tis) or cost[r][c] >= BIG:
                    continue
                pi, t = pis[r], live[tis[c]]
                used_p.add(pi)
                key, x, y, st = pieces[pi]
                dt = f - t.last
                t.vx = max(-VMAX, min(VMAX, (x - t.x) / dt))
                t.vy = max(-VMAX, min(VMAX, (y - t.y) / dt))
                t.x, t.y, t.last = x, y, f
                t.obs.append((f, x, y, st))
        for pi, (key, x, y, st) in enumerate(pieces):
            if pi in used_p:
                continue
            t = Track(nid, key, f, x, y)
            nid += 1
            t.obs.append((f, x, y, st))
            tracks.append(t)
            live.append(t)
    return tracks


def segments(t, count):
    """the track as runs of per-frame (frame, state, x, y); gaps become G frames at
    interpolated positions, or a split when an identical sprite was drawn there"""
    segs, out = [], []
    for (fa, xa, ya, sa), nxt in zip(t.obs, t.obs[1:] + [None]):
        out.append((fa, sa, xa, ya))
        if not nxt:
            continue
        fb, xb, yb, _ = nxt
        if fb == fa + 1:
            continue
        gap = []
        for g in range(fa + 1, fb):
            a = (g - fa) / (fb - fa)
            gap.append((g, "G", xa + (xb - xa) * a, ya + (yb - ya) * a))
        if not any(abs(x - gx) <= NEAR and abs(y - gy) <= NEAR for g, _, gx, gy in gap for x, y in count[(g, t.key)]):
            out += gap
        else:
            segs.append(out)
            out = []
    segs.append(out)
    return segs


def runs(states, hidden="DG"):
    best = cur = 0
    count = defaultdict(int)
    for s in states + "S":
        if s in hidden:
            cur += 1
        else:
            if cur:
                count[cur] += 1
            best, cur = max(best, cur), 0
    return best, count


def kind_count(frames):
    """(frame, key) -> positions of the sprites with that key in OAM"""
    count = defaultdict(list)
    for f, pcs in frames.items():
        for key, x, y, _ in pcs:
            count[(f, key)].append((x, y))
    return count


def stats(tracks, count):
    per = []
    tot = defaultdict(int)
    run_hist = defaultdict(int)
    wanted = defaultdict(list)       # frame -> OAM Y of every sprite the program wanted on screen
    counted = []                     # (frame, Y) of the sprite-frames in the stats
    for (f, key), pos in count.items():
        wanted[f] += [y for x, y in pos]
    for t, tl in ((t, tl) for t in tracks for tl in segments(t, count)):
        if len(tl) < MIN_LIFE:
            continue
        for f, st, x, y in tl:
            counted.append((f, round(y)))
            if st == "G":
                wanted[f].append(round(y))
        s = "".join(x[1] for x in tl)
        longest, cnt = runs(s)
        for k, v in cnt.items():
            run_hist[k] += v
        for c in "SPDG":
            tot[c] += s.count(c)
        per.append(dict(track=t.id, first=tl[0][0], last=tl[-1][0], tile=t.key[0], attr=t.key[1], life=len(s),
                        shown=s.count("S"), partly=s.count("P"), ppu=s.count("D"), game=s.count("G"),
                        hidden_pct=round(100 * (s.count("D") + s.count("G")) / len(s), 1), longest_hidden=longest,
                        x=tl[0][2], y=tl[0][3], pattern=s))
    n = sum(tot.values())
    # best case: with perfect cycling a sprite can be drawn at most 8/k of the
    # frames, k = the most sprites wanted on any of its lines (an upper bound)
    lines = {}
    for f, ys in wanted.items():
        h = 16 if TALL.get(f) else 8
        c = [0] * 241
        for y in ys:
            for l in range(y + 1, min(y + 1 + h, 241)):
                c[l] += 1
        lines[f] = c
    best = 0.0
    for f, y in counted:
        h = 16 if TALL.get(f) else 8
        k = max(lines[f][y + 1:min(y + 1 + h, 241)] + [0])
        best += 1.0 if k <= 8 else 8 / k
    twins = total = 0
    for (f, key), pos in count.items():
        for i, (x, y) in enumerate(pos):
            total += 1
            if any(j != i and abs(x - x2) <= CROWD and abs(y - y2) <= CROWD for j, (x2, y2) in enumerate(pos)):
                twins += 1
    agg = dict(tracks=len(per), sprite_frames=n, twin_pct=round(100 * twins / total, 1) if total else 0,
               best_shown_pct=round(100 * best / n, 1) if n else 0,
               shown_pct=round(100 * tot["S"] / n, 1) if n else 0,
               partly_pct=round(100 * tot["P"] / n, 1) if n else 0,
               ppu_pct=round(100 * tot["D"] / n, 1) if n else 0,
               game_pct=round(100 * tot["G"] / n, 1) if n else 0,
               hidden_pct=round(100 * (tot["D"] + tot["G"]) / n, 1) if n else 0,
               tracks_never_hidden=sum(1 for p in per if p["longest_hidden"] == 0),
               tracks_longest_1=sum(1 for p in per if p["longest_hidden"] == 1),
               tracks_longest_2_3=sum(1 for p in per if 2 <= p["longest_hidden"] <= 3),
               tracks_longest_4plus=sum(1 for p in per if p["longest_hidden"] >= 4),
               longest_hidden=max([p["longest_hidden"] for p in per] + [0]),
               hidden_runs={str(k): run_hist[k] for k in sorted(run_hist)})
    return agg, per


def report(agg, per, f0, f1, fps=60):
    w = []
    w.append(f"nesflicker report, frames {f0}-{f1} ({(f1 - f0 + 1) / fps:.1f} s at 60 fps)")
    w.append(f"Sprites followed: {agg['tracks']} (each one hardware sprite, 8x8 or 8x16; a 16x16 object is 4 of them)")
    w.append(f"Sprite-frames: {agg['sprite_frames']}. Shown {agg['shown_pct']}%, partly drawn {agg['partly_pct']}%, "
             f"hidden {agg['hidden_pct']}% (dropped by the PPU's 8-per-line limit {agg['ppu_pct']}%, "
             f"left out of OAM by the program {agg['game_pct']}%).")
    w.append(f"Best case with 8 sprites per line: about {agg['best_shown_pct']}% of these sprite-frames can be fully shown "
             f"(each sprite, 8 / the most sprites wanted on one of its lines: an estimate of the most any cycling scheme can show; "
             f"it counts transparent rows too, so it can sit a few tenths below the real share).")
    w.append(f"Longest stretch one sprite stayed hidden: {agg['longest_hidden']} frames.")
    if agg["twin_pct"] >= 5:
        w.append(f"WARNING: {agg['twin_pct']}% of the sprites in OAM had an identical sprite (same tile and attributes) within "
                 f"{CROWD} px. The tracker cannot tell those apart, so per-sprite hidden stretches are unreliable here "
                 f"(the hidden share is still right).")
    w.append(f"Sprites by longest hidden stretch: never hidden {agg['tracks_never_hidden']}, 1 frame {agg['tracks_longest_1']}, "
             f"2-3 frames {agg['tracks_longest_2_3']}, 4 or more {agg['tracks_longest_4plus']}.")
    if agg["hidden_runs"]:
        w.append("Hidden stretches by length (frames: count): " + ", ".join(f"{k}: {v}" for k, v in agg["hidden_runs"].items()))
    worst = sorted(per, key=lambda p: (-p["longest_hidden"], -p["hidden_pct"]))[:8]
    worst = [p for p in worst if p["longest_hidden"]]
    if worst:
        w.append("Worst sprites (pattern of the first 48 frames of their life: S shown, P partly, D dropped by PPU, G left out by the program):")
        for p in worst:
            w.append(f"  tile ${p['tile']:02X} attr ${p['attr']:02X} from x={p['x']:.0f} y={p['y']:.0f}, frames {p['first']}-{p['last']}: "
                     f"hidden {p['hidden_pct']}% (PPU {p['ppu']}, program {p['game']}), longest {p['longest_hidden']} frames  "
                     f"{p['pattern'][:48]}")
    return "\n".join(w)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("oam")
    ap.add_argument("--from", dest="f0", type=int, default=0)
    ap.add_argument("--to", dest="f1", type=int, default=None)
    ap.add_argument("--tracks")
    ap.add_argument("--json")
    ap.add_argument("--report", action="store_true")
    a = ap.parse_args()
    frames, last = load(a.oam, a.f0, a.f1 if a.f1 is not None else 10 ** 9)
    f1 = a.f1 if a.f1 is not None else last
    tracks = track(frames, a.f0, f1)
    count = kind_count(frames)
    agg, per = stats(tracks, count)
    if a.tracks:
        with open(a.tracks, "w", newline="") as fh:
            cw = csv.writer(fh)
            cw.writerow(["track", "frame", "state", "x", "y", "tile", "attr"])
            sid = 0
            for t in tracks:
                for seg in segments(t, count):
                    for f, s, x, y in seg:
                        cw.writerow([sid, f, s, round(x, 1), round(y, 1), t.key[0], t.key[1]])
                    sid += 1
    if a.json:
        json.dump(dict(summary=agg, sprites=[{k: v for k, v in p.items() if k != "pattern"} for p in per]),
                  open(a.json, "w"), indent=1)
    if a.report or not (a.json or a.tracks):
        print(report(agg, per, a.f0, f1))
    else:
        print(json.dumps(agg))


if __name__ == "__main__":
    main()
