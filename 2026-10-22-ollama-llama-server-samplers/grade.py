#!/usr/bin/env python3
"""Scores results/raw.jsonl and writes results/summary.md. Standard library only.

Same checks as the 2026-10-14 and 2026-10-18 benches (validate, parse and field_ok are copied unchanged):
  parses   json.loads() on the whole reply (strict). "parses after cleanup" also accepts a reply wrapped in a
           Markdown code fence or with text around one {...} object.
  valid    the parsed object passes schema.json.
  correct  customer_name, order_id, category, refund_requested and items all match the hand-written labels.
Failure kinds: fence (JSON inside a Markdown fence), think (an empty <think></think> block before or instead of
the JSON), truncated (hit max_tokens), not-json, schema, wrong-fields.
"""
import argparse
import json
import os
import re
import statistics
from collections import defaultdict

HERE = os.path.dirname(os.path.abspath(__file__))
FIELDS = ["customer_name", "order_id", "category", "refund_requested", "items"]
MODES = ["prompt"]
CONDS = ["ls-t0", "ol-t0", "ls-default", "ol-api-default", "ol-v1-default", "ls-1.5", "ol-api-1.5", "ol-v1-1.5",
         "ol-api-1.5-fix"]
SAMPLERS = {  # what each server logged for the condition (results/effective-params.txt)
    "ls-t0": "greedy", "ol-t0": "temp 0 (greedy)",
    "ls-default": "temp 0.8, top_k 40, top_p 0.95, min_p 0.05", "ol-api-default": "temp 0.8, top_k 40, top_p 0.9, min_p 0",
    "ol-v1-default": "temp 1.0, top_k 40, top_p 1.0, min_p 0", "ls-1.5": "temp 1.5, top_k 40, top_p 0.95, min_p 0.05",
    "ol-api-1.5": "temp 1.5, top_k 40, top_p 0.9, min_p 0", "ol-v1-1.5": "temp 1.5, top_k 40, top_p 1.0, min_p 0",
    "ol-api-1.5-fix": "temp 1.5, top_k 40, top_p 0.95, min_p 0.05"}


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


