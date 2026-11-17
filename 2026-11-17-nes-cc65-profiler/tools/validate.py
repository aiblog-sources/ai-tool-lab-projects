#!/usr/bin/env python3
"""validate.py loops.csv loopfns.csv frames.csv funcs.csv [--md out.md]

Checks nesprof against the validation ROM (testrom/): for each of its 64
game loops, the self cycles of burn400 and burn_pair must be exactly
406 * (singles + 2 * pairs) and 38 * pairs, and the frames the profiler
says were missed (sum of NMIs - 1 over the loops) must equal the ROM's own
lag counter. MIT.
"""
import csv
import re
import sys

SINGLES = [0, 10, 20, 40, 60, 70, 72, 80, 5, 0, 90, 30, 1, 2, 64, 100]
PAIRS = [0, 1, 2, 4, 6, 0, 3, 0, 30, 35, 0, 10, 0, 1, 2, 20]
LOOPS = 64


def main():
    loops_p, loopfns_p, frames_p, funcs_p = sys.argv[1:5]
    md = sys.argv[sys.argv.index('--md') + 1] if '--md' in sys.argv else None
    loops = list(csv.DictReader(open(loops_p)))[:LOOPS]
    selfc = {}
    for r in csv.DictReader(open(loopfns_p)):
        selfc[(int(r['loop']), r['function'])] = int(r['self'])
    rows, bad = [], 0
    for k in range(LOOPS):
        s, p = SINGLES[k % 16], PAIRS[k % 16]
        exp_b, exp_p = 406 * (s + 2 * p), 38 * p
        got_b, got_p = selfc.get((k, '_burn400'), 0), selfc.get((k, '_burn_pair'), 0)
        ok = got_b == exp_b and got_p == exp_p
        bad += not ok
        rows.append((k, s, p, exp_b, got_b, exp_p, got_p, int(loops[k]['busy']), int(loops[k]['nmis']), ok))
    missed = sum(r[8] - 1 for r in rows)
    lag = [int(r['lag']) for r in csv.DictReader(open(frames_p)) if int(r['loops_done']) >= LOOPS]
    rom_lag = lag[-1] if lag else None  # last frame: RAM holds $FF before main runs
    funcs = {r['function']: r for r in csv.DictReader(open(funcs_p))}
    tot_b = int(funcs['_burn400']['self'])
    calls_b = int(funcs['_burn400']['calls'])
    pair = funcs['_burn_pair']
    out = []
    out.append('loops checked: %d, exact on both routines: %d, wrong: %d' % (LOOPS, LOOPS - bad, bad))
    out.append('whole run: _burn400 %d calls, %d self cycles (406 x calls = %d); _burn_pair %s calls, %s self (38 x calls = %d), %s inclusive (850 x calls = %d)'
               % (calls_b, tot_b, 406 * calls_b, pair['calls'], pair['self'], 38 * int(pair['calls']),
                  pair['inclusive'], 850 * int(pair['calls'])))
    out.append('frames missed: profiler %d (loops with 2+ NMIs: %d), ROM lag counter %s'
               % (missed, sum(1 for r in rows if r[8] >= 2), rom_lag))
    print('\n'.join(out))
    if md:
        with open(md, 'w') as f:
            f.write('\n\n'.join(out) + '\n\n')
            f.write('| loop | singles | pairs | burn400 expected | burn400 measured | pair expected | pair measured | busy | NMIs |\n')
            f.write('| --- | --- | --- | --- | --- | --- | --- | --- | --- |\n')
            for r in rows:
                f.write('| %d | %d | %d | %d | %d | %d | %d | %d | %d |\n' % r[:9])
    sys.exit(1 if bad or rom_lag != missed else 0)


if __name__ == '__main__':
    main()
