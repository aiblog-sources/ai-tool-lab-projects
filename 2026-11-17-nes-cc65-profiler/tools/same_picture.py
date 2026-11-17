#!/usr/bin/env python3
"""same_picture.py a-oam.csv b-oam.csv [--from F]

Do two builds of a scene show the same sprites, game loop by game loop?
Each frame's OAM (nesprof --oam) becomes the sorted list of its visible
entries (Y < $EF), so order and flicker rotation do not matter; repeated
frames are dropped, which turns a 30 fps run and a 60 fps run into the same
sequence of pictures if the program logic did not change. The two sequences
are compared over their common length, from frame 0 so that both start
at game loop 0 (a later start compares different loops of a 30 fps and a
60 fps run). MIT.
"""
import sys


def tables(path, start):
    seq = []
    for line in open(path):
        f, hexs = line.strip().split(',')
        if int(f) < start:
            continue
        b = bytes.fromhex(hexs)
        t = tuple(sorted(tuple(b[i:i + 4]) for i in range(0, 256, 4) if b[i] < 0xEF))
        if not seq or seq[-1] != t:
            seq.append(t)
    return seq


def compare(a_path, b_path, start=0):
    a, b = tables(a_path, start), tables(b_path, start)
    n = min(len(a), len(b))
    first_diff = next((i for i in range(n) if a[i] != b[i]), None)
    return {"tables_a": len(a), "tables_b": len(b), "compared": n,
            "same": first_diff is None and n > 0, "first_diff": first_diff}


if __name__ == '__main__':
    start = int(sys.argv[sys.argv.index('--from') + 1]) if '--from' in sys.argv else 0
    print(compare(sys.argv[1], sys.argv[2], start))
