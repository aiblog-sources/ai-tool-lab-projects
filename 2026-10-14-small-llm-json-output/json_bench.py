#!/usr/bin/env python3
"""Asks a running llama-server to turn 20 support emails into JSON tickets, in three modes.

  prompt       the JSON Schema is in the system prompt, nothing constrains the output
  grammar      the same prompt, plus response_format json_schema (llama-server turns it into a grammar)
  grammar-only response_format json_schema, but the prompt does not show the schema

Each mode is pinned to its own server slot (id_slot), so the fixed system prompt stays cached per mode
while the modes are interleaved email by email. Temperature 0, thinking off. Standard library only.
Appends one JSON line per request to --out; grade.py scores them.
"""
import argparse
import json
import os
import time
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
MODES = ["prompt", "grammar", "grammar-only"]


def system_prompt(mode, schema):
    if mode == "grammar-only":
        return "You turn customer support emails into JSON tickets."
    return ("You turn customer support emails into JSON tickets. Reply with only a JSON object that matches "
            "this JSON Schema, with no other text:\n\n" + json.dumps(schema, indent=2))


def request(base, mode, slot, schema, email, max_tokens):
    payload = {
        "messages": [{"role": "system", "content": system_prompt(mode, schema)},
                     {"role": "user", "content": email}],
        "max_tokens": max_tokens, "temperature": 0, "seed": 42, "id_slot": slot, "cache_prompt": True,
        "chat_template_kwargs": {"enable_thinking": False},
    }
    if mode != "prompt":
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
    ap.add_argument("--base", default="http://127.0.0.1:11710")
    ap.add_argument("--model", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--max-tokens", type=int, default=300)
    ap.add_argument("--limit", type=int, default=0, help="only the first N emails (smoke test)")
    args = ap.parse_args()

    with open(os.path.join(HERE, "schema.json")) as f:
        schema = json.load(f)
    with open(os.path.join(HERE, "inputs.jsonl")) as f:
        emails = [json.loads(line) for line in f if line.strip()]
    if args.limit:
        emails = emails[:args.limit]

    with open(args.out, "a") as out:
        for email in emails:
            for slot, mode in enumerate(MODES):
                body, wall = request(args.base, mode, slot, schema, email["text"], args.max_tokens)
                row = {"model": args.model, "id": email["id"], "mode": mode, "wall_s": round(wall, 3)}
                if "error" in body:
                    row["error"] = body["error"]
                else:
                    ch = body["choices"][0]
                    t = body.get("timings", {})
                    row.update({"content": ch["message"].get("content") or "",
                                "reasoning_chars": len(ch["message"].get("reasoning_content") or ""),
                                "finish_reason": ch.get("finish_reason"),
                                "prompt_n": t.get("prompt_n"), "prompt_ms": t.get("prompt_ms"),
                                "cache_n": t.get("cache_n"),
                                "gen_n": t.get("predicted_n"), "gen_ms": t.get("predicted_ms")})
                out.write(json.dumps(row, ensure_ascii=False) + "\n")
                out.flush()
                shown = (row.get("content") or row.get("error", ""))[:90].replace("\n", " ")
                print(f"{args.model} {email['id']} {mode:12s} {wall:6.1f}s {shown!r}", flush=True)


if __name__ == "__main__":
    main()
