#!/usr/bin/env python3
"""Convert every corpus image (snapped to SNES colors) with each method and ask pixcheck whether the result fits
the Mega Drive (4 palettes of 15 + backdrop, 8x8 tiles, tile budget). Writes results/fit-md.json. MIT license."""
import json, subprocess, sys, tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent
PIXCHECK = HERE.parent / "2026-10-30-pixel-art-limits-checker/pixcheck/pixcheck.py"


def main():
    rows = []
    with tempfile.TemporaryDirectory() as tmp:
        for img in sorted((HERE / "corpus").glob("*.png")):
            for method in ["none", "trunc", "round", "lab", "distinct"]:
                src = img
                if method != "none":
                    src = Path(tmp) / f"{img.stem}-{method}.png"
                    subprocess.run([sys.executable, HERE / "mdcolor.py", "convert", img, src, "--method", method,
                                    "--snap-snes"], check=True, capture_output=True)
                args = [sys.executable, PIXCHECK, "md", src, "--json", Path(tmp) / "r.json"]
                if "knight" in img.stem:
                    args += ["--sprite", "32x32"]
                p = subprocess.run(args, capture_output=True, text=True)
                r = json.load(open(Path(tmp) / "r.json"))
                rows.append(dict(image=img.stem, method=method, ok=r["ok"], errors=r["errors"],
                                 palettes_used=r["info"].get("palettes_used"), console_colors=r["info"].get("console_colors")))
                print(img.stem, method, "PASS" if r["ok"] else "FAIL", r["info"].get("palettes_used"),
                      "; ".join(e[:80] for e in r["errors"]))
    json.dump(rows, open(HERE / "results/fit-md.json", "w"), indent=1)


if __name__ == "__main__":
    main()