def kinds(text, strict, lenient, valid, correct, finish):
    k = []
    if "```" in text:
        k.append("fence")
    if "<think>" in text:
        k.append("think")
    if finish == "length":
        k.append("truncated")
    if lenient is None:
        k.append("not-json")
    elif not valid:
        k.append("schema")
    elif not correct:
        k.append("wrong-fields")
    return k


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dir", default=os.path.join(HERE, "results"), help="folder with raw.jsonl")
    args = ap.parse_args()
    schema = json.load(open(os.path.join(HERE, "schema.json")))
    gold = {}
    for line in open(os.path.join(HERE, "inputs.jsonl")):
        if line.strip():
            e = json.loads(line)
            gold[e["id"]] = e["gold"]
    rows = [json.loads(l) for l in open(os.path.join(args.dir, "raw.jsonl")) if l.strip()]
    models = list(dict.fromkeys(r["model"] for r in rows))
    shared = not any("rep" in r for r in rows)

    st = defaultdict(lambda: defaultdict(list))
    detail = []
    for r in rows:
        text = r.get("content", "")
        strict, lenient = parse(text) if "error" not in r else (None, None)
        obj = lenient
        valid = obj is not None and isinstance(obj, dict) and not validate(obj, schema)
        g = gold[r["id"]]
        fok = {f: isinstance(obj, dict) and f in obj and field_ok(f, obj[f], g[f]) for f in FIELDS}
        correct = all(fok.values())
        invented = isinstance(obj, dict) and g["order_id"] is None and obj.get("order_id") not in (None, "")
        k = kinds(text, strict, lenient, valid, correct, r.get("finish_reason"))
        d = {"model": r["model"], "cond": r["cond"], "mode": r["mode"], "id": r["id"], "rep": r.get("rep", r["seed"]), "seed": r["seed"],
             "strict": strict is not None, "parses": lenient is not None, "valid": valid, "correct": correct,
             "invented_order_id": invented, "kinds": k, "wrong_fields": [f for f in FIELDS if not fok[f]],
             "schema_errors": (validate(obj, schema)[:3] if isinstance(obj, dict) else ["no JSON object"]),
             "gen_n": r.get("gen_n"), "error": r.get("error")}
        detail.append(d)
        for key in [(r["model"], r["cond"], r["mode"]), ("ALL", r["cond"], r["mode"])]:
            s = st[key]
            s["n"].append(1)
            s["strict"].append(d["strict"])
            s["lenient"].append(d["parses"])
            s["valid"].append(valid)
            s["strict_valid"].append(d["strict"] and valid)
            s["correct"].append(correct)
            s["invented"].append(invented)
            s["no_order_emails"].append(g["order_id"] is None)
            for kk in ["fence", "think", "truncated", "not-json", "schema"]:
                s[kk].append(kk in k)
            for f in FIELDS:
                s["f_" + f].append(fok[f])
            if r.get("gen_n"):
                s["gen_n"].append(r["gen_n"])

    with open(os.path.join(args.dir, "graded.jsonl"), "w") as f:
        for d in detail:
            f.write(json.dumps(d, ensure_ascii=False) + "\n")

    def c(xs):
        return f"{sum(xs)}/{len(xs)}"

    out = ["# Results", "",
           "Generated by `grade.py` from `raw.jsonl`. 20 emails per model; the t0 conditions run once, every sampled "
           "condition runs twice, each request with its own seed. ls = llama-server, ol-api = Ollama /api/chat, "
           "ol-v1 = Ollama /v1/chat/completions. Effective samplers (from the server logs):", ""]
    out += [f"- `{k}`: {v}" for k, v in SAMPLERS.items()] + [""]
    for title, ms in [("All models", ["ALL"])] + [(m, [m]) for m in models]:
        out += [f"## {title}", "",
                "| Condition | Mode | Parses as-is | Parses + passes schema as-is | Passes schema after cleanup | "
                "All 5 fields right | Fenced | Think tags | Truncated | Invented order id |",
                "| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |"]
        for cond in CONDS:
            for mode in MODES:
                s = st[(ms[0], cond, mode)]
                if not s["n"]:
                    continue
                out.append(f"| {cond} | {mode} | {c(s['strict'])} | {c(s['strict_valid'])} | {c(s['valid'])} | "
                           f"{c(s['correct'])} | {sum(s['fence'])} | {sum(s['think'])} | {sum(s['truncated'])} | "
                           f"{sum(s['invented'])}/{sum(s['no_order_emails'])} |")
        out.append("")
    out += ["## Fenced replies by repeat (prompt mode)", "",
            "Counts per repeat. In the shared-seed run one seed served all 20 emails of a repeat." if shared else
            "Counts per repeat. Every reply here had its own seed.", "",
            "| Model | Condition | Repeat | Fenced | Think tags |", "| --- | --- | --- | --- | --- |"]
    rep = defaultdict(lambda: [0, 0, 0])
    for d in detail:
        if d["mode"] == "prompt" and not d["cond"].endswith("t0"):
            x = rep[(d["model"], d["cond"], d["rep"])]
            x[0] += 1
            x[1] += "fence" in d["kinds"]
            x[2] += "think" in d["kinds"]
    for (m, cond, r), (n, fe, th) in sorted(rep.items(), key=lambda kv: (models.index(kv[0][0]), CONDS.index(kv[0][1]), kv[0][2])):
        out.append(f"| {m} | {cond} | {r} | {fe}/{n} | {th}/{n} |")
    out.append("")
    out += ["## Greedy control: llama-server vs Ollama at temperature 0", "",
            "Same GGUF, temperature 0 on both. Identical = the two replies are the same text.", "",
            "| Model | Identical replies | Same parsed object |", "| --- | --- | --- |"]
    by = {(r["model"], r["cond"], r["id"]): r.get("content", "") for r in rows if r["cond"] in ("ls-t0", "ol-t0")}
    for m in models:
        ids = sorted({k[2] for k in by if k[0] == m and (m, "ls-t0", k[2]) in by and (m, "ol-t0", k[2]) in by})
        same = sum(by[(m, "ls-t0", i)].strip() == by[(m, "ol-t0", i)].strip() for i in ids)
        same_obj = sum(parse(by[(m, "ls-t0", i)])[1] == parse(by[(m, "ol-t0", i)])[1] for i in ids)
        out.append(f"| {m} | {same}/{len(ids)} | {same_obj}/{len(ids)} |")
    out.append("")
    out += ["## Field accuracy, all models", "", "| Condition | Mode | " + " | ".join(FIELDS) + " | Median output tokens |",
            "| --- | --- | " + " | ".join("---" for _ in FIELDS) + " | --- |"]
    for cond in CONDS:
        for mode in MODES:
            s = st[("ALL", cond, mode)]
            if s["n"]:
                out.append(f"| {cond} | {mode} | " + " | ".join(c(s['f_' + f]) for f in FIELDS) +
                           f" | {statistics.median(s['gen_n']) if s['gen_n'] else '-'} |")
    out += ["", "## Every miss", "", "| Model | Condition | Mode | Email | Repeat | Problem |",
            "| --- | --- | --- | --- | --- | --- |"]
    for d in detail:
        probs = []
        if d["error"]:
            probs.append("server error: " + d["error"][:80])
        elif not d["parses"]:
            probs.append("not JSON" + (" (hit max_tokens)" if "truncated" in d["kinds"] else ""))
        else:
            if not d["strict"]:
                probs.append("needed cleanup (" + ", ".join(k for k in d["kinds"] if k in ("fence", "think")) + ")")
            if not d["valid"]:
                probs.append("schema: " + "; ".join(d["schema_errors"]))
            if d["wrong_fields"]:
                probs.append("wrong: " + ", ".join(d["wrong_fields"]))
        if probs:
            out.append(f"| {d['model']} | {d['cond']} | {d['mode']} | {d['id']} | {d['rep']} | "
                       f"{' / '.join(probs).replace('|', '/')} |")
    open(os.path.join(args.dir, "summary.md"), "w").write("\n".join(out) + "\n")
    print("\n".join(out[:16]))


if __name__ == "__main__":
    main()
