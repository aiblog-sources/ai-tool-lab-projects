"""Per-phase summary of the test ROM runs (mddma CSVs). Usage: analyze.py ntsc.csv [pal.csv]"""
import csv, os, statistics, sys

# same table as testrom/src/main.c: (KB of tiles per frame, ignore over capacity, H32, SAT first, split)
PHASES = [
    (0, 0, 0, 0, 0), (2, 0, 0, 0, 0), (4, 0, 0, 0, 0), (6, 0, 0, 0, 0), (7, 0, 0, 0, 0),
    (8, 0, 0, 0, 0), (10, 0, 0, 0, 0), (12, 0, 0, 0, 0), (16, 0, 0, 0, 0),
    (8, 1, 0, 0, 0), (10, 1, 0, 0, 0), (12, 1, 0, 0, 0), (16, 1, 0, 0, 0),
    (12, 0, 0, 1, 0), (12, 1, 0, 1, 0), (12, 0, 0, 0, 1),
    (4, 0, 1, 0, 0), (6, 0, 1, 0, 0), (8, 0, 1, 0, 0), (12, 0, 1, 0, 0),
]
SKIP = 4  # frames left out at the start and end of each phase (mode switch, plane redraw)


def label(p):
    kb, ign, h32, satf, split = PHASES[p - 1]
    parts = [f"{kb:>2} KB", "H32" if h32 else "H40", "ignore" if ign else "default"]
    if satf:
        parts.append("SAT first")
    if split:
        parts.append("split over 2 frames")
    return " ".join(parts)


def phases_of(path):
    by = {}
    for r in csv.DictReader(open(path)):
        p = int(r["phase"])
        if 1 <= p <= len(PHASES):
            by.setdefault(p, []).append(r)
    return {p: fr[SKIP:-SKIP] for p, fr in by.items()}


def med(xs):
    return statistics.median(xs) if xs else -1


def report(path):
    print(f"\n{os.path.basename(path)}")
    print("loops/s: game loops (frames with a DMA flush) per second; queued: SGDK queue per loop (median);")
    print("start: line of the first DMA write; vblank/display: bytes sent per frame (median of flush frames);")
    print("last: last display line with DMA (max); lost: (queued - sent) per loop over the phase;")
    print("SAT ok: loops whose sprite table reached the VDP; SAT in disp: those landing after line 0 (median line)\n")
    print(f"{'phase':<40} {'loops/s':>7} {'queued':>6} {'start':>5} {'vblank':>6} {'display':>7} {'last':>4} "
          f"{'lost':>5} {'SAT ok':>9} {'SAT in disp':>12} {'load':>5}")
    hz = None
    for p, fr in sorted(phases_of(path).items()):
        hz = 50 if int(fr[0]["lines"]) == 313 else 60
        g = lambda k, rows=fr: [int(r[k]) for r in rows]
        fl = [r for r in fr if int(r["flushes"]) > 0]
        loops = len(fl)
        q = g("queued", fl)
        sent = sum(g("sent_vblank")) + sum(g("sent_active"))
        lost = max(0, sum(g("queued")) - sent) / max(1, loops)
        last = [x for x in g("last_active_line") if x >= 0]
        sat_ok = sum(1 for x in g("sat_all") if x > 0)
        satl = [x for x in g("sat_line") if x >= 0]
        load = statistics.mean(float(r["load_pct"]) for r in fr)
        idx = [i for i, r in enumerate(fr) if int(r["flushes"]) > 0]
        span = idx[-1] - idx[0] + 1 if idx else len(fr)  # video frames from the first to the last loop
        print(f"{p:>2} {label(p):<37} {hz * loops / span:>7.1f} {med(q):>6.0f} {med(g('first_dma_line', fl)):>5.0f} "
              f"{med(g('sent_vblank', fl)):>6.0f} {med(g('sent_active', fl)):>7.0f} {(max(last) if last else -1):>4} "
              f"{lost:>5.0f} {sat_ok:>4}/{loops:<4} {len(satl):>4} ({med(satl):>4.0f}) {load:>5.1f}")


for path in sys.argv[1:]:
    report(path)
