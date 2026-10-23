#!/usr/bin/env python3
"""Summarizes results/guard-raw.jsonl into results/guard-summary.md. Standard library only.

A label is the first line of the reply ("safe" or "unsafe"); the category is the second line (S1 to S14).
For each prompt and route: how often each label came back over the repeats, and whether the route ever disagreed
with itself. /api/chat runs at the shipped temperature 0, so its label is used as the reference.
"""
import json
import os
from collections import Counter, defaultdict

HERE = os.path.dirname(os.path.abspath(__file__))


def label(text):
    lines = [l.strip() for l in (text or "").strip().splitlines() if l.strip()]
    first = lines[0].lower() if lines else ""
    lab = first if first in ("safe", "unsafe") else "other"
    return lab, (lines[1] if lab == "unsafe" and len(lines) > 1 else "")


def main():
    rows = [json.loads(l) for l in open(os.path.join(HERE, "results", "guard-raw.jsonl")) if l.strip()]
    prompts = {json.loads(l)["id"]: json.loads(l) for l in open(os.path.join(HERE, "guard-prompts.jsonl")) if l.strip()}
    by = defaultdict(list)
    for r in rows:
        by[(r["id"], r["route"])].append(label(r.get("content")))
    reps = max(len(v) for v in by.values())
    out = ["# llama-guard3:1b: same prompt, repeated", "",
           f"{len(rows)} requests: {len(prompts)} prompts x {reps} repeats x 2 routes, nothing sent but the prompt "
           "(and a per-request seed). /api/chat applies the shipped `temperature 0`; /v1/chat/completions samples at "
           "temperature 1.0 and top_p 1.0.", ""]
    tot = {}
    for route in ("api", "v1"):
        flips = sum(len({lab for lab, _ in by[(pid, route)]}) > 1 for pid in prompts)
        cats = sum(len({c for lab, c in by[(pid, route)] if lab == "unsafe"}) > 1 for pid in prompts)
        ref = {pid: Counter(lab for lab, _ in by[(pid, "api")]).most_common(1)[0][0] for pid in prompts}
        off = sum(lab != ref[pid] for pid in prompts for lab, _ in by[(pid, route)])
        n = sum(len(by[(pid, route)]) for pid in prompts)
        tot[route] = (flips, cats, off, n)
    out += ["| Route | Prompts whose label changed between repeats | Prompts with more than one unsafe category | "
            "Labels different from the /api/chat label |", "| --- | --- | --- | --- |"]
    for route, (flips, cats, off, n) in tot.items():
        out.append(f"| {route} | {flips}/{len(prompts)} | {cats}/{len(prompts)} | {off}/{n} |")
    out += ["", "## Per prompt", "",
            "Counts over the repeats. Kind is how I wrote the prompt (safe, borderline, unsafe), not a ground truth.", "",
            "| Prompt | Kind | /api/chat | /api/chat categories | /v1 | /v1 categories |",
            "| --- | --- | --- | --- | --- | --- |"]
    for pid, p in prompts.items():
        def fmt(route):
            c = Counter(lab for lab, _ in by[(pid, route)])
            return ", ".join(f"{k} {v}" for k, v in sorted(c.items()))
        def cats(route):
            c = Counter(c for lab, c in by[(pid, route)] if lab == "unsafe")
            return ", ".join(f"{k} {v}" for k, v in sorted(c.items())) or "-"
        out.append(f"| {pid}: {p['text'][:70]} | {p['kind']} | {fmt('api')} | {cats('api')} | {fmt('v1')} | "
                   f"{cats('v1')} |")
    by_kind = defaultdict(Counter)
    for r in rows:
        by_kind[(r["kind"], r["route"])][label(r.get("content"))[0]] += 1
    out += ["", "## By kind", "", "| Kind | Route | safe | unsafe | other |", "| --- | --- | --- | --- | --- |"]
    for kind in ("safe", "borderline", "unsafe"):
        for route in ("api", "v1"):
            c = by_kind[(kind, route)]
            out.append(f"| {kind} | {route} | {c['safe']} | {c['unsafe']} | {c['other']} |")
    open(os.path.join(HERE, "results", "guard-summary.md"), "w").write("\n".join(out) + "\n")
    print("\n".join(out[:9]))


if __name__ == "__main__":
    main()
