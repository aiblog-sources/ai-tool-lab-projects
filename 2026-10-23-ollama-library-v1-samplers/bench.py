#!/usr/bin/env python3
"""Sends 20 support emails to a running Ollama server and asks a library model (pulled with `ollama pull`) for a
JSON ticket. Standard library only. Appends one JSON line per request to --out.

The question: what happens to the sampler settings a library model ships with (its `params` layer, shown by
`ollama show <model> --parameters`) when a client uses the OpenAI-compatible route and sends no sampler fields?

  api-t0       /api/chat, options.temperature 0 (greedy baseline, one run)
  api-default  /api/chat, no options: the model's shipped parameters apply
  v1-default   /v1/chat/completions, no sampler fields (what the OpenAI SDK sends when you set nothing)
  v1-fix       /v1/chat/completions with the shipped temperature and top_p sent in the request

Sampled conditions run --reps times per email (v1-fix runs --fix-reps times), each request with its own seed
(CRC32 of email id, condition and repeat). Thinking is off on both routes. The JSON Schema is in the system prompt;
nothing constrains the output. --probe sends one short request per condition, one at a time, and copies the sampler
parameters the server logs for it to results/effective-params.txt.
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


def conditions(shipped):
    fix = {k: shipped[k] for k in ("temperature", "top_p") if k in shipped}
    # name, route, sampler fields, sampled?
    return [("api-t0", "api", {"temperature": 0}, False),
            ("api-default", "api", {}, True),
            ("v1-default", "v1", {}, True),
            ("v1-fix", "v1", fix, True)]


def system_prompt(schema):
    return ("You turn customer support emails into JSON tickets. Reply with only a JSON object that matches "
            "this JSON Schema, with no other text:\n\n" + json.dumps(schema, indent=2))


def build(route, model, messages, samplers, seed, max_tokens):
    if route == "v1":
        return "/v1/chat/completions", {"model": model, "messages": messages, "max_tokens": max_tokens,
                                        "seed": seed, "reasoning_effort": "none", **samplers}
    return "/api/chat", {"model": model, "messages": messages, "stream": False, "think": False, "keep_alive": -1,
                         "options": {"num_predict": max_tokens, "seed": seed, **samplers}}


def post(base, url, payload):
    req = urllib.request.Request(base + url, data=json.dumps(payload).encode(),
                                 headers={"Content-Type": "application/json"})
    t0 = time.perf_counter()
    try:
        with urllib.request.urlopen(req, timeout=900) as r:
            body = json.loads(r.read())
    except urllib.error.HTTPError as e:
        return {"error": f"HTTP {e.code}: {e.read().decode()[:500]}"}, time.perf_counter() - t0
    return body, time.perf_counter() - t0


def unpack(route, body):
    if route == "v1":
        ch = body["choices"][0]
        return {"content": ch["message"].get("content") or "", "thinking": ch["message"].get("reasoning") or "",
                "finish_reason": ch.get("finish_reason"), "gen_n": body.get("usage", {}).get("completion_tokens")}
    m = body["message"]
    return {"content": m.get("content") or "", "thinking": m.get("thinking") or "",
            "finish_reason": "length" if body.get("done_reason") == "length" else body.get("done_reason"),
            "gen_n": body.get("eval_count")}


def shipped_params(base, model):
    body, _ = post(base, "/api/show", {"model": model})
    out = {}
    for line in (body.get("parameters") or "").splitlines():
        parts = line.split(None, 1)
        if len(parts) == 2 and parts[0] != "stop":
            try:
                out[parts[0]] = float(parts[1])
            except ValueError:
                out[parts[0]] = parts[1]
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--base", required=True)
    ap.add_argument("--model", required=True, help="Ollama model name, for example qwen3:1.7b")
    ap.add_argument("--out", default="")
    ap.add_argument("--reps", type=int, default=2)
    ap.add_argument("--fix-reps", type=int, default=1)
    ap.add_argument("--workers", type=int, default=2)
    ap.add_argument("--max-tokens", type=int, default=300)
    ap.add_argument("--limit", type=int, default=0)
    ap.add_argument("--conds", default="api-t0,api-default,v1-default,v1-fix",
                    help="comma-separated conditions to run (--probe always records all four)")
    ap.add_argument("--probe", action="store_true")
    ap.add_argument("--log", default="", help="server log to read in --probe mode")
    args = ap.parse_args()

    schema = json.load(open(os.path.join(HERE, "schema.json")))
    emails = [json.loads(l) for l in open(os.path.join(HERE, "inputs.jsonl")) if l.strip()]
    if args.limit:
        emails = emails[:args.limit]
    shipped = shipped_params(args.base, args.model)
    conds = conditions(shipped)
    print(args.model, "ships", shipped, flush=True)

    if args.probe:
        keep = ("sampler chain", "repeat_last_n", "top_k =", "sampler params")
        rec = open(os.path.join(HERE, "results", "effective-params.txt"), "a")
        rec.write(f"# {args.model} ships {json.dumps(shipped)}\n\n")
        for name, route, samplers, _ in conds:
            msgs = [{"role": "user", "content": "Reply with an empty JSON object."}]
            url, p = build(route, args.model, msgs, samplers, 1, 8)
            start = os.path.getsize(args.log)
            post(args.base, url, p)
            time.sleep(1)
            with open(args.log, errors="replace") as f:
                f.seek(start)
                lines = [l.strip() for l in f if any(k in l for k in keep)]
            sent = json.dumps(samplers) if samplers else "nothing"
            rec.write(f"## {args.model} | {name} | {route} | sent {sent}\n" + "\n".join(lines[-3:]) + "\n\n")
            print(name, lines[-1] if lines else "NO LOG LINES", flush=True)
        return

    conds = [c for c in conds if c[0] in args.conds.split(",")]
    # Interleave email by email so background load hits every condition alike.
    q = queue.Queue()
    for email in emails:
        for name, route, samplers, sampled in conds:
            reps = (args.fix_reps if name == "v1-fix" else args.reps) if sampled else 1
            for rep in range(1, reps + 1):
                seed = zlib.crc32(f"{email['id']}|{name}|{rep}".encode()) if sampled else 42
                q.put((email, name, route, samplers, rep if sampled else 0, seed))
    lock = threading.Lock()
    out = open(args.out, "a")

    def worker():
        while True:
            try:
                email, name, route, samplers, rep, seed = q.get_nowait()
            except queue.Empty:
                return
            msgs = [{"role": "system", "content": system_prompt(schema)}, {"role": "user", "content": email["text"]}]
            url, p = build(route, args.model, msgs, samplers, seed, args.max_tokens)
            body, wall = post(args.base, url, p)
            row = {"model": args.model, "id": email["id"], "mode": "prompt", "cond": name, "route": route,
                   "sent": samplers, "shipped": shipped, "rep": rep, "seed": seed, "wall_s": round(wall, 3)}
            if "error" in body:
                row["error"] = body["error"]
            else:
                row.update(unpack(route, body))
            with lock:
                out.write(json.dumps(row, ensure_ascii=False) + "\n")
                out.flush()
                shown = (row.get("content") or row.get("error", ""))[:60].replace("\n", " ")
                print(f"{args.model} {email['id']} {name:12s} r{rep} {wall:6.1f}s {shown!r}", flush=True)

    threads = [threading.Thread(target=worker) for _ in range(args.workers)]
    for th in threads:
        th.start()
    for th in threads:
        th.join()
    out.close()


if __name__ == "__main__":
    main()
