#!/usr/bin/env python3
"""Sends 20 support emails to one running server (llama-server or Ollama) and asks for a JSON ticket, using that
server's own sampler defaults. Standard library only. Appends one JSON line per request to --out.

The point is what a client gets when it does NOT set samplers, or sets only temperature, so every condition sends
as few sampler fields as possible:

  llama-server, OpenAI route (/v1/chat/completions)
    ls-t0           temperature 0 (greedy baseline, one run)
    ls-default      no sampler fields
    ls-1.5          temperature 1.5 only
  Ollama, native route (/api/chat)
    ol-t0           options.temperature 0 (greedy baseline, one run)
    ol-api-default  no options
    ol-api-1.5      options.temperature 1.5 only
    ol-api-1.5-fix  options.temperature 1.5 + top_p 0.95 + min_p 0.05 (llama-server's values)
  Ollama, OpenAI route (/v1/chat/completions)
    ol-v1-default   no sampler fields (what the OpenAI SDK sends when you set nothing)
    ol-v1-1.5       temperature 1.5 only

Every sampled condition runs --reps times per email, each with its own seed (CRC32 of email id, condition and
repeat). Thinking is off on every route. The JSON Schema is in the system prompt; nothing constrains the output.
--probe sends one short request per condition, one at a time, and copies the sampler chain and parameters the
server logs for it (llama-server with --log-verbosity 4; Ollama passes that flag to its own llama-server) to
results/effective-params.txt.
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
# name, server, route, sampler fields, sampled?
CONDITIONS = [
    ("ls-t0", "llama", "v1", {"temperature": 0}, False),
    ("ls-default", "llama", "v1", {}, True),
    ("ls-1.5", "llama", "v1", {"temperature": 1.5}, True),
    ("ol-t0", "ollama", "api", {"temperature": 0}, False),
    ("ol-api-default", "ollama", "api", {}, True),
    ("ol-api-1.5", "ollama", "api", {"temperature": 1.5}, True),
    ("ol-api-1.5-fix", "ollama", "api", {"temperature": 1.5, "top_p": 0.95, "min_p": 0.05}, True),
    ("ol-v1-default", "ollama", "v1", {}, True),
    ("ol-v1-1.5", "ollama", "v1", {"temperature": 1.5}, True),
]


def system_prompt(schema):
    return ("You turn customer support emails into JSON tickets. Reply with only a JSON object that matches "
            "this JSON Schema, with no other text:\n\n" + json.dumps(schema, indent=2))


def build(server, route, model, messages, samplers, seed, max_tokens, slot):
    if route == "v1":
        url = "/v1/chat/completions"
        p = {"model": model, "messages": messages, "max_tokens": max_tokens, "seed": seed, **samplers}
        if server == "llama":
            p.update({"id_slot": slot, "cache_prompt": True, "chat_template_kwargs": {"enable_thinking": False}})
        else:
            p["reasoning_effort"] = "none"
    else:
        url = "/api/chat"
        p = {"model": model, "messages": messages, "stream": False, "think": False, "keep_alive": -1,
             "options": {"num_predict": max_tokens, "seed": seed, **samplers}}
    return url, p


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
        t = body.get("timings", {})
        u = body.get("usage", {})
        return {"content": ch["message"].get("content") or "", "finish_reason": ch.get("finish_reason"),
                "gen_n": t.get("predicted_n", u.get("completion_tokens"))}
    m = body["message"]
    if m.get("thinking"):
        print("warning: thinking text present", flush=True)
    return {"content": m.get("content") or "", "finish_reason": "length" if body.get("done_reason") == "length"
            else body.get("done_reason"), "gen_n": body.get("eval_count")}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--server", choices=["llama", "ollama"], required=True)
    ap.add_argument("--base", required=True)
    ap.add_argument("--model", required=True, help="name written to the results")
    ap.add_argument("--served-model", default="", help="model name the server expects (Ollama)")
    ap.add_argument("--out", default="")
    ap.add_argument("--reps", type=int, default=2)
    ap.add_argument("--workers", type=int, default=2)
    ap.add_argument("--max-tokens", type=int, default=300)
    ap.add_argument("--limit", type=int, default=0)
    ap.add_argument("--probe", action="store_true")
    ap.add_argument("--log", default="", help="server log to read in --probe mode")
    args = ap.parse_args()

    schema = json.load(open(os.path.join(HERE, "schema.json")))
    emails = [json.loads(l) for l in open(os.path.join(HERE, "inputs.jsonl")) if l.strip()]
    if args.limit:
        emails = emails[:args.limit]
    conds = [c for c in CONDITIONS if c[1] == args.server]

    if args.probe:
        keep = ("sampler chain", "repeat_last_n", "top_k =")
        rec = open(os.path.join(HERE, "results", "effective-params.txt"), "a")
        for name, server, route, samplers, _ in conds:
            msgs = [{"role": "user", "content": "Reply with an empty JSON object."}]
            url, p = build(server, route, args.served_model, msgs, samplers, 1, 8, 0)
            start = os.path.getsize(args.log)
            post(args.base, url, p)
            time.sleep(1)
            with open(args.log, errors="replace") as f:
                f.seek(start)
                lines = [l.strip() for l in f if any(k in l for k in keep)]
            sent = json.dumps(samplers) if samplers else "nothing"
            rec.write(f"## {args.model} | {name} | {route} | sent {sent}\n" + "\n".join(
                l.split("| ", 2)[-1] if "sampler chain" in l else l for l in lines[-3:]) + "\n\n")
            print(name, lines[-1] if lines else "NO LOG LINES", flush=True)
        return

    # Interleave email by email so background load hits every condition alike.
    q = queue.Queue()
    for email in emails:
        for name, server, route, samplers, sampled in conds:
            for rep in (range(1, args.reps + 1) if sampled else [0]):
                seed = zlib.crc32(f"{email['id']}|{name}|{rep}".encode()) if sampled else 42
                q.put((email, name, route, samplers, rep, seed))
    lock = threading.Lock()
    out = open(args.out, "a")

    def worker(slot):
        while True:
            try:
                email, name, route, samplers, rep, seed = q.get_nowait()
            except queue.Empty:
                return
            msgs = [{"role": "system", "content": system_prompt(schema)}, {"role": "user", "content": email["text"]}]
            url, p = build(args.server, route, args.served_model, msgs, samplers, seed, args.max_tokens, slot)
            body, wall = post(args.base, url, p)
            row = {"model": args.model, "id": email["id"], "mode": "prompt", "cond": name, "server": args.server,
                   "route": route, "sent": samplers, "rep": rep, "seed": seed, "wall_s": round(wall, 3)}
            if "error" in body:
                row["error"] = body["error"]
            else:
                row.update(unpack(route, body))
            with lock:
                out.write(json.dumps(row, ensure_ascii=False) + "\n")
                out.flush()
                shown = (row.get("content") or row.get("error", ""))[:60].replace("\n", " ")
                print(f"{args.model} {email['id']} {name:15s} r{rep} {wall:6.1f}s {shown!r}", flush=True)

    threads = [threading.Thread(target=worker, args=(s,)) for s in range(args.workers)]
    for th in threads:
        th.start()
    for th in threads:
        th.join()
    out.close()


if __name__ == "__main__":
    main()
