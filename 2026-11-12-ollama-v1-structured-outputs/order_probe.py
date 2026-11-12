#!/usr/bin/env python3
"""Does a JSON schema keep its property order when Ollama turns it into a grammar?

Sends a schema whose properties are declared as zulu, alpha, mike (not alphabetical) at temperature 0 and records
the key order of the reply. Each route is asked to fill the same object. Standard library only.
  order_probe.py --ollama http://127.0.0.1:11500 --served-model qwen3508b --version 0.40.16 \
                 [--llama-server http://127.0.0.1:11711]
Appends to results/order-probe.jsonl and prints one line per route.
"""
import argparse
import json
import os
import re
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
SCHEMA = {"type": "object",
          "properties": {"zulu": {"type": "string"}, "alpha": {"type": "string"}, "mike": {"type": "string"}},
          "required": ["zulu", "alpha", "mike"], "additionalProperties": False}
PROMPT = "Fill in a JSON object with three short city names."


def post(url, payload):
    req = urllib.request.Request(url, data=json.dumps(payload).encode(), headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=300) as r:
        return json.loads(r.read())


def order(text):
    return re.findall(r'"(zulu|alpha|mike)"\s*:', text)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ollama", default="")
    ap.add_argument("--served-model", default="")
    ap.add_argument("--version", default="")
    ap.add_argument("--llama-server", default="")
    a = ap.parse_args()
    msgs = [{"role": "user", "content": PROMPT}]
    res = []
    if a.ollama:
        b = post(a.ollama + "/v1/chat/completions", {
            "model": a.served_model, "messages": msgs, "temperature": 0, "max_tokens": 60, "reasoning_effort": "none",
            "response_format": {"type": "json_schema", "json_schema": {"name": "cities", "schema": SCHEMA}}})
        res.append((f"Ollama {a.version} /v1 response_format", b["choices"][0]["message"]["content"]))
        b = post(a.ollama + "/api/chat", {"model": a.served_model, "messages": msgs, "stream": False, "think": False,
                                          "format": SCHEMA, "options": {"temperature": 0, "num_predict": 60}})
        res.append((f"Ollama {a.version} /api/chat format", b["message"]["content"]))
    if a.llama_server:
        b = post(a.llama_server + "/v1/chat/completions", {
            "messages": msgs, "temperature": 0, "max_tokens": 60,
            "chat_template_kwargs": {"enable_thinking": False},
            "response_format": {"type": "json_schema", "json_schema": {"name": "cities", "schema": SCHEMA}}})
        res.append(("upstream llama-server /v1 response_format", b["choices"][0]["message"]["content"]))
    with open(os.path.join(HERE, "results", "order-probe.jsonl"), "a") as f:
        for route, text in res:
            f.write(json.dumps({"route": route, "declared": list(SCHEMA["properties"]), "order": order(text),
                                "content": text}) + "\n")
            print(f"{route}: {order(text)}  {' '.join(text.split())[:80]}")


if __name__ == "__main__":
    main()
