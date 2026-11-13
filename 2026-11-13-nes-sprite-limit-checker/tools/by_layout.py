#!/usr/bin/env python3
"""Per-layout table from a nessprites run of the test ROM (MIT).

usage: by_layout.py frames.csv events.csv [mesen_frames.csv]
The ROM's flag is the one it read in its NMI, at the start of the next
frame, so it is compared with the model of the previous frame. The Mesen run
is aligned to the FCEUmm run by the ROM's own NMI counter ($07F1), because
the two cores end retro_run() at different points of the frame. Frames before
the second NMI (boot, power-up OAM) are skipped.
"""
import csv, sys
from collections import defaultdict

NAMES = ['eight', 'nine', 'twelve', 'rotate', 'tall', 'zeroed', 'flag_miss', 'flag_false', 'spread', 'rotate5']
def unwrap(rows):
    """Adds 'n' = NMI count since boot (the RAM byte wraps at 256). Rows before
    the first NMI (power-up RAM may hold anything there) get n = -1."""
    base, last, seen = 0, 0, False
    for r in rows:
        v = int(r['nmi'])
        if not seen:
            seen = v == 1 and last == 0
        elif v < last:
            base += 256
        r['n'] = base + v if seen else -1
        last = v
    return rows

rows = unwrap(list(csv.DictReader(open(sys.argv[1]))))
ev = list(csv.DictReader(open(sys.argv[2])))
mesen = {}
if len(sys.argv) > 3:
    for r in unwrap(list(csv.DictReader(open(sys.argv[3])))):
        if r['n'] >= 0: mesen.setdefault(r['n'], r)   # first frame that saw this NMI
lost_by = defaultdict(lambda: defaultdict(int))
fr_layout = {int(r['frame']): int(r['layout']) for r in rows}
start = next(int(r['frame']) for r in rows if r['n'] >= 2)
for e in ev:
    f = int(e['frame'])
    if f < start: continue
    lost_by[fr_layout[f]][int(e['x'])] += 1

g = defaultdict(lambda: defaultdict(int))
prev = None
for r in rows:
    f = int(r['frame'])
    if f < start:
        prev = r; continue
    L = int(r['layout'])
    d = g[L]
    d['frames'] += 1
    d['loss_frames'] += int(r['loss_lines']) > 0
    d['max_lost'] = max(d['max_lost'], int(r['sprites_lost']))
    d['max_on_line'] = max(d['max_on_line'], int(r['max_on_line']))
    d['exact'] += int(r['flag_exact'])
    d['hw'] += int(r['flag_hw'])
    for k in ('real_lines', 'tp', 'fp', 'fn', 'loss_lines'):
        d[k] += int(r[k])
    # flag read by the ROM in this frame's NMI belongs to the previous frame
    if prev is not None and int(prev['layout']) == L:
        d['pairs'] += 1
        d['rom_fceumm'] += int(r['flag_rom'])
        d['rom_eq_exact'] += int(r['flag_rom']) == int(prev['flag_exact'])
        k = r['n']
        if k in mesen and int(mesen[k]['layout']) == L:
            d['mesen_pairs'] += 1
            d['mesen'] += int(mesen[k]['flag_rom'])
            d['mesen_eq_hw'] += int(mesen[k]['flag_rom']) == int(prev['flag_hw'])
    prev = r

print('| Layout | Most on a line | Frames with loss | Sprites lost per frame (by X position) | Lines lost, predicted | Lines that really differ | Exact flag | Bug model | FCEUmm flag read by ROM' + (' | Mesen flag read by ROM' if mesen else '') + ' |')
print('|' + ' --- |' * (9 + bool(mesen)))
for L in sorted(g):
    d = g[L]
    ents = lost_by[L]
    if ents:
        lst = ', '.join(f'{k}' + ('' if v == d['frames'] else f' ({100*v/d["frames"]:.0f}%)') for k, v in sorted(ents.items()))
        if len(ents) > 8: lst = f'{len(ents)} objects'
    else:
        lst = '0'
    cells = [f'{L} {NAMES[L]}', d['max_on_line'], f"{d['loss_frames']}/{d['frames']}", f"{d['max_lost']}: {lst}",
             d['loss_lines'], f"{d['real_lines']} (missed {d['fn']})", f"{d['exact']}/{d['frames']}", f"{d['hw']}/{d['frames']}",
             f"{d['rom_fceumm']}/{d['pairs']}"]
    if mesen: cells.append(f"{d['mesen']}/{d['mesen_pairs']} (matches model: {d['mesen_eq_hw']}/{d['mesen_pairs']})")
    print('| ' + ' | '.join(str(c) for c in cells) + ' |')
