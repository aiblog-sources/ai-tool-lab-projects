#!/usr/bin/env python3
"""Lists every model on https://ollama.com/library (popular order) and reads the sampler parameters its default tag
ships with: the `params` layer of the registry manifest, the same values `ollama show <model> --parameters` prints
after a pull. Nothing is downloaded except the manifest and the small params blob. Standard library only.

Writes results/library-params.jsonl (one line per model). `survey.py --summary` turns it into
results/library-summary.md: which models ship a temperature or top_p that Ollama's OpenAI-compatible route
(/v1/chat/completions) replaces with 1.0 when the client sends none (openai/openai.go in Ollama 0.40.9).
"""
import html
import json
import os
import re
import sys
import time
import urllib.error
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
REG = "https://registry.ollama.ai/v2/library"
ACCEPT = "application/vnd.docker.distribution.manifest.v2+json"


def get(url, accept=None):
    req = urllib.request.Request(url, headers={"Accept": accept} if accept else {})
    for attempt in range(3):
        try:
            with urllib.request.urlopen(req, timeout=60) as r:
                return r.read()
        except urllib.error.HTTPError as e:
            if e.code == 404:
                return None
            time.sleep(2 * (attempt + 1))
        except urllib.error.URLError:
            time.sleep(2 * (attempt + 1))
    raise RuntimeError(f"failed: {url}")


def library():
    page = get("https://ollama.com/library?sort=popular").decode()
    out = []
    for block in page.split('<li  class="flex items-baseline')[1:]:
        name = re.search(r'href="/library/([^"]+)"', block).group(1)
        tags = [html.unescape(t) for t in re.findall(r'<span\s+class="inline-flex[^"]*">([^<]+)</span>', block)]
        pulls = re.search(r'<span >([\d.]+[KMB]?)</span>\s*<span class="hidden sm:flex">&nbsp;Pulls', block)
        sizes = [t for t in tags if re.fullmatch(r"(e?\d+(\.\d+)?[mbkx]|\d+x\d+(\.\d+)?b|e\d+b)", t)]
        caps = [t for t in tags if t not in sizes]
        out.append({"rank": len(out) + 1, "name": name, "capabilities": caps, "sizes": sizes,
                    "pulls": pulls.group(1) if pulls else None})
    return out


def summarize():
    """Writes results/library-summary.md from results/library-params.jsonl."""
    ms = [json.loads(l) for l in open(os.path.join(HERE, "results", "library-params.jsonl"))]
    gen = [m for m in ms if m["tag"] and "embedding" not in m["capabilities"]]
    hit = [m for m in gen if "temperature" in m["params"] or "top_p" in m["params"]]
    temp = [m for m in gen if "temperature" in m["params"]]
    low = [m for m in temp if m["params"]["temperature"] < 1]
    zero = [m for m in temp if m["params"]["temperature"] == 0]
    topp = [m for m in gen if "top_p" in m["params"] and m["params"]["top_p"] < 1]
    top50 = [m for m in gen if m["rank"] <= 50]
    out = ["# What the Ollama library ships, and what /v1 replaces", "",
           f"Library page read in popular order: {len(ms)} models. {sum(not m['tag'] for m in ms)} have no `latest` "
           f"tag (mostly cloud-only) and {sum(bool(m['tag']) and 'embedding' in m['capabilities'] for m in ms)} are "
           f"embedding models; that leaves {len(gen)} generative models, read at their `latest` tag.", "",
           "| Count | Of generative models |", "| --- | --- |",
           f"| Ship a temperature or top_p (replaced by 1.0 on /v1 when the client sends none) | {len(hit)}/{len(gen)} |",
           f"| Ship a temperature below 1 | {len(low)}/{len(gen)} |",
           f"| Ship temperature 0 | {len(zero)}/{len(gen)} |",
           f"| Ship a top_p below 1 | {len(topp)}/{len(gen)} |",
           f"| Of the 50 most popular entries: ship temperature or top_p | "
           f"{sum(m in hit for m in top50)}/{len(top50)} |", "",
           "## Every model that ships a temperature or top_p", "",
           "| Rank | Model | Pulls | Shipped temperature | Shipped top_p | /v1 with nothing sent |",
           "| --- | --- | --- | --- | --- | --- |"]
    for m in hit:
        p = m["params"]
        out.append(f"| {m['rank']} | {m['name']} | {m['pulls']} | {p.get('temperature', '-')} | {p.get('top_p', '-')} "
                   f"| 1.0 / 1.0 |")
    open(os.path.join(HERE, "results", "library-summary.md"), "w").write("\n".join(out) + "\n")
    print("\n".join(out[:12]))


def main():
    if sys.argv[1:] == ["--summary"]:
        return summarize()
    models = library()
    print(f"{len(models)} models on the library page", file=sys.stderr)
    with open(os.path.join(HERE, "results", "library-params.jsonl"), "w") as f:
        for m in models:
            raw = get(f"{REG}/{m['name']}/manifests/latest", ACCEPT)
            if raw is None:
                m["tag"] = None  # no :latest tag (for example cloud-only models)
            else:
                m["tag"] = "latest"
                man = json.loads(raw)
                params = {}
                for layer in man.get("layers", []):
                    if layer["mediaType"].endswith(".params"):
                        params = json.loads(get(f"{REG}/{m['name']}/blobs/{layer['digest']}"))
                m["params"] = params
                m["size_mb"] = sum(l["size"] for l in man.get("layers", [])) // 1_000_000
            f.write(json.dumps(m) + "\n")
            print(m["rank"], m["name"], m.get("params"), file=sys.stderr, flush=True)


if __name__ == "__main__":
    main()
