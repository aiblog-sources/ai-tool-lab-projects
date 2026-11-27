#!/usr/bin/env python3
"""Runs inside the container next to solution.py: calls the model's function on every relevant input and prints
JSON: whether the words only use the CRAM color bits (0x0EEE), each channel's level per input value, and which
known rule that matches (trunc = top 3 bits, round = round(v * 7 / 255), never a tie, table = nearest of the non-linear
levels 0,52,87,116,144,172,206,255, round5 = nearest on the 5-bit scale c*7/31). MIT license."""
import json, sys

task = sys.argv[1]
ns = {}
try:
    exec(open("solution.py").read(), ns)
    fn = ns["snes_to_md" if task == "snes" else "rgb_to_md"]
except Exception as e:  # noqa: BLE001
    print(json.dumps({"error": f"load: {type(e).__name__}: {e}"[:300]}))
    sys.exit()

TABLE = [0, 52, 87, 116, 144, 172, 206, 255]


def expand(c5):
    return (c5 << 3) | (c5 >> 2)


def call(r, g, b):
    """r, g, b are 5-bit for the snes task and 8-bit for the rgb task."""
    if task == "snes":
        return fn(r | (g << 5) | (b << 10))
    return fn(r, g, b)


n = 32 if task == "snes" else 256
out = {"bad_bits": 0, "errors": 0}
maps = {}
for ch in range(3):
    m = []
    for v in range(n):
        rgb = [0, 0, 0]
        rgb[ch] = v
        try:
            w = int(call(*rgb))
        except Exception as e:  # noqa: BLE001
            out["errors"] += 1
            out.setdefault("first_error", f"{type(e).__name__}: {e}"[:200])
            m.append(None)
            continue
        if w & ~0x0EEE:
            out["bad_bits"] += 1
        levels = [(w >> 1) & 7, (w >> 5) & 7, (w >> 9) & 7]
        others = [levels[i] for i in range(3) if i != ch]
        m.append(levels[ch] if not any(others) else ("cross", levels))
    maps["rgb"[ch]] = m


def rule(v):
    v8 = expand(v) if task == "snes" else v
    return {"trunc": v8 >> 5, "round": round(v8 * 7 / 255),
            "table": min(range(8), key=lambda k: (abs(TABLE[k] - v8), k)),
            "round5": round(v * 7 / 31) if task == "snes" else None, "trunc5": v >> 2 if task == "snes" else None}


rules = {}
for ch, m in maps.items():
    match = []
    for name in ["trunc", "round", "table", "round5", "trunc5"]:
        exp = [rule(v)[name] for v in range(n)]
        if exp[0] is not None and m == exp:
            match.append(name)
    rules[ch] = match or ["other"]
out["rules"] = rules
out["same_rule_all_channels"] = len({tuple(v) for v in rules.values()}) == 1
out["maps"] = {ch: [x if isinstance(x, int) else str(x) for x in m] for ch, m in maps.items()}
print(json.dumps(out))
