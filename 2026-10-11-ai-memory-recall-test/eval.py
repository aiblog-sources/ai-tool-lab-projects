#!/usr/bin/env python3
"""Recall test for an MCP memory server: does the right note come back for a plain question?

Talks MCP over Streamable HTTP (JSON-RPC), calls the server's search tool once per question and language,
and records where the expected page ranks. Written for ai-memory (tool `memory_query`), but the request shape
is plain MCP. Configuration through environment variables, no defaults that point at a real machine:

    MCP_URL=http://127.0.0.1:<port>/mcp MEMORY_WORKSPACE=default MEMORY_PROJECT=my-project python3 eval.py
"""
import json
import os
import sys
import time
import urllib.request

URL = os.environ.get("MCP_URL") or sys.exit("Set MCP_URL to the memory server's MCP endpoint")
WORKSPACE = os.environ.get("MEMORY_WORKSPACE") or sys.exit("Set MEMORY_WORKSPACE")
PROJECT = os.environ.get("MEMORY_PROJECT") or sys.exit("Set MEMORY_PROJECT")
TOOL = os.environ.get("MEMORY_TOOL", "memory_query")
K = 5
session = None
next_id = 0


def rpc(method, params=None, notify=False):
    """One JSON-RPC call; handles both JSON and SSE responses."""
    global session, next_id
    msg = {"jsonrpc": "2.0", "method": method, "params": params or {}}
    if not notify:
        next_id += 1
        msg["id"] = next_id
    headers = {"Content-Type": "application/json", "Accept": "application/json, text/event-stream"}
    if session:
        headers["Mcp-Session-Id"] = session
    req = urllib.request.Request(URL, data=json.dumps(msg).encode(), headers=headers)
    with urllib.request.urlopen(req, timeout=60) as r:
        session = r.headers.get("Mcp-Session-Id") or session
        raw = r.read().decode()
    if notify or not raw.strip():
        return None
    if raw.lstrip().startswith("{"):
        return json.loads(raw)
    events = [line[5:].strip() for line in raw.splitlines() if line.startswith("data:")]
    return json.loads(events[-1])


def search(query):
    res = rpc("tools/call", {"name": TOOL, "arguments": {
        "query": query, "workspace": WORKSPACE, "project": PROJECT, "limit": K}})
    text = res["result"]["content"][0]["text"]
    return [h["path"] for h in json.loads(text).get("hits", [])]


def main():
    rpc("initialize", {"protocolVersion": "2025-06-18", "capabilities": {},
                       "clientInfo": {"name": "recall-eval", "version": "1"}})
    rpc("notifications/initialized", notify=True)
    questions = json.load(open(os.path.join(os.path.dirname(__file__), "questions.json")))
    rows = []
    for q in questions:
        for lang in ("en", "pt"):
            t0 = time.perf_counter()
            paths = search(q[lang])
            ms = (time.perf_counter() - t0) * 1000
            rank = paths.index(q["expected"]) + 1 if q["expected"] in paths else None
            rows.append({"lang": lang, "question": q[lang], "expected": q["expected"],
                         "rank": rank, "returned": len(paths), "ms": round(ms, 1)})
    json.dump(rows, open(os.path.join(os.path.dirname(__file__), "results.json"), "w"), indent=2, ensure_ascii=False)

    print(f"| Language | Questions | Hit@1 | Hit@3 | Hit@{K} | MRR | Median latency |")
    print("| --- | ---: | ---: | ---: | ---: | ---: | ---: |")
    for lang in ("en", "pt"):
        rs = [r for r in rows if r["lang"] == lang]
        hit = lambda k: sum(1 for r in rs if r["rank"] and r["rank"] <= k)
        mrr = sum(1 / r["rank"] for r in rs if r["rank"]) / len(rs)
        lat = sorted(r["ms"] for r in rs)[len(rs) // 2]
        print(f"| {lang} | {len(rs)} | {hit(1)} | {hit(3)} | {hit(K)} | {mrr:.2f} | {lat:.0f} ms |")
    print()
    for r in rows:
        mark = r["rank"] if r["rank"] else "miss"
        print(f"{r['lang']}  rank={mark!s:>4}  returned={r['returned']}  {r['question']}")


if __name__ == "__main__":
    main()
