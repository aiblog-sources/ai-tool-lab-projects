#!/usr/bin/env python3
"""Ollama vs llama.cpp (llama-server) on CPU: same GGUF, same prompt, interleaved runs.

Talks to two servers that are already running:
  - Ollama's native API      POST /api/generate  (raw mode, no chat template)
  - llama.cpp's llama-server POST /completion

Both report their own prompt and generation timings; the client also measures wall time.
Standard library only. Writes one JSON line per run to the --out file.
"""
import argparse
import json
import os
import statistics
import time
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))


def post(url, payload, timeout=900):
    req = urllib.request.Request(
        url, data=json.dumps(payload).encode(), headers={"Content-Type": "application/json"}
    )
    t0 = time.perf_counter()
    with urllib.request.urlopen(req, timeout=timeout) as r:
        body = json.loads(r.read())
    return body, time.perf_counter() - t0


def run_ollama(base, model, prompt, n_predict, threads):
    options = {"num_predict": n_predict, "temperature": 0, "seed": 42}
    if threads:
        options["num_thread"] = threads
    body, wall = post(
        f"{base}/api/generate",
        {"model": model, "prompt": prompt, "raw": True, "stream": False,
         "keep_alive": -1, "options": options},
    )
    return {
        "prompt_tokens": body["prompt_eval_count"],
        "prompt_ms": body["prompt_eval_duration"] / 1e6,
        "gen_tokens": body["eval_count"],
        "gen_ms": body["eval_duration"] / 1e6,
        "wall_s": wall,
    }


def run_llamacpp(base, prompt, n_predict):
    body, wall = post(
        f"{base}/completion",
        {"prompt": prompt, "n_predict": n_predict, "temperature": 0, "seed": 42,
         "cache_prompt": False, "ignore_eos": False},
    )
    t = body["timings"]
    return {
        "prompt_tokens": t["prompt_n"],
        "prompt_ms": t["prompt_ms"],
        "gen_tokens": t["predicted_n"],
        "gen_ms": t["predicted_ms"],
        "wall_s": wall,
    }


def rates(r):
    r["prompt_tps"] = r["prompt_tokens"] / (r["prompt_ms"] / 1000)
    r["gen_tps"] = r["gen_tokens"] / (r["gen_ms"] / 1000)
    return r


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--label", required=True, help="config name written into every row")
    ap.add_argument("--model-name", required=True, help="model label for the results")
    ap.add_argument("--ollama", default="http://127.0.0.1:11500")
    ap.add_argument("--ollama-model", required=True)
    ap.add_argument("--ollama-threads", type=int, default=0, help="0 = Ollama default")
    ap.add_argument("--llamacpp", default="http://127.0.0.1:11600")
    ap.add_argument("--runs", type=int, default=5)
    ap.add_argument("--warmup", type=int, default=1)
    ap.add_argument("--n-predict", type=int, default=128)
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    with open(os.path.join(HERE, "prompt.txt")) as f:
        base_prompt = f.read().strip()

    rows = []
    for i in range(args.warmup + args.runs):
        # A different first line on every run defeats prefix caching in both servers.
        prompt = f"Run {i}.\n\n{base_prompt}"
        # Alternate which engine goes first so background load hits both equally.
        order = ["ollama", "llama.cpp"] if i % 2 == 0 else ["llama.cpp", "ollama"]
        for engine in order:
            if engine == "ollama":
                r = run_ollama(args.ollama, args.ollama_model, prompt, args.n_predict, args.ollama_threads)
            else:
                r = run_llamacpp(args.llamacpp, prompt, args.n_predict)
            r = rates(r)
            r.update(engine=engine, run=i, warmup=i < args.warmup, label=args.label,
                     model=args.model_name, loadavg=os.getloadavg()[0])
            rows.append(r)
            print(f"{args.label} {args.model_name} run {i} {engine:9s} "
                  f"pp {r['prompt_tps']:6.1f} t/s  tg {r['gen_tps']:5.2f} t/s  "
                  f"({r['prompt_tokens']} + {r['gen_tokens']} tok, wall {r['wall_s']:.1f} s)",
                  flush=True)

    with open(args.out, "a") as f:
        for r in rows:
            f.write(json.dumps(r) + "\n")

    for engine in ("ollama", "llama.cpp"):
        m = [r for r in rows if r["engine"] == engine and not r["warmup"]]
        print(f"median {engine:9s} pp {statistics.median(x['prompt_tps'] for x in m):6.1f} t/s  "
              f"tg {statistics.median(x['gen_tps'] for x in m):5.2f} t/s")


if __name__ == "__main__":
    main()
