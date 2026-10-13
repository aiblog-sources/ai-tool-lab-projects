#!/usr/bin/env python3
"""Turns the raw results into results/summary.md (speed, memory, latency, sanity check)."""
import csv
import json
import os
import statistics

HERE = os.path.dirname(os.path.abspath(__file__))
R = os.path.join(HERE, "results")


def load_jsonl(name):
    path = os.path.join(R, name)
    if not os.path.exists(path):
        return []
    with open(path) as f:
        return [json.loads(line) for line in f if line.strip()]


def load_tsv(name):
    path = os.path.join(R, name)
    if not os.path.exists(path):
        return {}
    with open(path) as f:
        return {row["model"]: row for row in csv.DictReader(f, delimiter="\t")}


with open(os.path.join(HERE, "models.tsv")) as f:
    models = list(csv.DictReader(f, delimiter="\t"))
by_file = {m["file"]: m["name"] for m in models}

bench = {}
for b in load_jsonl("llama-bench.jsonl"):
    name = by_file[os.path.basename(b["model_filename"])]
    d = bench.setdefault(name, {"size_gib": b["model_size"] / 2**30, "params_b": b["model_n_params"] / 1e9})
    key = "pp" if b["n_gen"] == 0 else "tg"
    d[key] = (b["avg_ts"], min(b["samples_ts"]), max(b["samples_ts"]))

# Regrade from the stored answers, so a change to the grading rules applies to every run.
import sys
sys.path.insert(0, HERE)
from serve_bench import grade  # noqa: E402

with open(os.path.join(HERE, "questions.jsonl")) as f:
    QUESTIONS = {q["id"]: q for q in map(json.loads, f) if q}
serve = load_jsonl("serve.jsonl") + [r for r in load_jsonl("serve-no-prompt-cache.jsonl") if r["kind"] == "question"]
overrides = {}
if os.path.exists(os.path.join(R, "grading-overrides.tsv")):
    with open(os.path.join(R, "grading-overrides.tsv")) as f:
        for o in csv.DictReader(f, delimiter="\t"):
            overrides[(o["model"], o["id"])] = o["pass"] == "true"
for r in serve:
    if r["kind"] == "question":
        r["pass"] = overrides.get((r["model"], r["id"]), grade(QUESTIONS[r["id"]], r["answer"]))
server = load_tsv("server.tsv")
background = load_tsv("background-cpu.tsv")

lines = ["# Results", "",
         "llama.cpp b11554, 2 threads, Q4_K_M, CPU only. Speed from llama-bench (3 repetitions, mean and range).",
         "Latency, memory and the sanity check from llama-server (`-c 4096 -np 1 --load-mode none`).", "",
         "| Model | Params (B) | File (GiB) | Prompt t/s (pp512) | Generation t/s (tg128) | Load s | Peak RSS MiB "
         "| First token s (median) | Sanity check | Background CPU % |",
         "| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |"]
for m in models:
    name = m["name"]
    b = bench.get(name, {})
    s = server.get(name, {})
    lat = [r["ttft_s"] for r in serve if r["model"] == name and r["kind"] == "latency" and not r["warmup"]]
    qs = [r for r in serve if r["model"] == name and r["kind"] == "question"]

    def rate(k):
        if k not in b:
            return "n/a"
        avg, lo, hi = b[k]
        return f"{avg:.1f} ({lo:.1f}-{hi:.1f})"

    lines.append("| " + " | ".join([
        name,
        f"{b['params_b']:.2f}" if b else "n/a",
        f"{b['size_gib']:.2f}" if b else "n/a",
        rate("pp"), rate("tg"),
        s.get("load_s", "n/a") if s.get("status") == "ok" else s.get("status", "n/a"),
        f"{int(s['peak_rss_mib']):,}" if s.get("peak_rss_mib") else "n/a",
        f"{statistics.median(lat):.2f}" if lat else "n/a",
        f"{sum(r['pass'] for r in qs)}/{len(qs)}" if qs else "n/a",
        background.get(name, {}).get("busy_pct_before", "n/a"),
    ]) + " |")

ids = []
for r in serve:
    if r["kind"] == "question" and r["id"] not in ids:
        ids.append(r["id"])
names = [m["name"] for m in models if any(r["model"] == m["name"] for r in serve)]
if ids:
    lines += ["", "## Sanity check, per question", "",
              "| Question | " + " | ".join(names) + " |",
              "| --- | " + " | ".join("---" for _ in names) + " |"]
    for q in ids:
        cells = []
        for n in names:
            hit = [r for r in serve if r["model"] == n and r.get("id") == q]
            cells.append(("pass" if hit[0]["pass"] else "FAIL") if hit else "")
        lines.append(f"| {q} | " + " | ".join(cells) + " |")

rounds = {}
for b in load_jsonl("speed-rounds.jsonl"):
    name = by_file[os.path.basename(b["model_filename"])]
    key = "pp" if b["n_gen"] == 0 else "tg"
    rounds.setdefault(name, {}).setdefault(key, []).append(b["avg_ts"])
nocache = load_tsv("memory-no-prompt-cache.tsv")
if rounds:
    lines += ["", "## Interleaved speed rounds (speed_rounds.sh)", "",
              "Each round runs every model once (pp256, tg64, 2 threads), so background load hits all of them alike.",
              "Median of the rounds, with min-max.", "",
              "| Model | Prompt t/s (pp256) | Generation t/s (tg64) | Rounds |", "| --- | --- | --- | --- |"]
    for m in models:
        d = rounds.get(m["name"])
        if not d:
            continue
        cell = lambda v: f"{statistics.median(v):.1f} ({min(v):.1f}-{max(v):.1f})" if v else "n/a"
        lines.append(f"| {m['name']} | {cell(d.get('pp', []))} | {cell(d.get('tg', []))} | {len(d.get('tg', []))} |")
if nocache:
    lines += ["", "## Memory with the prompt cache off (mem_check.sh, `--cache-ram 0`)", "",
              "| Model | RSS after load MiB | Peak RSS MiB | Peak with default prompt cache MiB | Load s |",
              "| --- | --- | --- | --- | --- |"]
    for m in models:
        n = nocache.get(m["name"])
        if not n:
            continue
        default = server.get(m["name"], {})
        dpeak = f"{int(default['peak_rss_mib']):,}" if default.get("peak_rss_mib") else default.get("status", "n/a")
        if m["name"] == "Gemma 4 E4B" and not default.get("peak_rss_mib"):
            dpeak = "killed at the 6 GB cap"
        lines.append(f"| {m['name']} | {int(n['rss_after_load_mib']):,} | {int(n['peak_rss_mib']):,} | {dpeak} | {n['load_s']} |")

with open(os.path.join(R, "summary.md"), "w") as f:
    f.write("\n".join(lines) + "\n")
print("\n".join(lines))
