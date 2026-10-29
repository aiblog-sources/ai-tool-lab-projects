"""Turns the cost-table ROM's per-frame profile into cycles per call.

usage: costs.py bench-frames.csv [rom.dis] [--json out.json]

For each phase: mean busy cycles over the phase's frames (the first SKIP frames
of a phase are left out), minus the mean of its baseline phase, divided by the
repetitions per frame. With the disassembly, it also lists what GCC emitted in
each case function (libgcc calls, divs/divu/muls/mulu).
"""
import csv
import json
import re
import statistics
import sys
from collections import defaultdict

PHASE_LEN, SKIP, REPS = 64, 8, 40
NTSC_FRAME = 128_005  # master clocks / 7 = 68000 cycles per NTSC frame here (from frames.csv)

# name, repetitions per frame, baseline phase name, label for the table
CASES = [
    ("nothing", 1, None, "empty frame (SPR_update + SYS_doVBlankProcess)"),
    ("base_s32_1", REPS, "nothing", "loop baseline, one s32 read"),
    ("base_s32_2", REPS, "nothing", "loop baseline, s32 a + b"),
    ("base_u32_2", REPS, "nothing", "loop baseline, u32 a + b"),
    ("base_s16_2", REPS, "nothing", "loop baseline, s16 a + b"),
    ("base_u16_2", REPS, "nothing", "loop baseline, u16 a + b"),
    ("base_f_2", REPS, "nothing", "loop baseline, (s32) float"),
    ("div_s32_var", REPS, "base_s32_2", "s32 / s32 variable"),
    ("div_u32_var", REPS, "base_u32_2", "u32 / u32 variable"),
    ("div_s16_var", REPS, "base_s16_2", "s16 / s16 variable"),
    ("div_u16_var", REPS, "base_u16_2", "u16 / u16 variable"),
    ("mod_s32_var", REPS, "base_s32_2", "s32 % s32 variable"),
    ("mod_u16_var", REPS, "base_u16_2", "u16 % u16 variable"),
    ("div_s32_c10", REPS, "base_s32_1", "s32 / 10"),
    ("div_u16_c10", REPS, "base_s32_1", "u16 / 10"),
    ("div_s32_c8", REPS, "base_s32_1", "s32 / 8"),
    ("div_u16_c8", REPS, "base_s32_1", "u16 / 8"),
    ("sgdk_divu", REPS, "base_u32_2", "SGDK divu(u32, u16)"),
    ("sgdk_divs", REPS, "base_s32_2", "SGDK divs(s32, s16)"),
    ("mul_s32_var", REPS, "base_s32_2", "s32 * s32 variable"),
    ("mul_s16_var", REPS, "base_s16_2", "s16 * s16 variable"),
    ("mul_u16_var", REPS, "base_u16_2", "u16 * u16 variable"),
    ("mul_s32_c10", REPS, "base_s32_1", "s32 * 10"),
    ("sgdk_muls", REPS, "base_s16_2", "SGDK muls(s16, s16)"),
    ("float_mul", REPS, "base_f_2", "float * float (soft float)"),
    ("float_div", REPS, "base_f_2", "float / float (soft float)"),
    ("float_add", REPS, "base_f_2", "float + float (soft float)"),
    ("f16_mul", REPS, "base_s16_2", "SGDK F16_mul (fix16)"),
    ("f16_div", REPS, "base_s16_2", "SGDK F16_div (fix16)"),
    ("f32_mul", REPS, "base_s32_2", "SGDK F32_mul (fix32)"),
    ("tile_xy_40", 1, "nothing", "40 x VDP_setTileMapXY (one row)"),
    ("tile_fill_40", 1, "nothing", "VDP_fillTileMapRect, 40x1"),
    ("tile_datarect_40_cpu", 1, "nothing", "VDP_setTileMapDataRect 40x1, CPU"),
    ("tile_datarect_40_dma", 1, "nothing", "VDP_setTileMapDataRect 40x1, DMA_QUEUE"),
    ("text_20", 1, "nothing", "VDP_drawText, 19 characters"),
    ("sprintf_score", 1, "nothing", "sprintf(\"SCORE %ld\")"),
    ("inttostr_score", 1, "nothing", "intToStr(score, buf, 7)"),
    ("sprintf_and_text", 1, "nothing", "sprintf + VDP_drawText"),
    ("spr_add_release", 1, "nothing", "SPR_addSprite + SPR_releaseSprite"),
    ("load_tiles_64_dma", 1, "nothing", "VDP_loadTileData 64 tiles (2 KB), DMA_QUEUE"),
    ("load_tiles_64_cpu", 1, "nothing", "VDP_loadTileData 64 tiles (2 KB), CPU"),
    ("draw_image_none", 1, "nothing", "VDP_drawImageEx 128x64, uncompressed"),
    ("draw_image_fast", 1, "nothing", "VDP_drawImageEx 128x64, LZ4W (FAST)"),
    ("draw_image_aplib", 1, "nothing", "VDP_drawImageEx 128x64, APLIB"),
    ("div_u16expr_c30", REPS, "base_s32_1", "(u16 - 240) / 30"),
    ("mod_u16_c20", REPS, "base_s32_1", "u16 % 20"),
    ("mul_s16_s32", REPS, "base_s32_2", "s16 * s32 variable"),
    ("mod_u16_fix16c", REPS, "base_s32_1", "u16 % FIX16(2.7)"),
    ("muls_known_positive", REPS, "muls_mod_only", "muls(s16, 256 + u16 % 768)"),
    ("muls_mod_only", REPS, "base_s16_2", "s16 + (256 + u16 % 768), the same without muls"),
    ("nothing", 1, None, "empty frame again (end)"),
]


