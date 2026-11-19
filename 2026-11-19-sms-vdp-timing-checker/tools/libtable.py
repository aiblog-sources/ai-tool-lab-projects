"""Per-test table for the SMSlib test ROM: joins smsvdp's per-frame CSV (test id
column) with its --events CSV (every active-display data write) and prints one
markdown row per test: writes per call, smallest gap, writes under 23/26/29."""
import csv, sys
from collections import defaultdict

NAMES = ["SMS_loadTiles (4 tiles)", "SMS_VRAMmemcpy (128 B)", "SMS_VRAMmemcpy_brief (128 B)",
         "SMS_VRAMmemset (128 B)", "SMS_VRAMmemsetW (128 B)", "SMS_loadTileMap (32 entries)",
         "SMS_loadTileMapArea (8x4)", "SMS_loadTileMapColumn (24)", "SMS_setTileatXY x32",
         "SMS_setNextTileatXY + SMS_setTile x32", "SMS_loadBGPalette", "SMS_loadSpritePalette",
         "SMS_setBGPaletteColor x16", "SMS_zeroBGPalette", "SMS_loadBGPaletteHalfBrightness",
         "SMS_load1bppTiles (4 tiles)", "SMS_load2bppTiles (4 tiles)", "SMS_print (32 chars)",
         "SMS_copySpritestoSAT (16 sprites)", "UNSAFE_SMS_copySpritestoSAT (16 sprites)",
         "UNSAFE_SMS_VRAMmemcpy128", "UNSAFE_SMS_VRAMmemcpy (128 B)", "SMS_readVRAM (64 B, reads)",
         "SMS_setAddr + SMS_setTile x32"]

frames_csv, events_csv = sys.argv[1], sys.argv[2]
from_frame = int(sys.argv[3]) if len(sys.argv) > 3 else 60  # smsvdp --from
test_of, reads = {}, defaultdict(lambda: [0, 0])
for r in csv.DictReader(open(frames_csv)):
    if int(r["frame"]) < from_frame:
        continue
    test_of[int(r["frame"])] = int(r["test"])
    if int(r["reads_active"]):
        rr = reads[int(r["test"])]; rr[0] += int(r["reads_active"]); rr[1] += int(r["reads_fast"])
calls = defaultdict(set)
st = defaultdict(lambda: {"n": 0, "min": 10**9, "lt23": 0, "lt26": 0, "lt29": 0, "lines": set()})
for e in csv.DictReader(open(events_csv)):
    f = int(e["frame"])
    if f not in test_of:
        continue
    t, g = test_of[f], int(e["gap"])
    calls[t].add(f)
    s = st[t]
    s["n"] += 1; s["lines"].add(int(e["line"]))
    # the first write of a call follows the previous call by a whole frame
    if g < 1000:
        s["min"] = min(s["min"], g)
    s["lt23"] += g < 23; s["lt26"] += g < 26; s["lt29"] += g < 29
print("| SMSlib call | Writes per call | Smallest gap (cycles) | Under 23 | Under 26 | Under 29 | Lines |")
print("| --- | --- | --- | --- | --- | --- | --- |")
for t, name in enumerate(NAMES):
    if t in reads and t not in st:
        n = len([f for f, x in test_of.items() if x == t])
        print(f"| {name} | {reads[t][0] // max(n, 1)} reads | | | {reads[t][1] // max(n, 1)} reads | | |")
        continue
    s, n = st[t], len(calls[t]) or 1
    lines = sorted(s["lines"])
    span = f"{lines[0]}-{lines[-1]}" if lines else ""
    print(f"| {name} | {s['n'] // n} | {s['min'] if s['min'] < 10**9 else ''} | {s['lt23'] // n} | {s['lt26'] // n} | {s['lt29'] // n} | {span} |")
