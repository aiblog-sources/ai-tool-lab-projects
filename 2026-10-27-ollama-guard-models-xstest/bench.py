#!/usr/bin/env python3
"""Sends every XSTest prompt (data/xstest_prompts.csv, 250 safe + 200 unsafe, labelled by the XSTest authors) to one
safety classifier from the Ollama library through /api/chat, with the model's own template and shipped temperature
(0 for all three tested here), and appends one JSON line per prompt to --out.

Each row keeps the verdict text, the probability the classifier gave to its "unsafe" first token (from Ollama's
logprobs), token counts and timings. Standard library only.
"""
import argparse
import csv
import json
import math
import os
import time
import urllib.request
import zlib
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))

# First token that means "flag this prompt" and the one that means "let it through", per model family.
TOKENS = {
    "llama-guard3": ("unsafe", "safe"),
    "shieldgemma": ("Yes", "No"),
    "granite3-guardian": ("Yes", "No"),
}


def load_prompts():
    with open(os.path.join(HERE, "data", "xstest_prompts.csv"), newline="") as f:
        return list(csv.DictReader(f))


def post(base, payload):
    req = urllib.request.Request(base + "/api/chat", json.dumps(payload).encode(),
                                 {"Content-Type": "application/json"})
    t0 = time.perf_counter()
    with urllib.request.urlopen(req, timeout=600) as r:
        body = json.load(r)
    return body, time.perf_counter() - t0


def p_flag(family, logprobs):
    """P(flag) renormalised over the two verdict tokens, from the first token's top logprobs."""
    if not logprobs:
        return None
    flag, ok = TOKENS[family]
    top = {}
    for t in logprobs[0]["top_logprobs"]:  # sorted best first; keep the best spelling (" No" after "No")
        top.setdefault(t["token"].strip(), t["logprob"])
    if flag not in top and ok not in top:
        return None
    pf = math.exp(top.get(flag, -50.0))
    po = math.exp(top.get(ok, -50.0))
    return pf / (pf + po)


def verdict(family, text):
    first = text.strip().split()[0].strip(".,:").lower() if text.strip() else ""
    flag, ok = (t.lower() for t in TOKENS[family])
    return "flag" if first == flag else "pass" if first == ok else "other"


def run_one(base, model, family, p, num_predict):
    seed = zlib.crc32(f"{model}|{p['id']}".encode())
    payload = {"model": model, "stream": False, "messages": [{"role": "user", "content": p["prompt"]}],
               "options": {"num_predict": num_predict, "num_ctx": 2048, "seed": seed},
               "logprobs": True, "top_logprobs": 5}
    body, wall = post(base, payload)
    text = body.get("message", {}).get("content", "")
    return {"model": model, "id": int(p["id"]), "type": p["type"], "label": p["label"], "content": text,
            "verdict": verdict(family, text), "p_flag": p_flag(family, body.get("logprobs")),
            "prompt_tokens": body.get("prompt_eval_count"), "eval_tokens": body.get("eval_count"),
            "prompt_eval_s": round(body.get("prompt_eval_duration", 0) / 1e9, 3),
            "total_s": round(body.get("total_duration", 0) / 1e9, 3), "wall_s": round(wall, 3), "seed": seed}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--base", required=True)
    ap.add_argument("--model", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--limit", type=int, default=0)
    ap.add_argument("--workers", type=int, default=2)
    args = ap.parse_args()
    family = next(k for k in TOKENS if args.model.startswith(k))  # shieldgemma-userprompt -> shieldgemma
    # Llama Guard writes the categories on a second line; the other two only need the first word.
    num_predict = 10 if family == "llama-guard3" else 2
    prompts = load_prompts()
    done = set()
    if os.path.exists(args.out):
        done = {(r["model"], r["id"]) for r in map(json.loads, open(args.out)) if r.get("model")}
    todo = [p for p in prompts if (args.model, int(p["id"])) not in done]
    if args.limit:
        todo = todo[:args.limit]
    with open(args.out, "a") as out, ThreadPoolExecutor(args.workers) as pool:
        for row in pool.map(lambda p: run_one(args.base, args.model, family, p, num_predict), todo):
            out.write(json.dumps(row) + "\n")
            out.flush()
            print(row["id"], row["label"], row["verdict"], repr(row["content"])[:30], row["p_flag"],
                  row["wall_s"], flush=True)


if __name__ == "__main__":
    main()
