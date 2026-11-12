#!/usr/bin/env python3
"""Sends 20 support emails to one running Ollama server through its OpenAI-compatible route
(/v1/chat/completions) and asks for a JSON ticket, with and without response_format. Standard library only.
Appends one JSON line per request to --out.

Conditions (the client sends as few fields as each one allows, like an OpenAI SDK user who sets nothing):
  t0            temperature 0, no response_format (greedy control, one run per email)
  t0-schema     temperature 0, response_format json_schema
  none          no sampler fields, no response_format
  schema        no sampler fields, response_format json_schema (the shape the OpenAI SDK's parse() sends)
  object        no sampler fields, response_format json_object
  schema-only   no sampler fields, json_schema, and the system prompt does NOT include the schema
  1.5           temperature 1.5 only
  1.5-schema    temperature 1.5 + json_schema

--conds picks a subset. Every sampled condition runs --reps times per email, each with its own seed (CRC32 of
email id, condition and repeat; the same seeds on every Ollama version). Thinking is off (reasoning_effort none).

--probe sends one short request per condition, one at a time, and copies the sampler chain and parameters that
Ollama's bundled llama-server logs for it to results/effective-params.txt.
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
# name, sampler fields, response_format kind, schema in prompt, sampled
CONDITIONS = [
    ("t0", {"temperature": 0}, None, True, False),
    ("t0-schema", {"temperature": 0}, "json_schema", True, False),
    ("none", {}, None, True, True),
    ("schema", {}, "json_schema", True, True),
    ("object", {}, "json_object", True, True),
    ("schema-only", {}, "json_schema", False, True),
    ("1.5", {"temperature": 1.5}, None, True, True),
    ("1.5-schema", {"temperature": 1.5}, "json_schema", True, True),
]


def system_prompt(schema, with_schema):
    if with_schema:
        return ("You turn customer support emails into JSON tickets. Reply with only a JSON object that matches "
                "this JSON Schema, with no other text:\n\n" + json.dumps(schema, indent=2))
    return ("You turn customer support emails into JSON tickets: customer name, order id, category, priority, "
            "the products mentioned with quantities, and whether a refund is requested. Reply with only a JSON object.")


def response_format(kind, schema):
    if kind == "json_schema":
        # What openai-python's client.chat.completions.parse(response_format=<pydantic model>) puts on the wire
        # (see probe_formats.py): name, schema and strict inside a json_schema object.
        return {"type": "json_schema", "json_schema": {"name": "ticket", "schema": schema, "strict": True}}
    if kind == "json_object":
        return {"type": "json_object"}
    return None


def build(model, messages, samplers, fmt, seed, max_tokens):
    p = {"model": model, "messages": messages, "max_tokens": max_tokens, "seed": seed,
         "reasoning_effort": "none", **samplers}
    if fmt:
        p["response_format"] = fmt
    return "/v1/chat/completions", p


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


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--base", required=True)
    ap.add_argument("--model", required=True, help="name written to the results")
    ap.add_argument("--served-model", required=True, help="model name Ollama serves")
    ap.add_argument("--version", required=True, help="Ollama version, written to the results")
    ap.add_argument("--conds", default=",".join(c[0] for c in CONDITIONS))
    ap.add_argument("--out", default="")
    ap.add_argument("--reps", type=int, default=2)
    ap.add_argument("--workers", type=int, default=2)
    ap.add_argument("--max-tokens", type=int, default=300)
    ap.add_argument("--limit", type=int, default=0)
    ap.add_argument("--resume", action="store_true",
                    help="skip requests already in --out for this version and model (after an interrupted run)")
    ap.add_argument("--probe", action="store_true")
    ap.add_argument("--log", default="", help="server log to read in --probe mode")
    args = ap.parse_args()

    schema = json.load(open(os.path.join(HERE, "schema.json")))
    emails = [json.loads(l) for l in open(os.path.join(HERE, "inputs.jsonl")) if l.strip()]
    if args.limit:
        emails = emails[:args.limit]
    wanted = args.conds.split(",")
    conds = [c for c in CONDITIONS if c[0] in wanted]

    if args.probe:
        keep = ("sampler chain", "repeat_last_n", "top_k =")
        rec = open(os.path.join(HERE, "results", "effective-params.txt"), "a")
        for name, samplers, kind, _, _ in conds:
            msgs = [{"role": "user", "content": "Reply with an empty JSON object."}]
            url, p = build(args.served_model, msgs, samplers, response_format(kind, {"type": "object"}), 1, 8)
            start = os.path.getsize(args.log)
            post(args.base, url, p)
            time.sleep(1)
            with open(args.log, errors="replace") as f:
                f.seek(start)
                lines = [l.strip() for l in f if any(k in l for k in keep)]
            sent = json.dumps(samplers) if samplers else "nothing"
            rec.write(f"## Ollama {args.version} | {args.model} | {name} | sent {sent}\n" + "\n".join(
                l.split("| ", 2)[-1] if "sampler chain" in l else l for l in lines[-3:]) + "\n\n")
            print(name, lines[-1] if lines else "NO LOG LINES", flush=True)
        return

    # Interleave email by email so background load hits every condition alike.
    done = set()
    if args.resume and os.path.exists(args.out):
        for line in open(args.out):
            r = json.loads(line)
            if r["version"] == args.version and r["model"] == args.model and "error" not in r:
                done.add((r["id"], r["cond"], r["rep"]))
    q = queue.Queue()
    for email in emails:
        for name, samplers, kind, in_prompt, sampled in conds:
            for rep in (range(1, args.reps + 1) if sampled else [0]):
                seed = zlib.crc32(f"{email['id']}|{name}|{rep}".encode()) if sampled else 42
                if (email["id"], name, rep) not in done:
                    q.put((email, name, samplers, kind, in_prompt, rep, seed))
    lock = threading.Lock()
    out = open(args.out, "a")

    def worker():
        while True:
            try:
                email, name, samplers, kind, in_prompt, rep, seed = q.get_nowait()
            except queue.Empty:
                return
            msgs = [{"role": "system", "content": system_prompt(schema, in_prompt)},
                    {"role": "user", "content": email["text"]}]
            url, p = build(args.served_model, msgs, samplers, response_format(kind, schema), seed, args.max_tokens)
            body, wall = post(args.base, url, p)
            row = {"version": args.version, "model": args.model, "id": email["id"], "mode": "prompt", "cond": name,
                   "sent": samplers, "format": kind, "schema_in_prompt": in_prompt, "rep": rep, "seed": seed,
                   "wall_s": round(wall, 3)}
            if "error" in body:
                row["error"] = body["error"]
            else:
                ch = body["choices"][0]
                row.update({"content": ch["message"].get("content") or "", "finish_reason": ch.get("finish_reason"),
                            "gen_n": body.get("usage", {}).get("completion_tokens"),
                            "prompt_n": body.get("usage", {}).get("prompt_tokens")})
            with lock:
                out.write(json.dumps(row, ensure_ascii=False) + "\n")
                out.flush()
                shown = (row.get("content") or row.get("error", ""))[:60].replace("\n", " ")
                print(f"{args.version} {args.model} {email['id']} {name:11s} r{rep} {wall:6.1f}s {shown!r}", flush=True)

    threads = [threading.Thread(target=worker) for _ in range(args.workers)]
    for th in threads:
        th.start()
    for th in threads:
        th.join()
    out.close()


if __name__ == "__main__":
    main()
