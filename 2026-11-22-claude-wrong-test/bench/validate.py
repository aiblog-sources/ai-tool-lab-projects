#!/usr/bin/env python3
"""Check the variants before any model run: each one fails exactly one visible test (the target), and the hidden spec
tests fail on `bug` variants only. Usage: python3 bench/validate.py"""
import importlib.util, json, re, shutil, subprocess, sys, tempfile
from pathlib import Path

root = Path(__file__).resolve().parent.parent
variants = json.loads((root / "variants/variants.json").read_text())
ok = True
for fn, v in variants.items():
    for kind in ("bug", "badtest"):
        with tempfile.TemporaryDirectory() as t:
            work = Path(t) / "w"
            shutil.copytree(root / "fixture", work, ignore=shutil.ignore_patterns("__pycache__"))
            path, old, new = v[kind]
            f = work / path
            assert f.read_text().count(old) == 1
            f.write_text(f.read_text().replace(old, new))
            p = subprocess.run([sys.executable, "-m", "unittest", "discover", "-s", "tests", "-t", "."], cwd=work,
                               capture_output=True, text=True)
            fails = re.findall(r"^(?:FAIL|ERROR): (\w+)", p.stderr, re.M)
            spec = json.loads(subprocess.run([sys.executable, "-I", str(root / "hidden/spec_tests.py"), str(work), v["module"]],
                                             capture_output=True, text=True).stdout)
            good = len(fails) == 1 and (spec["failed"] > 0) == (kind == "bug")
            ok &= good
            print(f"{fn:9} {kind:7} visible fails={fails} hidden={spec['passed']}/{spec['passed']+spec['failed']} {'ok' if good else 'WRONG'}")
print("ALL OK" if ok else "PROBLEMS")
