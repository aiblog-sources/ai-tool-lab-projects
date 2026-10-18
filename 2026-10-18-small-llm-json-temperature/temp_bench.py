#!/usr/bin/env python3
"""Asks a running llama-server to turn 20 support emails into JSON tickets at several temperatures.

Conditions (see CONDITIONS below):
  t0            greedy, one run (the baseline; temperature 0 ignores the seed)
  default-T     temperature T with llama-server's default samplers: top_k 40, top_p 0.95, min_p 0.05,
                applied BEFORE temperature (llama.cpp's sampler order puts temperature last)
  pure-T        temperature T with every other sampler off (top_k 0, top_p 1, min_p 0)
Each sampled condition runs SEEDS times per email. Every (email, condition, repeat) gets its own seed, a CRC32 of
those three, shared by the two modes so prompt and grammar replies are paired. (A first version gave every email the
same seed 1 or 2: seed 1 then made the same first-token choice on all 20 emails. That data is in
results/shared-seed/.) Each of those runs in two modes:
  prompt   the JSON Schema is in the system prompt, nothing constrains the output
  grammar  the same prompt plus response_format json_schema (llama-server compiles it into a grammar)

Four worker threads, each pinned to one server slot (id_slot) and one mode, so the fixed system prompt stays
cached. Thinking off, at most 300 output tokens. Standard library only. Appends one JSON line per request to --out.
"""
import argparse
import json
import os
import queue
import threading
import time
import urllib.request
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
MODES = ["prompt", "grammar"]
TEMPS = [0.8, 1.5]
SEEDS = 2
DEFAULT = {"top_k": 40, "top_p": 0.95, "min_p": 0.05}
PURE = {"top_k": 0, "top_p": 1.0, "min_p": 0.0}
CONDITIONS = [("t0", 0.0, DEFAULT, [0])]
for t in TEMPS:
    CONDITIONS.append((f"default-{t}", t, DEFAULT, list(range(1, SEEDS + 1))))
    CONDITIONS.append((f"pure-{t}", t, PURE, list(range(1, SEEDS + 1))))


def system_prompt(schema):
    return ("You turn customer support emails into JSON tickets. Reply with only a JSON object that matches "
            "this JSON Schema, with no other text:\n\n" + json.dumps(schema, indent=2))


def request(base, mode, slot, schema, email, temp, samplers, seed, max_tokens):
    payload = {
        "messages": [{"role": "system", "content": system_prompt(schema)},
                     {"role": "user", "content": email}],
        "max_tokens": max_tokens, "temperature": temp, "seed": seed, "id_slot": slot, "cache_prompt": True,
        "repeat_penalty": 1.0, "chat_template_kwargs": {"enable_thinking": False}, **samplers,
    }
    if mode == "grammar":
        payload["response_format"] = {"type": "json_schema",
                                      "json_schema": {"name": "ticket", "strict": True, "schema": schema}}
    req = urllib.request.Request(f"{base}/v1/chat/completions", data=json.dumps(payload).encode(),
                                 headers={"Content-Type": "application/json"})
    t0 = time.perf_counter()
    try:
        with urllib.request.urlopen(req, timeout=900) as r:
            body = json.loads(r.read())
    except urllib.error.HTTPError as e:
        return {"error": f"HTTP {e.code}: {e.read().decode()[:500]}"}, time.perf_counter() - t0
    return body, time.perf_counter() - t0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--base", default="http://127.0.0.1:11711")
    ap.add_argument("--model", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--max-tokens", type=int, default=300)
    ap.add_argument("--limit", type=int, default=0, help="only the first N emails (smoke test)")
    ap.add_argument("--slots-per-mode", type=int, default=2)
    args = ap.parse_args()

    schema = json.load(open(os.path.join(HERE, "schema.json")))
    emails = [json.loads(l) for l in open(os.path.join(HERE, "inputs.jsonl")) if l.strip()]
    if args.limit:
        emails = emails[:args.limit]

    # Interleave: email by email, every condition and seed, so background load hits all conditions alike.
    queues = {m: queue.Queue() for m in MODES}
    for email in emails:
        for cond, temp, samplers, seeds in CONDITIONS:
            for rep in seeds:
                seed = 42 if cond == "t0" else zlib.crc32(f"{email['id']}|{cond}|{rep}".encode())
                for m in MODES:
                    queues[m].put((email, cond, temp, samplers, rep, seed))
    lock = threading.Lock()
    out = open(args.out, "a")

    def worker(mode, slot):
        while True:
            try:
                email, cond, temp, samplers, rep, seed = queues[mode].get_nowait()
            except queue.Empty:
                return
            body, wall = request(args.base, mode, slot, schema, email["text"], temp, samplers, seed,
                                 args.max_tokens)
            row = {"model": args.model, "id": email["id"], "mode": mode, "cond": cond, "temp": temp,
                   "rep": rep, "seed": seed, "slot": slot, "wall_s": round(wall, 3)}
            if "error" in body:
                row["error"] = body["error"]
            else:
                ch = body["choices"][0]
                t = body.get("timings", {})
                row.update({"content": ch["message"].get("content") or "",
                            "finish_reason": ch.get("finish_reason"),
                            "prompt_n": t.get("prompt_n"), "cache_n": t.get("cache_n"),
                            "gen_n": t.get("predicted_n"), "gen_ms": t.get("predicted_ms")})
            with lock:
                out.write(json.dumps(row, ensure_ascii=False) + "\n")
                out.flush()
                shown = (row.get("content") or row.get("error", ""))[:70].replace("\n", " ")
                print(f"{args.model} {email['id']} {cond:12s} r{rep} {mode:8s} {wall:6.1f}s {shown!r}", flush=True)

    threads = []
    slot = 0
    for m in MODES:
        for _ in range(args.slots_per_mode):
            threads.append(threading.Thread(target=worker, args=(m, slot)))
            slot += 1
    for th in threads:
        th.start()
    for th in threads:
        th.join()
    out.close()


if __name__ == "__main__":
    main()
