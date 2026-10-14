#!/usr/bin/env python3
"""Scores results/raw.jsonl and writes results/summary.md. Standard library only.

Three checks per answer:
  parses   json.loads() on the whole reply (strict). "parses after cleanup" also accepts a reply wrapped in a
           Markdown code fence or with text around one {...} object.
  valid    the parsed object passes schema.json (validate() below covers every keyword schema.json uses).
  correct  customer_name, order_id, category, refund_requested and items all match the hand-written labels
           in inputs.jsonl. Items match when the count is equal and every labelled product (by keyword) has
           the right quantity. priority is not graded (no single right answer).
"""
import json
import os
import re
import statistics
from collections import defaultdict

HERE = os.path.dirname(os.path.abspath(__file__))
FIELDS = ["customer_name", "order_id", "category", "refund_requested", "items"]
MODES = ["prompt", "grammar", "grammar-only"]


def validate(v, s, path="$"):
    """Returns a list of errors. Supports type, enum, anyOf, pattern, required, properties,
    additionalProperties false, items, minimum: the keywords schema.json uses."""
    errs = []
    if "anyOf" in s:
        if all(validate(v, sub, path) for sub in s["anyOf"]):
            errs.append(f"{path}: matches no anyOf branch")
        return errs
    if "enum" in s and v not in s["enum"]:
        errs.append(f"{path}: {v!r} not in enum")
    t = s.get("type")
    types = {"object": dict, "array": list, "string": str, "boolean": bool, "null": type(None)}
    if t == "integer":
        if not (isinstance(v, int) and not isinstance(v, bool)):
            return errs + [f"{path}: not an integer"]
    elif t and not isinstance(v, types[t]):
        return errs + [f"{path}: not {t}"]
    if t == "string" and "pattern" in s and not re.search(s["pattern"], v):
        errs.append(f"{path}: {v!r} does not match pattern")
    if "minimum" in s and v < s["minimum"]:
        errs.append(f"{path}: below minimum")
    if t == "object":
        for k in s.get("required", []):
            if k not in v:
                errs.append(f"{path}: missing {k}")
        props = s.get("properties", {})
        for k, val in v.items():
            if k in props:
                errs += validate(val, props[k], f"{path}.{k}")
            elif s.get("additionalProperties") is False:
                errs.append(f"{path}: extra key {k}")
    if t == "array":
        for i, item in enumerate(v):
            errs += validate(item, s["items"], f"{path}[{i}]")
    return errs


def parse(text):
    """(strict object or None, lenient object or None)"""
    try:
        return json.loads(text), json.loads(text)
    except json.JSONDecodeError:
        pass
    m = re.search(r"\{.*\}", text, re.S)
    if m:
        try:
            return None, json.loads(m.group(0))
        except json.JSONDecodeError:
            pass
    return None, None


def norm(name):
    return re.sub(r"\s+", " ", str(name)).strip().lower()


def field_ok(field, got, gold):
    if field == "customer_name":
        return norm(got) == norm(gold)
    if field == "category":
        return got in gold
    if field == "items":
        if not isinstance(got, list) or len(got) != len(gold):
            return False
        left = [g for g in got if isinstance(g, dict)]
        for key, qty in gold:
            hit = next((g for g in left if key in str(g.get("product", "")).lower() and g.get("quantity") == qty), None)
            if hit is None:
                return False
            left.remove(hit)
        return True
    return got == gold and type(got) is type(gold)


