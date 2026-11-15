"""Ground truth for the snake scenes, from the trail instead of the tracker (MIT).

In these programs the head is the one sprite with palette 1 and segment k sits where the head was
4k frames earlier, so a segment can be found by position history alone. A
segment counts as shown in a frame if a sprite at that exact position was fully
drawn (stacked segments at one spot count as one). Prints the longest hidden
stretch and hidden share per segment.
usage: snake_truth.py oam.csv.gz
"""
import csv, gzip, sys, collections
p = sys.argv[1]
rows = collections.defaultdict(list)
for r in csv.DictReader(gzip.open(p, 'rt')):
    f = int(r['frame'])
    if f < 60: continue
    rows[f].append((int(r['attr']) & 3 == 1, int(r['x']), int(r['y']), int(r['rows']), int(r['drawn']), int(r['tile'])))
head = {}
for f, es in rows.items():
    e0 = [e for e in es if e[0]]
    if e0: head[f] = (e0[0][1], e0[0][2])
best = {}
amb = 0
for k in range(1, 20):
    run = longest = hidden = n = 0
    for f in sorted(rows):
        if f - 4 * k not in head: continue
        pos = head[f - 4 * k]
        at = [e for e in rows[f] if (e[1], e[2]) == pos and not e[0]]
        if not at: continue
        if len(at) > 1: amb += 1
        n += 1
        shown = any(e[4] == e[3] for e in at)
        if shown: run = 0
        else: run += 1; hidden += 1
        longest = max(longest, run)
    best[k] = (longest, round(100 * hidden / max(n, 1), 1), n)
print('max longest', max(v[0] for v in best.values()), 'ambiguous frames', amb)
print({k: v for k, v in best.items()})
