"""Build one main.c with SGDK 2.11 in the SGDK Docker image (no network). Returns (ok, errors, project dir)."""
import os
import re
import shutil
import subprocess
from pathlib import Path

WORK = Path(os.environ.get("WORK", Path.home() / ".cache/mdperf-loops"))
SDK = Path(os.environ.get("SGDK_DIR", Path.home() / ".cache/mddma/sgdk-v2.11"))
IMG = os.environ.get("SGDK_IMAGE", "ghcr.io/stephane-d/sgdk:latest")
UID = f"{os.getuid()}:{os.getgid()}"


def build(code, tag):
    d = WORK / tag
    if d.exists():
        shutil.rmtree(d)
    (d / "src").mkdir(parents=True)
    (d / "src" / "main.c").write_text(code)
    mounts = []
    for x in ("inc", "lib", "res", "src"):
        mounts += ["-v", f"{SDK / x}:/sgdk/{x}:ro"]
    mounts += ["-v", f"{SDK / 'bin/rescomp.jar'}:/sgdk/bin/rescomp.jar:ro"]
    p = subprocess.run(["docker", "run", "--rm", "--network", "none", "--user", UID] + mounts + ["-v", f"{d}:/src", IMG],
                       capture_output=True, text=True, timeout=300)
    out = p.stdout + p.stderr
    ok = (d / "out/rom.bin").exists() and (d / "out/symbol.txt").exists()
    errs = [l for l in out.splitlines() if re.search(r"error|undefined reference", l, re.I)]
    return ok, "\n".join(errs[:40]) if errs else out[-3000:], d
