"""Does a fixed scene still show the same pictures? Run the original and a fix with the fighters standing still
(no d-pad, so positions do not depend on the loop rate) and only punch presses, save every video frame with
mdsprites (2026-11-05 project, --dump), and compare the sets of distinct frames from frame 400 on.
usage: same_scene.py <original project dir> <fixed project dir> [...more fixed dirs]"""
import hashlib
import os
import shutil
import subprocess
import sys
from pathlib import Path

W = Path(os.environ.get("MDSPRITES_WORK", Path.home() / ".cache/mdsprites"))
UID = f"{os.getuid()}:{os.getgid()}"
FRAMES, FROM, SIZE = 1500, 400, 320 * 224 * 2
PUNCHES = [f"{f}-{f + 4}:A" for f in range(300, 1500, 90)]


def frames(proj):
    tag = Path(proj).name
    d = W / "same" / tag
    if d.exists():
        shutil.rmtree(d)
    d.mkdir(parents=True)
    for f in ("rom.bin", "symbol.txt"):
        shutil.copy(Path(proj) / "out" / f, d / f)
    args = ["docker", "run", "--rm", "--network", "none", "--user", UID, "-v", f"{W}:/w", "-w", "/w", "gcc:14",
            "./mdsprites", "gpgx/genesis_plus_gx_libretro.so", f"same/{tag}/rom.bin", f"same/{tag}/symbol.txt",
            str(FRAMES), "--dump", f"same/{tag}/dump.raw", "--input", "70-74:S"]
    for p in PUNCHES:
        args += ["--input", p]
    subprocess.run(args, check=True, capture_output=True)
    data = (d / "dump.raw").read_bytes()
    # early boot frames can have another size (H32 before the game sets H40), so read the 320x224 frames from the end
    n = FRAMES - FROM
    assert len(data) >= n * SIZE, len(data)
    data = data[len(data) - n * SIZE:]
    # P1 stands on the left half, P2 on the right half: hash each half, so the pose pairs (which depend on timing)
    # do not matter, only the poses each fighter shows
    row = 320 * 2
    out = []
    for i in range(n):
        f = data[i * SIZE:(i + 1) * SIZE]
        left = b"".join(f[y * row:y * row + row // 2] for y in range(224))
        right = b"".join(f[y * row + row // 2:(y + 1) * row] for y in range(224))
        out.append((hashlib.sha1(left).hexdigest(), hashlib.sha1(right).hexdigest()))
    shutil.rmtree(d)
    return out


orig = frames(sys.argv[1])
for p in [None] + sys.argv[2:]:
    hs = orig if p is None else frames(p)
    parts = []
    for side, name in ((0, "left (P1)"), (1, "right (P2)")):
        so, s = {h[side] for h in orig}, {h[side] for h in hs}
        parts.append(f"{name}: {len(s)} distinct, {len(s & so)} shared with the original, {len(s - so)} only here, "
                     f"{len(so - s)} missing")
    print(f"{Path(p or sys.argv[1]).name}: " + "; ".join(parts))
