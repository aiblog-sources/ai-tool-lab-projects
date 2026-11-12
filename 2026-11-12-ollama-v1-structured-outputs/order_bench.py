#!/usr/bin/env python3
"""Does the order of the keys a grammar forces change the answers? Standard library only.

Ollama (0.40.13 and 0.40.16) re-encodes a response_format schema through a Go map before llama-server builds the
grammar, so the properties come out sorted alphabetically (ollama/ollama#18717): the ticket starts with
"category" instead of "customer_name". Upstream llama-server keeps the declared order. This sends the same 20
emails to upstream llama-server twice per repeat, with the same seed and samplers: once with schema.json as
declared, once with its properties sorted alphabetically (what Ollama sends). The only difference between the two
requests is the key order.

Samplers: Ollama 0.40.16's /v1 defaults for a model without PARAMETER lines (temperature 0.8, top_k 40, top_p 0.9,
min_p 0). The system prompt shows schema.json as declared in both conditions, as in bench.py.
  order_bench.py --base http://127.0.0.1:11711 --model "Qwen 3.5 0.8B" --out results/raw-order.jsonl
"""
import argparse
import json
import os
import queue
import threading
import zlib

from bench import post, system_prompt

HERE = os.path.dirname(os.path.abspath(__file__))
SAMPLERS = {"temperature": 0.8, "top_k": 40, "top_p": 0.9, "min_p": 0}


def alphabetical(schema):
    s = dict(schema)
    if "properties" in s:
        s["properties"] = {k: alphabetical(v) for k, v in sorted(s["properties"].items())}
    if isinstance(s.get("items"), dict):
        s["items"] = alphabetical(s["items"])
    return s


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--base", required=True)
    ap.add_argument("--model", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--reps", type=int, default=2)
    ap.add_argument("--limit", type=int, default=0)
    args = ap.parse_args()
    schema = json.load(open(os.path.join(HERE, "schema.json")))
    variants = {"ls-declared": schema, "ls-alpha": alphabetical(schema)}
    emails = [json.loads(l) for l in open(os.path.join(HERE, "inputs.jsonl")) if l.strip()]
    if args.limit:
        emails = emails[:args.limit]
    q = queue.Queue()
    for email in emails:
        for rep in range(1, args.reps + 1):
            for name in variants:
                q.put((email, name, rep, zlib.crc32(f"{email['id']}|ls-schema|{rep}".encode())))
    lock = threading.Lock()
    out = open(args.out, "a")

    def worker(slot):
        while True:
            try:
                email, name, rep, seed = q.get_nowait()
            except queue.Empty:
                return
            p = {"messages": [{"role": "system", "content": system_prompt(schema, True)},
                              {"role": "user", "content": email["text"]}],
                 "max_tokens": 300, "seed": seed, **SAMPLERS, "id_slot": slot, "cache_prompt": True,
                 "chat_template_kwargs": {"enable_thinking": False},
                 "response_format": {"type": "json_schema",
                                     "json_schema": {"name": "ticket", "schema": variants[name], "strict": True}}}
            body, wall = post(args.base, "/v1/chat/completions", p)
            row = {"version": "llama-server", "model": args.model, "id": email["id"], "mode": "prompt", "cond": name,
                   "sent": SAMPLERS, "format": "json_schema", "schema_in_prompt": True, "rep": rep, "seed": seed,
                   "wall_s": round(wall, 3)}
            if "error" in body:
                row["error"] = body["error"]
            else:
                ch = body["choices"][0]
                row.update({"content": ch["message"].get("content") or "", "finish_reason": ch.get("finish_reason"),
                            "gen_n": body.get("usage", {}).get("completion_tokens")})
            with lock:
                out.write(json.dumps(row, ensure_ascii=False) + "\n")
                out.flush()
                print(f"{args.model} {email['id']} {name:11s} r{rep} {wall:6.1f}s "
                      f"{(row.get('content') or row.get('error', ''))[:50]!r}", flush=True)

    threads = [threading.Thread(target=worker, args=(s,)) for s in range(2)]
    for th in threads:
        th.start()
    for th in threads:
        th.join()


if __name__ == "__main__":
    main()
