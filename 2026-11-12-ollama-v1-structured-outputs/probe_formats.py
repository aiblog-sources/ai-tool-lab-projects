#!/usr/bin/env python3
"""Which response_format shapes does Ollama's /v1/chat/completions actually enforce?

The probe schema asks for {"animal": "zebra", "code": <integer>} while the prompt asks for a favorite color, so a
reply with animal == "zebra" can only come from the grammar. Temperature 0, one request per shape.

Shape 1 goes through the real OpenAI Python SDK (client.chat.completions.parse with a Pydantic model); an httpx
event hook records the exact JSON body the SDK sent, which bench.py copies. The other shapes are raw HTTP so
that malformed ones reach the server unchanged.

Needs the openai and pydantic packages (run.sh uses vendor/venv). Usage:
  probe_formats.py --base http://127.0.0.1:11500 --served-model qwen3508b --version 0.40.16
Appends to results/format-probe.jsonl and prints a table.
"""
import argparse
import json
import os
import urllib.error
import urllib.request
from typing import Literal

import httpx2 as httpx  # openai-python 3.x ships its HTTP client as httpx2
from openai import OpenAI
from pydantic import BaseModel

HERE = os.path.dirname(os.path.abspath(__file__))
PROMPT = "Reply with a JSON object with one key, color, set to your favorite color."


class Probe(BaseModel):
    animal: Literal["zebra"]
    code: int


SCHEMA = {"type": "object", "properties": {"animal": {"enum": ["zebra"]}, "code": {"type": "integer"}},
          "required": ["animal", "code"], "additionalProperties": False}


def raw(base, path, payload):
    req = urllib.request.Request(base + path, data=json.dumps(payload).encode(),
                                 headers={"Content-Type": "application/json"})
    try:
        with urllib.request.urlopen(req, timeout=300) as r:
            return r.status, json.loads(r.read())
    except urllib.error.HTTPError as e:
        return e.code, {"error": e.read().decode()[:300]}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--base", required=True)
    ap.add_argument("--served-model", required=True)
    ap.add_argument("--version", required=True)
    args = ap.parse_args()
    msgs = [{"role": "user", "content": PROMPT}]
    common = {"model": args.served_model, "messages": msgs, "temperature": 0, "max_tokens": 60,
              "reasoning_effort": "none"}
    rows = []

    # 1. The OpenAI SDK's parse() helper, with the wire body recorded.
    sent = {}

    def hook(request):
        sent["body"] = json.loads(request.content)

    client = OpenAI(base_url=args.base + "/v1", api_key="ollama",
                    http_client=httpx.Client(event_hooks={"request": [hook]}))
    try:
        r = client.chat.completions.parse(model=args.served_model, messages=msgs, response_format=Probe,
                                          temperature=0, max_tokens=60, reasoning_effort="none")
        content, status, parsed = r.choices[0].message.content, 200, r.choices[0].message.parsed
        note = f"parsed={parsed!r}"
    except Exception as e:  # the SDK raises when the reply does not fit the model
        content, status, note = "", "exception", f"{type(e).__name__}: {str(e)[:160]}"
    rows.append(("sdk parse(response_format=Pydantic model)", sent.get("body", {}).get("response_format"), status,
                 content, note))

    shapes = [
        ("json_schema inside json_schema (OpenAI spec)",
         {"type": "json_schema", "json_schema": {"name": "probe", "schema": SCHEMA, "strict": True}}),
        ("json_schema, schema one level up (common mistake)", {"type": "json_schema", "schema": SCHEMA}),
        ("json_schema, json_schema without schema key", {"type": "json_schema", "json_schema": SCHEMA}),
        ("json_object", {"type": "json_object"}),
        ("type: json (not an OpenAI type)", {"type": "json"}),
        ("type: JSON_SCHEMA (upper case)", {"type": "JSON_SCHEMA", "json_schema": {"name": "probe", "schema": SCHEMA}}),
        ("no response_format", None),
    ]
    for label, fmt in shapes:
        p = dict(common)
        if fmt is not None:
            p["response_format"] = fmt
        status, body = raw(args.base, "/v1/chat/completions", p)
        content = body["choices"][0]["message"].get("content", "") if status == 200 else ""
        rows.append((label, fmt, status, content, body.get("error", "") if status != 200 else ""))

    status, body = raw(args.base, "/api/chat", {"model": args.served_model, "messages": msgs, "stream": False,
                                               "think": False, "format": SCHEMA,
                                               "options": {"temperature": 0, "num_predict": 60}})
    rows.append(("native /api/chat format: <schema>", SCHEMA, status, body.get("message", {}).get("content", ""), ""))

    out = open(os.path.join(HERE, "results", "format-probe.jsonl"), "a")
    print(f"\nOllama {args.version}, {args.served_model}")
    print("| Shape | HTTP | Probe schema enforced | Reply |\n| --- | --- | --- | --- |")
    for label, fmt, status, content, note in rows:
        try:
            obj = json.loads(content)
            applied = isinstance(obj, dict) and obj.get("animal") == "zebra"
        except (json.JSONDecodeError, TypeError):
            applied = False
        out.write(json.dumps({"version": args.version, "model": args.served_model, "shape": label, "sent": fmt,
                              "http": status, "content": content, "grammar_applied": applied, "note": note}) + "\n")
        print(f"| {label} | {status} | {'yes' if applied else 'no'} | `{" ".join(content.split())[:70]}` |")


if __name__ == "__main__":
    main()