def main():
    schema = json.load(open(os.path.join(HERE, "schema.json")))
    gold = {}
    for line in open(os.path.join(HERE, "inputs.jsonl")):
        if line.strip():
            e = json.loads(line)
            gold[e["id"]] = e["gold"]
    rows = [json.loads(l) for l in open(os.path.join(HERE, "results", "raw.jsonl")) if l.strip()]
    models = list(dict.fromkeys(r["model"] for r in rows))

    stats = defaultdict(lambda: defaultdict(list))
    detail = []
    for r in rows:
        k = (r["model"], r["mode"])
        st = stats[k]
        text = r.get("content", "")
        strict, lenient = parse(text) if "error" not in r else (None, None)
        obj = lenient
        valid = obj is not None and not validate(obj, schema)
        g = gold[r["id"]]
        fok = {f: obj is not None and isinstance(obj, dict) and f in obj and field_ok(f, obj[f], g[f]) for f in FIELDS}
        st["n"].append(1)
        st["error"].append("error" in r)
        st["strict"].append(strict is not None)
        st["lenient"].append(lenient is not None)
        st["valid"].append(valid)
        st["strict_valid"].append(strict is not None and valid)
        st["correct"].append(all(fok.values()))
        st["correct_valid"].append(all(fok.values()) and valid)
        for f in FIELDS:
            st["f_" + f].append(fok[f])
        st["truncated"].append(r.get("finish_reason") == "length")
        if r.get("gen_n"):
            st["gen_tps"].append(r["gen_n"] / (r["gen_ms"] / 1000))
            st["gen_n"].append(r["gen_n"])
            st["wall"].append(r["wall_s"])
        detail.append({"model": r["model"], "mode": r["mode"], "id": r["id"], "strict": strict is not None,
                       "parses": lenient is not None, "valid": valid,
                       "schema_errors": validate(obj, schema)[:3] if obj is not None else ["no JSON"],
                       "wrong_fields": [f for f in FIELDS if not fok[f]],
                       "finish_reason": r.get("finish_reason"), "error": r.get("error")})

    with open(os.path.join(HERE, "results", "graded.jsonl"), "w") as f:
        for d in detail:
            f.write(json.dumps(d, ensure_ascii=False) + "\n")

    def pct(xs):
        return f"{sum(xs)}/{len(xs)}"

    out = ["# Results", "", "Generated by `grade.py` from `raw.jsonl`. 20 emails per model and mode, temperature 0.", ""]
    out += ["## Parse, schema and accuracy", "",
            "| Model | Mode | Parses as-is | Parses after cleanup | Passes schema | All 5 fields right | Truncated |",
            "| --- | --- | --- | --- | --- | --- | --- |"]
    for m in models:
        for mode in MODES:
            st = stats[(m, mode)]
            if not st["n"]:
                continue
            out.append(f"| {m} | {mode} | {pct(st['strict'])} | {pct(st['lenient'])} | {pct(st['valid'])} | "
                       f"{pct(st['correct'])} | {sum(st['truncated'])} |")
    out += ["", "## Field accuracy (all modes)", "", "| Model | Mode | " + " | ".join(FIELDS) + " |",
            "| --- | --- | " + " | ".join("---" for _ in FIELDS) + " |"]
    for m in models:
        for mode in MODES:
            st = stats[(m, mode)]
            if st["n"]:
                out.append(f"| {m} | {mode} | " + " | ".join(pct(st['f_' + f]) for f in FIELDS) + " |")
    out += ["", "## Speed (machine under background load, compare modes within a model only)", "",
            "| Model | Mode | Median output tokens | Median generation t/s | Median seconds per email |",
            "| --- | --- | --- | --- | --- |"]
    for m in models:
        for mode in MODES:
            st = stats[(m, mode)]
            if st["gen_tps"]:
                out.append(f"| {m} | {mode} | {statistics.median(st['gen_n']):.0f} | "
                           f"{statistics.median(st['gen_tps']):.2f} | {statistics.median(st['wall']):.1f} |")
    out += ["", "## Every miss", "", "| Model | Mode | Email | Problem |", "| --- | --- | --- | --- |"]
    for d in detail:
        probs = []
        if d["error"]:
            probs.append("server error: " + d["error"][:80])
        elif not d["parses"]:
            probs.append("not JSON" + (" (hit max_tokens)" if d["finish_reason"] == "length" else ""))
        else:
            if not d["strict"]:
                probs.append("needed cleanup")
            if not d["valid"]:
                probs.append("schema: " + "; ".join(d["schema_errors"]))
            if d["wrong_fields"]:
                probs.append("wrong: " + ", ".join(d["wrong_fields"]))
        if probs:
            out.append(f"| {d['model']} | {d['mode']} | {d['id']} | {' / '.join(probs).replace('|', '/')} |")
    open(os.path.join(HERE, "results", "summary.md"), "w").write("\n".join(out) + "\n")
    print("\n".join(out[:8 + 3 * len(models)]))


if __name__ == "__main__":
    main()
