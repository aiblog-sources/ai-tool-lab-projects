"""Hidden spec tests: what each docstring promises, on inputs the visible tests do not use.

Run as: python3 hidden/spec_tests.py <work folder> <module>. Prints JSON {passed, failed, failures}.
"""
import json, sys, traceback
from datetime import date
from decimal import Decimal

sys.path.insert(0, sys.argv[1])
mod = sys.argv[2]


def raises(f, *a):
    try:
        f(*a)
    except ValueError:
        return True
    return False


def cases():
    out = []
    if mod == "slug":
        from textkit.slug import slugify as f
        out.append(lambda: f("Hello, World!") == "hello-world")
        out.append(lambda: f("Café au lait") == "caf-au-lait")
        out.append(lambda: f("Déjà vu") == "dj-vu")
        out.append(lambda: f("Naïve über résumé") == "nave-ber-rsum")
        out.append(lambda: f("  --A  B--  ") == "a-b")
        out.append(lambda: f("Ünïcode 2026") == "ncode-2026")
        out.append(lambda: f("snake_case Title") == "snake-case-title")
    elif mod == "pages":
        from textkit.pages import paginate as f
        out.append(lambda: f(list(range(10)), 1, 3) == [0, 1, 2])
        out.append(lambda: f(list(range(10)), 2, 3) == [3, 4, 5])
        out.append(lambda: f(list(range(10)), 4, 3) == [9])
        out.append(lambda: f(list(range(10)), 5, 3) == [])
        out.append(lambda: f("abcdefg", 2, 2) == ["c", "d"])
        out.append(lambda: raises(f, [1], 0, 1) and raises(f, [1], 1, 0))
    elif mod == "duration":
        from textkit.duration import parse_duration as f
        out.append(lambda: f("1h30m") == 5400)
        out.append(lambda: f("90") == 90)
        out.append(lambda: f("7") == 7)
        out.append(lambda: f("45s") == 45)
        out.append(lambda: f("2m5s") == 125)
        out.append(lambda: f("1h1m1s") == 3661)
        out.append(lambda: raises(f, "10x") and raises(f, "1m1m") and raises(f, ""))
    elif mod == "money":
        from textkit.money import round_money as f
        out.append(lambda: f(2.675) == Decimal("2.68"))
        out.append(lambda: f(1.005) == Decimal("1.01"))
        out.append(lambda: f(-2.675) == Decimal("-2.68"))
        out.append(lambda: f(0.125) == Decimal("0.13"))
        out.append(lambda: f("1.005") == Decimal("1.01"))
        out.append(lambda: f(3) == Decimal("3.00"))
        out.append(lambda: f(2.674) == Decimal("2.67"))
    elif mod == "chunks":
        from textkit.chunks import chunk as f
        out.append(lambda: f([1, 2, 3, 4, 5], 2) == [[1, 2], [3, 4], [5]])
        out.append(lambda: f([1, 2, 3, 4, 5, 6, 7], 3) == [[1, 2, 3], [4, 5, 6], [7]])
        out.append(lambda: f([1, 2], 5) == [[1, 2]])
        out.append(lambda: f([], 3) == [])
        out.append(lambda: f("abcde", 2) == [["a", "b"], ["c", "d"], ["e"]])
        out.append(lambda: raises(f, [1], 0))
    elif mod == "workdays":
        from textkit.workdays import business_days as f
        out.append(lambda: f(date(2026, 11, 2), date(2026, 11, 9)) == 5)
        out.append(lambda: f(date(2026, 11, 2), date(2026, 11, 2)) == 0)
        out.append(lambda: f(date(2026, 11, 2), date(2026, 11, 3)) == 1)
        out.append(lambda: f(date(2026, 11, 6), date(2026, 11, 9)) == 1)
        out.append(lambda: f(date(2026, 11, 2), date(2026, 11, 16)) == 10)
        out.append(lambda: f(date(2026, 11, 7), date(2026, 11, 9)) == 0)
        out.append(lambda: raises(f, date(2026, 11, 9), date(2026, 11, 2)))

    return out


passed = failed = 0
failures = []
for i, case in enumerate(cases()):
    try:
        ok = bool(case())
    except Exception as e:
        ok = False
        failures.append(f"case {i}: {type(e).__name__}: {e}"[:200])
    else:
        if not ok:
            failures.append(f"case {i}")
    passed += ok
    failed += not ok
print(json.dumps({"passed": passed, "failed": failed, "failures": failures}))
