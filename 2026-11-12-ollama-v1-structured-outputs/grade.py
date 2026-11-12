#!/usr/bin/env python3
"""Scores results/raw.jsonl and writes results/summary.md and results/graded.jsonl. Standard library only.

validate, parse, norm and field_ok are copied unchanged from the 2026-10-14, 10-18 and 10-22 benches:
  parses   json.loads() on the whole reply (strict); "after cleanup" also accepts a fenced reply or text around
           one {...} object.
  valid    the parsed object passes schema.json.
  correct  customer_name, order_id, category, refund_requested and items all match the hand-written labels.
Also recorded: whether the keys came out in schema order or alphabetical order, truncation (hit max_tokens),
output tokens and wall time per reply.
"""
import argparse
import json
import os
import re
import statistics
from collections import defaultdict

HERE = os.path.dirname(os.path.abspath(__file__))
FIELDS = ["customer_name", "order_id", "category", "refund_requested", "items"]
CONDS = ["t0", "t0-schema", "none", "schema", "object", "schema-only", "1.5", "1.5-schema", "ls-declared", "ls-alpha"]
VERSIONS = ["0.40.13", "0.40.16", "llama-server"]
SAMPLERS = {  # what Ollama's runner logged (results/effective-params.txt) for an imported GGUF with no PARAMETERs
    "0.40.13": {"t0": "greedy", "t0-schema": "greedy", "1.5": "temp 1.5, top_p 1.0", "1.5-schema": "temp 1.5, top_p 1.0",
                "*": "temp 1.0, top_p 1.0"},
    "0.40.16": {"t0": "greedy", "t0-schema": "greedy", "1.5": "temp 1.5, top_p 0.9", "1.5-schema": "temp 1.5, top_p 0.9",
                "*": "temp 0.8, top_p 0.9"},
    "llama-server": {"*": "temp 0.8, top_p 0.9, min_p 0 (sent)"},
}


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