def emitted(dis_path):
    """function name -> sorted set of interesting things in its body"""
    out, cur = defaultdict(set), None
    for line in open(dis_path, errors="replace"):
        m = re.match(r"^[0-9a-f]+ <([^>]+)>:", line)
        if m:
            cur = m.group(1)
            continue
        if cur is None:
            continue
        # direct calls, and libgcc helpers loaded into an address register first
        m = re.search(r"\t(jsr|bsr\w*|jbsr|lea)\s+[0-9a-f]+ <([^>+]+)>", line)
        if m and (m.group(1) != "lea" or m.group(2).startswith("__")):
            out[cur].add(m.group(2))
        m = re.search(r"\t(divs|divu|muls|mulu)w?\b", line)
        if m:
            out[cur].add(m.group(1))
    return out


def main():
    args = sys.argv[1:]
    json_out = None
    if "--json" in args:
        i = args.index("--json")
        json_out = args[i + 1]
        del args[i:i + 2]
    rows = list(csv.DictReader(open(args[0])))
    dis = emitted(args[1]) if len(args) > 1 else {}

    by_phase = defaultdict(list)
    for r in rows:
        if int(r["frame"]) < 60:
            continue
        by_phase[int(r["phase"])].append(r)

    stats = {}
    for p, (name, reps, base, label) in enumerate(CASES):
        fr = by_phase.get(p, [])[SKIP:PHASE_LEN - 1]
        if not fr:
            continue
        # cost per game loop iteration: a case longer than one frame spans
        # several video frames, so sum busy cycles per game_frames value
        per_iter = defaultdict(int)
        for r in by_phase[p]:
            g = int(r["game_frames"]) - p * PHASE_LEN
            if SKIP <= g < PHASE_LEN - 1:
                per_iter[g] += int(r["busy_cycles"])
        fr = [r for r in by_phase[p] if SKIP <= int(r["game_frames"]) - p * PHASE_LEN < PHASE_LEN - 1]
        busy = list(per_iter.values())
        stats[p] = dict(
            name=name, label=label, reps=reps, base=base, n=len(busy),
            mean=statistics.mean(busy), sd=statistics.pstdev(busy),
            lo=min(busy), hi=max(busy),
            dropped=sum(int(r["dropped"]) for r in fr),
            dma_vb=statistics.mean(int(r["dma_vblank_bytes"]) for r in fr),
            dma_act=statistics.mean(int(r["dma_active_bytes"]) for r in fr),
        )
    first = {}
    for p, s in stats.items():
        first.setdefault(s["name"], s)

    results = []
    print(f"{'case':<46} {'cycles/call':>11} {'sd/rep':>7} {'% frame':>8} {'drop':>4} {'DMA B':>6}  emitted")
    for p, s in sorted(stats.items()):
        if s["base"] is None:
            print(f"{s['label']:<46} {s['mean']:>11.0f} {s['sd']:>7.1f} {100 * s['mean'] / NTSC_FRAME:>7.1f}% "
                  f"{s['dropped']:>4} {s['dma_vb'] + s['dma_act']:>6.0f}")
            continue
        b = first[s["base"]]
        per = (s["mean"] - b["mean"]) / s["reps"]
        sd = s["sd"] / s["reps"]
        what = ",".join(sorted(dis.get(s["name"], set()) - {"sink"}))
        print(f"{s['label']:<46} {per:>11.1f} {sd:>7.1f} {100 * per / NTSC_FRAME:>7.2f}% "
              f"{s['dropped']:>4} {s['dma_vb'] + s['dma_act']:>6.0f}  {what}")
        results.append(dict(case=s["name"], label=s["label"], cycles=round(per, 1), sd=round(sd, 2),
                            frames=s["n"], dropped=s["dropped"], dma_bytes=round(s["dma_vb"] + s["dma_act"]),
                            emitted=what))
    if json_out:
        json.dump(results, open(json_out, "w"), indent=1)


if __name__ == "__main__":
    main()
