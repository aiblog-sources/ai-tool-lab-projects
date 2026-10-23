#!/usr/bin/env python3
"""Asks a safety classifier from the Ollama library (llama-guard3:1b ships `temperature 0`) to label 24 user
prompts, --reps times each, on /api/chat and on /v1/chat/completions, sending no sampler fields either way.
Every request gets its own seed. Appends one JSON line per request to --out. Standard library only.

The question: does the label for the same prompt change from run to run? With the shipped temperature 0 it should
not; /v1 replaces the temperature with 1.0 when the request leaves it out.
"""
import argparse
import json
import os
import zlib

from bench import build, post, unpack

HERE = os.path.dirname(os.path.abspath(__file__))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--base", required=True)
    ap.add_argument("--model", default="llama-guard3:1b")
    ap.add_argument("--reps", type=int, default=20)
    ap.add_argument("--out", required=True)
    args = ap.parse_args()
    prompts = [json.loads(l) for l in open(os.path.join(HERE, "guard-prompts.jsonl")) if l.strip()]
    out = open(args.out, "a")
    for rep in range(1, args.reps + 1):
        for p in prompts:
            for route in ("api", "v1"):
                seed = zlib.crc32(f"{p['id']}|{route}|{rep}".encode())
                url, payload = build(route, args.model, [{"role": "user", "content": p["text"]}], {}, seed, 20)
                body, wall = post(args.base, url, payload)
                row = {"model": args.model, "id": p["id"], "kind": p["kind"], "route": route, "rep": rep,
                       "seed": seed, "wall_s": round(wall, 3)}
                row.update({"error": body["error"]} if "error" in body else unpack(route, body))
                out.write(json.dumps(row) + "\n")
                out.flush()
                print(rep, p["id"], route, repr(row.get("content", row.get("error")))[:40], flush=True)


if __name__ == "__main__":
    main()
