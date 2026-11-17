#!/usr/bin/env python3
"""symbols.py rom.dbg out.sym

Turns an ld65 debug file (cl65 -g -Wl --dbgfile,rom.dbg) into the address
ranges nesprof charges cycles to, one "start end name" line per range (hex,
end exclusive), covering every byte of the ROM's code segments.

- C functions: the scope cc65 writes for each function (exact start and size).
- Everything else (crt0, the cc65 runtime such as pushax or tosmulax, your
  own .s files): the nearest exported or file-level label at or before the
  address. cc65's internal labels (L0012, L1) and cheap locals (@loop) are
  skipped, so a runtime routine with several entry points (pusha, pusha0,
  pushax) shows up under the entry that starts each piece of code.
MIT.
"""
import re
import sys

LINE = re.compile(r'^(\w+)\t(.*)$')


def fields(s):
    out = {}
    for m in re.finditer(r'(\w+)=("([^"]*)"|[^,]*)', s):
        out[m.group(1)] = m.group(3) if m.group(3) is not None else m.group(2)
    return out


def main(dbg, out):
    segs, syms, scopes = {}, {}, []
    for line in open(dbg):
        m = LINE.match(line.rstrip('\n'))
        if not m:
            continue
        kind, f = m.group(1), fields(m.group(2))
        if kind == 'seg':
            segs[f['id']] = f
        elif kind == 'sym':
            syms[f['id']] = f
        elif kind == 'scope':
            scopes.append(f)
    code = {i for i, s in segs.items()
            if s.get('type') == 'ro' and 'oname' in s and int(s['start'], 16) >= 0x8000
            and s['name'] not in ('RODATA', 'VECTORS', 'CHARS')}
    owner = {}
    # labels first, C functions override them
    labels = []
    for s in syms.values():
        if s.get('type') != 'lab' or s.get('seg') not in code or 'parent' in s:
            continue
        name = s['name']
        if re.fullmatch(r'L[0-9A-F]{1,4}', name) or name.startswith('@'):
            continue
        labels.append((int(s['val'], 16), name))
    labels.sort()
    for sid in code:
        seg = segs[sid]
        a0 = int(seg['start'], 16)
        a1 = a0 + int(seg['size'], 16)
        cur = seg['name'].lower()
        li = 0
        for a in range(a0, a1):
            while li < len(labels) and labels[li][0] <= a:
                if labels[li][0] >= a0:
                    cur = labels[li][1]
                li += 1
            owner[a] = cur
    for sc in scopes:
        if sc.get('type') != 'scope' or 'sym' not in sc:
            continue
        sym = syms[sc['sym']]
        if sym.get('seg') not in code:
            continue
        a0 = int(sym['val'], 16)
        for a in range(a0, a0 + int(sc['size'])):
            owner[a] = sc['name']
    with open(out, 'w') as f:
        run = None
        for a in sorted(owner):
            if run and run[1] == a and run[2] == owner[a]:
                run[1] = a + 1
            else:
                if run:
                    f.write('%04x %04x %s\n' % tuple(run))
                run = [a, a + 1, owner[a]]
        if run:
            f.write('%04x %04x %s\n' % tuple(run))


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
