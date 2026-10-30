#!/usr/bin/env python3
"""Exact check of a background's palette packing (backtracking), to confirm or refute a pixcheck greedy FAIL.

usage: exact_pack.py CONSOLE image.png [seconds]   MIT license."""
import sys
import time

sys.path.insert(0, __file__.rsplit("/", 2)[0] + "/pixcheck")
import numpy as np  # noqa: E402
import pixcheck  # noqa: E402

console, path = sys.argv[1], sys.argv[2]
limit_s = float(sys.argv[3]) if len(sys.argv) > 3 else 600
spec = pixcheck.CONSOLES[console]["bg"]
rep, con, (backdrop, pals, sets) = pixcheck.check(console, path)
cap, count = spec["colors"], spec["palettes"]
uniq = {frozenset(s) for s in sets.values() if s}
maximal = sorted([s for s in uniq if not any(s < t for t in uniq)], key=len, reverse=True)
print(f"backdrop {rep['info']['backdrop']}, {len(uniq)} distinct tile color sets, {len(maximal)} maximal, "
      f"sizes {[len(s) for s in maximal][:20]}...")
t0 = time.time()
nodes = 0


def solve(i, bins):
    global nodes
    nodes += 1
    if time.time() - t0 > limit_s:
        raise TimeoutError
    if i == len(maximal):
        return bins
    s = maximal[i]
    tried = set()
    for k, b in enumerate(bins):
        u = b | s
        if len(u) <= cap and u not in tried:
            tried.add(u)
            r = solve(i + 1, bins[:k] + [u] + bins[k + 1:])
            if r:
                return r
    if len(bins) < count:
        return solve(i + 1, bins + [s])
    return None


try:
    r = solve(0, [])
    print("exact search:", "packing exists" if r else "no packing exists", f"({nodes} nodes, {time.time() - t0:.1f} s)")
    if r:
        print([len(b) for b in r])
except TimeoutError:
    print(f"exact search: no answer within {limit_s:.0f} s ({nodes} nodes)")
