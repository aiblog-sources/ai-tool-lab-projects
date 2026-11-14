#!/usr/bin/env python3
"""Which key order does Ollama's grammar force on each API? Standard library only.

Sends the reasoning-first schema ({"reasoning", "answer"}, declared in that order) to a running Ollama on
/v1/chat/completions (response_format json_schema) and /api/chat (format), greedy, and prints the reply's key order.
  probe_order.py --base http://127.0.0.1:11500 --model qwen352b >> results/order-probe.txt
"""
import argparse
import json
import urllib.request

from bench import SCHEMAS

Q = "A shop sells pens at 3 dollars each. Tom buys 4 pens and pays with a 20 dollar bill. How much change does he get?"
SYSTEM = "Solve the math word problem. Reply with only a JSON object with the keys reasoning and answer."


def call(base, path, body):
    req = urllib.request.Request(base + path, data=json.dumps(body).encode(), headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=600) as r:
        return json.loads(r.read())


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--base", required=True)
    ap.add_argument("--model", required=True)
    args = ap.parse_args()
    msgs = [{"role": "system", "content": SYSTEM}, {"role": "user", "content": Q}]
    for cond in ("reasoning_first", "prefixed"):
        schema = SCHEMAS[cond]
        v1 = call(args.base, "/v1/chat/completions", {
            "model": args.model, "messages": msgs, "temperature": 0, "seed": 1234, "max_tokens": 400,
            "reasoning_effort": "none",
            "response_format": {"type": "json_schema", "json_schema": {"name": cond, "schema": schema, "strict": True}}})
        native = call(args.base, "/api/chat", {
            "model": args.model, "messages": msgs, "stream": False, "think": False, "format": schema,
            "options": {"temperature": 0, "seed": 1234, "num_predict": 400}})
        for api, text in (("/v1 response_format", v1["choices"][0]["message"]["content"]),
                          ("/api/chat format", native["message"]["content"])):
            try:
                keys = list(json.loads(text).keys())
            except json.JSONDecodeError:
                keys = "not JSON: " + text[:80]
            print(f"{args.model}\t{cond}\tdeclared {list(schema['properties'])}\t{api}\treply keys {keys}")


if __name__ == "__main__":
    main()
