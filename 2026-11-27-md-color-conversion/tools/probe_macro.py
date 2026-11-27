#!/usr/bin/env python3
"""Compile SGDK's RGB24_TO_VDPCOLOR macro for all 256 grey values into a ROM and read the words back, to compare
the runtime macro with rescomp's conversion. Output: results/macro-probe.csv. MIT license."""
import csv, os, shutil, subprocess, sys
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent
WORK = Path(os.environ.get("WORK", Path.home() / ".cache/mdcolor")) / "macro"
SGDK = os.environ.get("SGDK_IMAGE", "ghcr.io/stephane-d/sgdk:latest")


def main():
    shutil.rmtree(WORK, ignore_errors=True)
    (WORK / "src").mkdir(parents=True)
    vals = ", ".join(f"RGB24_TO_VDPCOLOR(0x{v:02X}{v:02X}{v:02X})" for v in range(256))
    (WORK / "src/main.c").write_text(
        "#include <genesis.h>\n"
        f"__attribute__((used)) const u16 macro_out[256] = {{ {vals} }};\n"
        "volatile u16 sink;\n"
        "int main(bool hard) { sink = macro_out[sink & 255]; while (TRUE) SYS_doVBlankProcess(); return 0; }\n")
    p = subprocess.run(["docker", "run", "--rm", "--network", "none", "--user", f"{os.getuid()}:{os.getgid()}",
                        "-v", f"{WORK}:/src", SGDK], capture_output=True, text=True)
    if p.returncode:
        sys.exit(p.stdout[-2000:])
    rom = (WORK / "out/rom.bin").read_bytes()
    sym = {l.split()[2]: int(l.split()[0], 16) for l in (WORK / "out/symbol.txt").read_text().splitlines()
           if len(l.split()) == 3}
    a = sym["macro_out"]
    rows = []
    for v in range(256):
        w = int.from_bytes(rom[a + 2 * v:a + 2 * v + 2], "big")
        rows.append(dict(v=v, word=f"{w:04X}", r_level=(w >> 1) & 7, g_level=(w >> 5) & 7, b_level=(w >> 9) & 7,
                         rescomp_level=v >> 5))
    with open(HERE / "results/macro-probe.csv", "w", newline="") as f:
        wr = csv.DictWriter(f, fieldnames=list(rows[0]))
        wr.writeheader(); wr.writerows(rows)
    same_rgb = all(r["r_level"] == r["g_level"] == r["b_level"] for r in rows)
    differ = sum(r["r_level"] != r["rescomp_level"] for r in rows)
    match = sum(r["r_level"] == min(v + 16, 255) >> 5 for v, r in enumerate(rows))
    print(f"channels agree: {same_rgb}; differ from rescomp on {differ} of 256; equal to min(v+16,255)>>5 on {match}")


if __name__ == "__main__":
    main()
