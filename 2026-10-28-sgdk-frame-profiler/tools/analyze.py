"""Summarise mdprof's frames.csv for the test ROM: per phase, what the profiler
saw and what the ROM's own counters (SGDK load, vtimer, game frame counter) say."""
import csv, json, pathlib, sys
from collections import defaultdict

path = pathlib.Path(sys.argv[1])
rows = [{k: float(v) for k, v in r.items()} for r in csv.DictReader(path.open())]
summary = json.loads((path.parent / "summary.json").read_text())
FRAME = rows[0]["frame_cycles"]
# boot: skip until the game loop has run once
start = next(i for i, r in enumerate(rows) if r["game_frames"] >= 1)
run = rows[start:]
print(f"boot frames skipped: {start}; analysed video frames: {len(run)}; 68k cycles per frame: {FRAME:.0f}")

names = {0: "A light", 1: "B divisions", 2: "B2 reciprocals", 3: "C per-tile redraw", 4: "C2 one fill call", 5: "D DMA ramp", 6: "E idle"}
by_phase = defaultdict(list)
for r in run:
    by_phase[int(r["phase"])].append(r)
print("\nphase                 frames  busy% mean  busy% max  dropped(prof) dropped(ROM)  SGDK load mean")
for ph, rs in sorted(by_phase.items()):
    busy = [100 * r["busy_cycles"] / FRAME for r in rs]
    missed = sum(int(r["dropped"]) for r in rs)
    # ROM view: video frames that passed minus game loops that finished
    rom_missed = (rs[-1]["vtimer_lo"] - rs[0]["vtimer_lo"]) - (rs[-1]["game_frames"] - rs[0]["game_frames"])
    print(f"{names[ph]:20s} {len(rs):7d} {sum(busy)/len(busy):11.1f} {max(busy):10.1f} {missed:13d} {rom_missed:12.0f} {sum(r['sgdk_load'] for r in rs)/len(rs):15.1f}")

print("\nphase B, by divisions per sprite (24 sprites):")
print("div/sprite  frames  busy% mean  missed  SGDK load mean")
lv = defaultdict(list)
for r in by_phase[1]:
    lv[int(r["div"])].append(r)
for d, rs in sorted(lv.items()):
    busy = [100 * r["busy_cycles"] / FRAME for r in rs]
    print(f"{d:10d} {len(rs):7d} {sum(busy)/len(busy):11.1f} {sum(int(r['dropped']) for r in rs):7d} {sum(r['sgdk_load'] for r in rs)/len(rs):15.1f}")

print("\nphases C and C2, frames over 20% busy:")
for r in by_phase[3] + by_phase[4]:
    b = 100 * r["busy_cycles"] / FRAME
    if b > 20:
        print(f"  {names[int(r['phase'])]:18s} video frame {int(r['frame'])}: busy {b:.1f}%  dropped {int(r['dropped'])}  game frame {int(r['game_frames'])}")

print("\nphase D, DMA by queued size (bytes per frame):")
print("queued  frames  DMA in VBlank (mean)  DMA in active display (mean)  frames spilling  last DMA line (max)")
# the queue filled in game frame N is sent at the next VBlank, i.e. in the
# next video frame's row: pair each row with the size queued in the row before
dq = defaultdict(list)
for prev, r in zip(run, run[1:]):
    if int(prev["phase"]) == 5 and int(r["phase"]) == 5:
        dq[int(prev["dma_q"])].append(r)
for q, rs in sorted(dq.items()):
    vb = sum(r["dma_vblank_bytes"] for r in rs) / len(rs)
    ac = sum(r["dma_active_bytes"] for r in rs) / len(rs)
    sp = sum(1 for r in rs if r["dma_active_bytes"] > 0)
    ll = max(int(r["last_dma_line"]) for r in rs)
    print(f"{q:6d} {len(rs):7d} {vb:21.0f} {ac:29.0f} {sp:16d} {ll:20d}")

prof = {p["fn"]: p["cycles"] for p in summary["profile"]}
# the profile covers frames from summary["from"]; count the game loops that ran there
loops = rows[-1]["game_frames"] - rows[summary["from"] - 1]["game_frames"]
burn = next((v for k, v in prof.items() if k.startswith("burn")), None)
if burn:
    print(f"\ncalibration: burn(500) total {burn:.0f} cycles over {loops:.0f} game loops = {burn/loops:.1f} per call "
          f"(dbra loop alone: 10*500+14 = 5014)")