def key_order(text, schema_keys):
    keys = re.findall(r'^\s{0,4}"([a-z_]+)"\s*:', text, re.M)
    top = [k for k in keys if k in schema_keys]
    if len(top) < 2:
        return "-"
    if top == sorted(top):
        return "alphabetical"
    if top == [k for k in schema_keys if k in top]:
        return "schema"
    return "other"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dir", default=os.path.join(HERE, "results"))
    args = ap.parse_args()
    schema = json.load(open(os.path.join(HERE, "schema.json")))
    skeys = list(schema["properties"])
    gold = {e["id"]: e["gold"] for e in (json.loads(l) for l in open(os.path.join(HERE, "inputs.jsonl")) if l.strip())}
    rows = [json.loads(l) for l in open(os.path.join(args.dir, "raw.jsonl")) if l.strip()]
    models = list(dict.fromkeys(r["model"] for r in rows))

    st = defaultdict(lambda: defaultdict(list))
    detail = []
    for r in rows:
        text = r.get("content", "")
        strict, lenient = parse(text) if "error" not in r else (None, None)
        obj = lenient
        valid = isinstance(obj, dict) and not validate(obj, schema)
        g = gold[r["id"]]
        fok = {f: isinstance(obj, dict) and f in obj and field_ok(f, obj[f], g[f]) for f in FIELDS}
        correct = all(fok.values())
        d = {"version": r["version"], "model": r["model"], "cond": r["cond"], "id": r["id"], "rep": r["rep"],
             "seed": r["seed"], "strict": strict is not None, "parses": lenient is not None, "valid": valid,
             "correct": correct, "fence": "```" in text, "think": "<think>" in text,
             "truncated": r.get("finish_reason") == "length", "order": key_order(text, skeys),
             "wrong_fields": [f for f in FIELDS if not fok[f]],
             "schema_errors": validate(obj, schema)[:3] if isinstance(obj, dict) else ["no JSON object"],
             "gen_n": r.get("gen_n"), "wall_s": r.get("wall_s"), "error": r.get("error")}
        detail.append(d)
        for key in [(r["version"], r["model"], r["cond"]), (r["version"], "ALL", r["cond"])]:
            s = st[key]
            for k in ("strict", "parses", "valid", "correct", "fence", "think", "truncated"):
                s[k].append(d[k])
            s["strict_valid"].append(d["strict"] and valid)
            s["order"].append(d["order"])
            for f in FIELDS:
                s["f_" + f].append(fok[f])
            if r.get("gen_n"):
                s["gen_n"].append(r["gen_n"])
                s["wall"].append(r["wall_s"])

    with open(os.path.join(args.dir, "graded.jsonl"), "w") as f:
        for d in detail:
            f.write(json.dumps(d, ensure_ascii=False) + "\n")

    def c(xs):
        return f"{sum(xs)}/{len(xs)}"

    def samplers(v, cond):
        return SAMPLERS[v].get(cond, SAMPLERS[v]["*"])

    out = ["# Results", "", "Generated by `grade.py` from `raw.jsonl`. Ollama `/v1/chat/completions` only. 20 emails "
           "per model; t0 conditions run once per email, sampled conditions twice, each with its own seed (the same "
           "seeds on both versions).", ""]
    for title, m in [("All models", "ALL")] + [(x, x) for x in models]:
        out += [f"## {title}", "",
                "| Ollama | Condition | Samplers | Valid as-is | Valid after cleanup | All 5 fields right | Fenced | "
                "Truncated | Key order (schema/alpha) |",
                "| --- | --- | --- | --- | --- | --- | --- | --- | --- |"]
        for v in VERSIONS:
            for cond in CONDS:
                s = st[(v, m, cond)]
                if not s["valid"]:
                    continue
                out.append(f"| {v} | {cond} | {samplers(v, cond)} | {c(s['strict_valid'])} | {c(s['valid'])} | "
                           f"{c(s['correct'])} | {sum(s['fence'])} | {sum(s['truncated'])} | "
                           f"{s['order'].count('schema')}/{s['order'].count('alphabetical')} |")
        out.append("")
    out += ["## Field accuracy and speed, all models", "",
            "| Ollama | Condition | " + " | ".join(FIELDS) + " | Median output tokens | Median seconds per reply |",
            "| --- | --- | " + " | ".join("---" for _ in FIELDS) + " | --- | --- |"]
    for v in VERSIONS:
        for cond in CONDS:
            s = st[(v, "ALL", cond)]
            if s["valid"]:
                out.append(f"| {v} | {cond} | " + " | ".join(c(s['f_' + f]) for f in FIELDS) +
                           f" | {statistics.median(s['gen_n']) if s['gen_n'] else '-'} | "
                           f"{statistics.median(s['wall']) if s['wall'] else '-'} |")
    out += ["", "## Greedy control: 0.40.13 vs 0.40.16 at temperature 0, no response_format", "",
            "| Model | Identical replies |", "| --- | --- |"]
    by = {(r["version"], r["model"], r["id"]): r.get("content", "") for r in rows if r["cond"] == "t0"}
    for m in models:
        ids = sorted({k[2] for k in by if k[1] == m and ("0.40.13", m, k[2]) in by and ("0.40.16", m, k[2]) in by})
        same = sum(by[("0.40.13", m, i)].strip() == by[("0.40.16", m, i)].strip() for i in ids)
        out.append(f"| {m} | {same}/{len(ids)} |")
    out += ["", "## Every miss", "", "| Ollama | Model | Condition | Email | Repeat | Problem |",
            "| --- | --- | --- | --- | --- | --- |"]
    for d in detail:
        probs = []
        if d["error"]:
            probs.append("server error: " + d["error"][:80])
        elif not d["parses"]:
            probs.append("not JSON" + (" (hit max_tokens)" if d["truncated"] else ""))
        else:
            if not d["strict"]:
                probs.append("needed cleanup" + (" (fence)" if d["fence"] else "") + (" (think)" if d["think"] else ""))
            if not d["valid"]:
                probs.append("schema: " + "; ".join(d["schema_errors"]))
            if d["wrong_fields"]:
                probs.append("wrong: " + ", ".join(d["wrong_fields"]))
        if probs:
            out.append(f"| {d['version']} | {d['model']} | {d['cond']} | {d['id']} | {d['rep']} | "
                       f"{' / '.join(probs).replace('|', '/')} |")
    open(os.path.join(args.dir, "summary.md"), "w").write("\n".join(out) + "\n")
    print("\n".join(out[:24]))


if __name__ == "__main__":
    main()
