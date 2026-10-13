#!/usr/bin/env python3
"""Chat-level checks against a running llama-server (OpenAI-compatible API).

1. Latency: a 350-token chat request, streamed. Records the time until the first
   generated token arrives at the client, and the server's own prompt/generation timings.
2. Sanity check: 20 short questions with one right answer each (questions.jsonl),
   temperature 0, thinking disabled. Graded by the rules in grade() below.

Standard library only. Appends JSON lines to the --out file.
"""
import argparse
import json
import os
import re
import time
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))


def chat(base, messages, max_tokens, stream):
    payload = {
        "messages": messages, "max_tokens": max_tokens, "temperature": 0, "seed": 42,
        "stream": stream, "cache_prompt": False,
        # Templates that support a thinking switch (Qwen 3.5, SmolLM3) read this; others ignore it.
        "chat_template_kwargs": {"enable_thinking": False},
    }
    if stream:
        payload["stream_options"] = {"include_usage": True}
    req = urllib.request.Request(f"{base}/v1/chat/completions", data=json.dumps(payload).encode(),
                                 headers={"Content-Type": "application/json"})
    t0 = time.perf_counter()
    with urllib.request.urlopen(req, timeout=1800) as r:
        if not stream:
            body = json.loads(r.read())
            return body, time.perf_counter() - t0, None
        first = None
        text = []
        timings = None
        for raw in r:
            line = raw.decode().strip()
            if not line.startswith("data: ") or line == "data: [DONE]":
                continue
            ev = json.loads(line[6:])
            for ch in ev.get("choices", []):
                delta = ch.get("delta", {})
                piece = delta.get("content") or delta.get("reasoning_content")
                if piece:
                    if first is None:
                        first = time.perf_counter() - t0
                    text.append(piece)
            if "timings" in ev:
                timings = ev["timings"]
        return {"text": "".join(text), "timings": timings}, time.perf_counter() - t0, first


def numbers(s):
    s = re.sub(r"(?<=\d),(?=\d{3}\b)", "", s)  # 5,000 -> 5000
    return re.findall(r"-?\d+(?:\.\d+)?", s)


def grade(item, answer):
    a = answer.strip()
    low = a.lower()
    kind, exp = item["check"], item["expect"]
    if kind == "number":
        # The first number: the question asks for the number alone. Verbose answers are reviewed
        # by hand and corrected in results/grading-overrides.tsv.
        nums = numbers(a)
        return bool(nums) and float(nums[0]) == float(exp)
    if kind == "contains":
        return exp in low
    if kind == "yesno":
        words = re.findall(r"[a-z]+", low)
        return bool(words) and words[0] == exp
    if kind == "sequence":
        return ",".join(numbers(a)) == exp
    if kind == "regex":
        return re.search(exp, a) is not None
    if kind == "json":
        m = re.search(r"\{.*\}", a, re.S)
        if not m:
            return False
        try:
            obj = json.loads(m.group(0))
        except json.JSONDecodeError:
            return False
        return str(obj.get("name")) == exp["name"] and str(obj.get("age")) == str(exp["age"])
    raise ValueError(kind)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--base", default="http://127.0.0.1:11700")
    ap.add_argument("--model", required=True)
    ap.add_argument("--runs", type=int, default=3)
    ap.add_argument("--out", required=True)
    ap.add_argument("--tag", default="", help="written into latency rows")
    ap.add_argument("--latency-only", action="store_true", help="skip the 20 questions")
    args = ap.parse_args()

    with open(os.path.join(HERE, "prompt.txt")) as f:
        note = f.read().strip().removesuffix("1.").strip()
    rows = []

    for i in range(1 + args.runs):
        # A different first line every run, so no prompt cache can be reused.
        msg = (f"Request {i}.\n\n{note}\n\nList the three problems the team had with the hosted API, "
               "one short sentence each.")
        body, wall, first = chat(args.base, [{"role": "user", "content": msg}], 64, stream=True)
        t = body["timings"] or {}
        row = {"kind": "latency", "model": args.model, "run": i, "warmup": i == 0,
               "ttft_s": first, "wall_s": wall, "prompt_tokens": t.get("prompt_n"),
               "prompt_ms": t.get("prompt_ms"), "gen_tokens": t.get("predicted_n"),
               "gen_ms": t.get("predicted_ms"), "text": body["text"]}
        row["tag"] = args.tag
        rows.append(row)
        print(f"{args.model} latency run {i}: first token {first:.2f} s, "
              f"{row['prompt_tokens']} prompt tokens, wall {wall:.1f} s", flush=True)

    with open(os.path.join(HERE, "questions.jsonl")) as f:
        items = [json.loads(line) for line in f if line.strip()]
    if args.latency_only:
        items = []
    passed = 0
    for item in items:
        body, wall, _ = chat(args.base, [{"role": "user", "content": item["q"]}], 200, stream=False)
        msg = body["choices"][0]["message"]
        answer = msg.get("content") or ""
        ok = grade(item, answer)
        passed += ok
        t = body.get("timings", {})
        rows.append({"kind": "question", "model": args.model, "id": item["id"], "pass": ok,
                     "answer": answer, "reasoning_chars": len(msg.get("reasoning_content") or ""),
                     "gen_tokens": t.get("predicted_n"), "wall_s": wall})
        print(f"{args.model} {item['id']:9s} {'PASS' if ok else 'fail'}  {answer.strip()[:70]!r}", flush=True)
    if items:
        print(f"{args.model}: {passed}/{len(items)} correct", flush=True)

    with open(args.out, "a") as f:
        for r in rows:
            f.write(json.dumps(r) + "\n")


if __name__ == "__main__":
    main()