# per game loop: busy cycles from one loop start to the next, as % of a frame
loops, curl = [], None
for r in run:
    if r["loop_start"]:
        if curl: loops.append(curl)
        curl = {"phase": int(r["phase"]), "div": int(r["div"]), "busy": 0.0, "frames": 0}
    if curl:
        curl["busy"] += r["busy_cycles"]; curl["frames"] += 1
print("\nphase B and B2, cost of one game loop (busy cycles / one frame) vs SGDK's own load figure:")
print("phase           div/sprite  loops  loop cost %  video frames per loop  SGDK load")
grp = defaultdict(list)
for l in loops:
    if l["phase"] in (1, 2): grp[(l["phase"], l["div"])].append(l)
sg = defaultdict(list)
for r in run:
    if int(r["phase"]) in (1, 2): sg[(int(r["phase"]), int(r["div"]))].append(r["sgdk_load"])
for key, ls in sorted(grp.items()):
    cost = sum(100 * l["busy"] / FRAME for l in ls) / len(ls)
    fpl = sum(l["frames"] for l in ls) / len(ls)
    print(f"{names[key[0]]:15s} {key[1]:10d} {len(ls):6d} {cost:12.1f} {fpl:22.2f} {sum(sg[key])/len(sg[key]):10.1f}")

def cyc(prefix):
    return sum(v for k, v in prof.items() if k == prefix or k.startswith(prefix + "."))
# cost per division from the slope of loop cost over divisions per sprite
pts = [(k[1], sum(l["busy"] for l in ls) / len(ls)) for k, ls in sorted(grp.items()) if k[0] == 1]
n = len(pts); mx = sum(p[0] for p in pts) / n; my = sum(p[1] for p in pts) / n
slope = sum((x - mx) * (y - my) for x, y in pts) / sum((x - mx) ** 2 for x, _ in pts)
print(f"\nphase B slope: each extra division per sprite adds {slope:.0f} cycles per loop = {slope/24:.0f} per division (24 sprites)")
base = sum(l["busy"] for l in loops if l["phase"] == 0) / max(1, sum(1 for l in loops if l["phase"] == 0))
b2 = sum(l["busy"] for l in grp[(2, 8)]) / len(grp[(2, 8)])
print(f"phase A loop: {base:.0f} cycles; phase B2 loop: {b2:.0f} cycles; reciprocal step = ({b2:.0f} - {base:.0f}) / 192 = {(b2-base)/192:.0f} cycles")
ndiv = 24 * 30 * sum(range(1, 9))  # 25920          # phase B: 30 loops at each level 1..8
nrec = 24 * 8 * 120                         # phase B2: 120 loops at 8 per sprite
div_total = cyc("__udivsi3") + cyc("__divsi3") + cyc("physics_with_divisions")
print(f"\nphase B: {ndiv} divisions, __divsi3 + __udivsi3 + loop = {div_total:.0f} cycles = {div_total/ndiv:.0f} per division")
print(f"phase B2: {nrec} reciprocal steps, physics_with_reciprocals = {cyc('physics_with_reciprocals'):.0f} cycles = {cyc('physics_with_reciprocals')/nrec:.0f} per step")
print(f"redraw_plane_per_tile: {cyc('redraw_plane_per_tile'):.0f} cycles over 6 calls = {cyc('redraw_plane_per_tile')/6:.0f} per call")
# the fill version compiles to a jump into VDP_fillTileMapRect, so measure it as
# the extra busy time of the redraw frames over the other frames of phase C2
c2 = by_phase[4]
hi = [r["busy_cycles"] for r in c2 if r["busy_cycles"] > 0.3 * FRAME]
lo = [r["busy_cycles"] for r in c2 if r["busy_cycles"] <= 0.3 * FRAME]
print(f"one VDP_fillTileMapRect redraw: {len(hi)} frames at {sum(hi)/len(hi):.0f} busy cycles vs {sum(lo)/len(lo):.0f} on the others = {sum(hi)/len(hi)-sum(lo)/len(lo):.0f} cycles")
print("\nfull profile (top 40, % of all cycles):")
for p in summary["profile"]:
    print(f"  {p['pct']:6.2f}%  {p['cycles']:>11}  {p['fn']}")
