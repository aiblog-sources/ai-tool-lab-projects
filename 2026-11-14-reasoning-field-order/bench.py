#!/usr/bin/env python3
"""Does a `reasoning` field before `answer` in a JSON schema help a small model, and does Ollama keep that order?

Standard library only. Sends GSM8K test problems to an OpenAI-compatible /v1/chat/completions endpoint
(upstream llama-server or Ollama) under several conditions and writes one JSON line per request.

Conditions (the schema each one sends as response_format json_schema, strict):
  free            no response_format; "solve step by step, end with 'Answer: <number>'"
  answer_only     {"answer": number}
  reasoning_first {"reasoning": string, "answer": number}
  answer_first    {"answer": number, "reasoning": string}
  prefixed        {"a_reasoning": string, "b_answer": number}   (survives alphabetical sorting)

The system prompt shows the schema exactly as declared, so on Ollama the prompt asks for reasoning first even when
the grammar (built from alphabetically sorted keys, ollama/ollama#18717) forces the answer first.
Greedy decoding (temperature 0), fixed seed, thinking off.

  bench.py --base http://127.0.0.1:11711 --server llama-server --model "Qwen 3.5 2B" --served-model x \
           --conds free,answer_only --out results/raw.jsonl --limit 100
"""
import argparse
import json
import os
import queue
import re
import threading
import time
import urllib.error
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
NUM = {"type": "number"}
STR = {"type": "string"}


def obj(*props):
    return {"type": "object", "properties": dict(props), "required": [k for k, _ in props],
            "additionalProperties": False}


SCHEMAS = {
    "answer_only": obj(("answer", NUM)),
    "reasoning_first": obj(("reasoning", STR), ("answer", NUM)),
    "answer_first": obj(("answer", NUM), ("reasoning", STR)),
    "prefixed": obj(("a_reasoning", STR), ("b_answer", NUM)),
}
ANSWER_KEY = {"answer_only": "answer", "reasoning_first": "answer", "answer_first": "answer", "prefixed": "b_answer"}
REASON_KEY = {"reasoning_first": "reasoning", "answer_first": "reasoning", "prefixed": "a_reasoning"}

FREE_PROMPT = ("Solve the math word problem. Think step by step, then end your reply with a final line of the form "
               "'Answer: <number>' (digits only, no units).")
JSON_PROMPT = ("Solve the math word problem. Reply with only a JSON object that matches this JSON schema, "
               "with the keys in this order:\n{schema}\nThe answer is a plain number (no units).")


def load_problems(limit):
    out = []
    for i, line in enumerate(open(os.path.join(HERE, "data", "test.jsonl"))):
        if limit and i >= limit:
            break
        d = json.loads(line)
        gold = float(d["answer"].split("####")[-1].strip().replace(",", ""))
        out.append({"id": i, "question": d["question"], "gold": gold})
    return out


def messages(cond, question):
    if cond == "free":
        system = FREE_PROMPT
    else:
        system = JSON_PROMPT.format(schema=json.dumps(SCHEMAS[cond]))
    return [{"role": "system", "content": system}, {"role": "user", "content": question}]


def payload(model, cond, question, max_tokens, seed):
    p = {"model": model, "messages": messages(cond, question), "max_tokens": max_tokens, "seed": seed,
         "temperature": 0, "reasoning_effort": "none"}
    if cond != "free":
        p["response_format"] = {"type": "json_schema",
                                "json_schema": {"name": cond, "schema": SCHEMAS[cond], "strict": True}}
    return p


def post(base, body):
    req = urllib.request.Request(base + "/v1/chat/completions", data=json.dumps(body).encode(),
                                 headers={"Content-Type": "application/json"})
    t0 = time.perf_counter()
    try:
        with urllib.request.urlopen(req, timeout=1200) as r:
            res = json.loads(r.read())
    except urllib.error.HTTPError as e:
        res = {"error": f"HTTP {e.code}: {e.read().decode()[:500]}"}
    return res, time.perf_counter() - t0


NUMBER = re.compile(r"-?\d[\d,]*(?:\.\d+)?")


def to_num(s):
    try:
        return float(str(s).replace(",", "").replace("$", "").strip())
    except ValueError:
        return None


def grade(cond, text):
    """Return (parsed_ok, predicted, key_order, last_number_in_reasoning)."""
    if cond == "free":
        m = re.findall(r"Answer:\s*\$?\s*(-?[\d,]*\.?\d+)", text)
        return (bool(m), to_num(m[-1]) if m else None, None, None)
    try:
        d = json.loads(text)
    except (json.JSONDecodeError, TypeError):
        return (False, None, None, None)
    if not isinstance(d, dict):
        return (False, None, None, None)
    pred = to_num(d.get(ANSWER_KEY[cond]))
    rkey = REASON_KEY.get(cond)
    last = None
    if rkey and isinstance(d.get(rkey), str):
        nums = NUMBER.findall(d[rkey])
        last = to_num(nums[-1]) if nums else None
    return (pred is not None, pred, list(d.keys()), last)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--base", required=True)
    ap.add_argument("--server", required=True, help="label written to the results, e.g. llama-server-b11554")
    ap.add_argument("--model", required=True, help="model label written to the results")
    ap.add_argument("--served-model", required=True, help="model name the server expects")
    ap.add_argument("--conds", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--limit", type=int, default=100)
    ap.add_argument("--workers", type=int, default=2)
    ap.add_argument("--max-tokens", type=int, default=1024)
    args = ap.parse_args()

    done = set()
    if os.path.exists(args.out):
        for line in open(args.out):
            r = json.loads(line)
            if "error" not in r:
                done.add((r["server"], r["model"], r["cond"], r["id"]))
    q = queue.Queue()
    for p in load_problems(args.limit):  # problem-major order, so a partial run covers every condition evenly
        for cond in args.conds.split(","):
            if (args.server, args.model, cond, p["id"]) not in done:
                q.put((p, cond))
    print(f"{args.server} {args.model}: {q.qsize()} requests", flush=True)
    lock = threading.Lock()
    out = open(args.out, "a")

    def worker():
        while True:
            try:
                p, cond = q.get_nowait()
            except queue.Empty:
                return
            res, dt = post(args.base, payload(args.served_model, cond, p["question"], args.max_tokens, 1234))
            rec = {"server": args.server, "model": args.model, "cond": cond, "id": p["id"], "gold": p["gold"],
                   "seconds": round(dt, 2)}
            if "error" in res:
                rec["error"] = res["error"]
            else:
                ch = res["choices"][0]
                text = ch["message"].get("content") or ""
                ok, pred, keys, last = grade(cond, text)
                rec.update({"text": text, "finish": ch.get("finish_reason"),
                            "completion_tokens": res.get("usage", {}).get("completion_tokens"),
                            "parsed": ok, "pred": pred, "correct": ok and pred is not None
                            and abs(pred - p["gold"]) < 1e-6, "keys": keys, "reasoning_last_number": last})
            with lock:
                out.write(json.dumps(rec) + "\n")
                out.flush()

    threads = [threading.Thread(target=worker) for _ in range(args.workers)]
    for t in threads:
        t.start()
    for t in threads:
        t.join()
    print("done", flush=True)


if __name__ == "__main__":
    main()
