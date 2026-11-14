#!/usr/bin/env python3
"""Summarize results/raw.jsonl into results/summary.md. Standard library only.

For each server, model and condition: right answers, replies that parsed, the keys' order in the reply, mean output
tokens and mean seconds per request (2 requests in parallel). Paired comparisons use an exact two-sided sign test
on the problems where exactly one of the two conditions was right (McNemar's exact test).
"""
import collections
import json
import math
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
COND_ORDER = ["free", "answer_only", "reasoning_first", "answer_first", "prefixed"]


def sign_test(b, c):
    n = b + c
    if n == 0:
        return 1.0
    k = min(b, c)
    p = sum(math.comb(n, i) for i in range(k + 1)) / 2 ** n
    return min(1.0, 2 * p)


LEAD = re.compile(r'^\s*\{\s*"(?:b_)?answer"\s*:\s*(-?\d+(?:\.\d+)?)')


def right_incl_cut(r):
    """Right, also counting a reply cut off at max_tokens if the answer it had already written is right."""
    if r["correct"]:
        return True
    if r["cond"] == "free" or r["parsed"]:
        return False
    m = LEAD.match(r["text"])
    return bool(m) and abs(float(m.group(1)) - r["gold"]) < 1e-6


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "results", "raw.jsonl")
    rows = [json.loads(l) for l in open(path) if l.strip()]
    errors = [r for r in rows if "error" in r]
    rows = [r for r in rows if "error" not in r]
    by = collections.defaultdict(dict)
    for r in rows:
        by[(r["server"], r["model"], r["cond"])][r["id"]] = r
    out = ["# Summary", "", f"{len(rows)} replies, {len(errors)} HTTP errors. Greedy decoding, GSM8K test problems "
           "from the start of the file.", ""]
    out += ["| Server | Model | Condition | Right | Right incl. cut-off | Parsed | Key order in replies | Mean output tokens | "
            "Mean s/request | Hit max_tokens |", "| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |"]
    keys = sorted(by, key=lambda k: (k[1], k[0], COND_ORDER.index(k[2])))
    for k in keys:
        rs = list(by[k].values())
        n = len(rs)
        orders = collections.Counter(",".join(r["keys"]) if r.get("keys") else "-" for r in rs)
        order_s = "; ".join(f"`{o}` {c}" for o, c in orders.most_common()) if k[2] != "free" else "(text)"
        out.append(f"| {k[0]} | {k[1]} | {k[2]} | {sum(r['correct'] for r in rs)}/{n} | {sum(right_incl_cut(r) for r in rs)}/{n} | "
                   f"{sum(r['parsed'] for r in rs)}/{n} | {order_s} | "
                   f"{sum(r['completion_tokens'] or 0 for r in rs) / n:.0f} | "
                   f"{sum(r['seconds'] for r in rs) / n:.1f} | {sum(r['finish'] == 'length' for r in rs)} |")
    out += ["", "## Paired comparisons", "",
            "b = right only in the first condition, c = right only in the second; p = exact two-sided sign test.", "",
            "| Model | First | Second | Problems | Right (first / second) | b | c | p |",
            "| --- | --- | --- | --- | --- | --- | --- | --- |"]
    pairs = [
        (("llama-server-b11554", "reasoning_first"), ("llama-server-b11554", "answer_first")),
        (("llama-server-b11554", "reasoning_first"), ("llama-server-b11554", "answer_only")),
        (("llama-server-b11554", "answer_first"), ("llama-server-b11554", "answer_only")),
        (("llama-server-b11554", "free"), ("llama-server-b11554", "reasoning_first")),
        (("llama-server-b11554", "reasoning_first"), ("llama-server-b11554", "prefixed")),
        (("ollama-0.40.16", "prefixed"), ("ollama-0.40.16", "reasoning_first")),
        (("ollama-0.40.16", "prefixed"), ("ollama-0.40.16", "answer_only")),
        (("ollama-0.40.16", "reasoning_first"), ("ollama-0.40.16", "answer_only")),
        (("ollama-0.40.16", "reasoning_first"), ("llama-server-b11554", "reasoning_first")),
        (("ollama-0.40.16", "prefixed"), ("llama-server-b11554", "prefixed")),
    ]
    for model in sorted({k[1] for k in by}):
        for (s1, c1), (s2, c2) in pairs:
            a, b_ = by.get((s1, model, c1)), by.get((s2, model, c2))
            if not a or not b_:
                continue
            ids = sorted(set(a) & set(b_))
            ra = sum(a[i]["correct"] for i in ids)
            rb = sum(b_[i]["correct"] for i in ids)
            b = sum(a[i]["correct"] and not b_[i]["correct"] for i in ids)
            c = sum(b_[i]["correct"] and not a[i]["correct"] for i in ids)
            short = lambda s: "LS" if s.startswith("llama") else "Ollama"
            out.append(f"| {model} | {short(s1)} {c1} | {short(s2)} {c2} | {len(ids)} | {ra} / {rb} | {b} | {c} | "
                       f"{sign_test(b, c):.3f} |")
    out += ["", "## Answer written before the reasoning", "",
            "Replies whose keys put the answer first (cut-off replies are not included here; see 'Right incl. cut-off'). 'Reasoning ends on the right number' = the last number in the "
            "reasoning text equals the gold answer; 'contradicts' = the reasoning's last number differs from the "
            "committed answer.", "",
            "| Server | Model | Condition | Answer-first replies | Answer wrong | ...of those, reasoning ends on the "
            "right number | Reasoning's last number contradicts the answer |", "| --- | --- | --- | --- | --- | --- | --- |"]
    for k in keys:
        rs = [r for r in by[k].values() if r.get("keys") and r["keys"][0] == "answer" and len(r["keys"]) == 2]
        if not rs:
            continue
        wrong = [r for r in rs if not r["correct"]]
        fixed = [r for r in wrong if r["reasoning_last_number"] is not None
                 and abs(r["reasoning_last_number"] - r["gold"]) < 1e-6]
        contra = [r for r in rs if r["reasoning_last_number"] is not None and r["pred"] is not None
                  and abs(r["reasoning_last_number"] - r["pred"]) > 1e-6]
        out.append(f"| {k[0]} | {k[1]} | {k[2]} | {len(rs)} | {len(wrong)} | {len(fixed)} | {len(contra)} |")
    if errors:
        out += ["", "## HTTP errors", ""] + [f"- {e['server']} {e['model']} {e['cond']} #{e['id']}: {e['error'][:200]}"
                                             for e in errors]
    text = "\n".join(out) + "\n"
    open(os.path.join(HERE, "results", "summary.md"), "w").write(text)
    print(text)


if __name__ == "__main__":
    main()
