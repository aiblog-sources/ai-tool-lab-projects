# Ollama vs llama.cpp on a 2-core laptop CPU

**Finding:** with the same settings, Ollama and llama.cpp generated text at the same speed on a laptop CPU (within 4%,
which is inside the restart-to-restart noise), because Ollama 0.40.3 runs llama.cpp's own `llama-server` underneath.
The differences people notice come from defaults: llama-server's default context reserved enough memory to get a
3B model killed under a 6 GB cap, and Ollama took 0.5 to 1.6 s longer to answer the first request after a cold start.

Write-up: https://aitoollabs.dev/posts/ollama-vs-llama-cpp-cpu/

Hardware: Intel Core i5-7200U (2 cores, 4 threads, 15 W laptop part), 19 GB RAM, no GPU used. Run on 2026-10-12.

## Results

Median of 5 runs after 1 warm-up, 346-token prompt (347 for Llama), 128 generated tokens, temperature 0. Both
servers stay loaded and are called in alternating order. Generation speed in tokens per second:

| Config | Qwen 2.5 1.5B: Ollama | llama.cpp | Llama 3.2 3B: Ollama | llama.cpp |
| --- | --- | --- | --- | --- |
| Each tool's defaults | 14.65 | 14.68 | 6.80 * | killed (over 6 GB) |
| Same context and slots (`-c 4096 -np 1`) | 14.68 | 13.75 | 6.80 | 6.73 |
| Ollama's exact launch flags, 2 threads | 14.62 | 14.69 | 6.81 | 7.06 |
| Ollama's exact launch flags, 4 threads | 16.66 | 16.11 | 7.71 | 7.52 |
| `-c 4096 -np 1 -t 4` vs `num_thread 4` | 16.39 | 14.69 | 7.99 | 7.61 |

\* Ollama's defaults are the same as the "same context" row: it always launches with `-c 4096 -np 1` here.

Prompt processing was within 5% in every config except one (Qwen, 4 threads, Ollama flags: 46.9 vs 43.1 t/s).
Full table with min-max ranges: [`results/summary.md`](results/summary.md).

Memory (peak RSS of the inference process, MiB):

| Setup | Qwen 2.5 1.5B | Llama 3.2 3B |
| --- | --- | --- |
| Ollama (peak while benchmarking) | 1,231 | 2,768 |
| llama-server, defaults (4 slots x 32,768 tokens) | 2,610 | killed at the 6 GB cap |
| llama-server `-c 4096 -np 1` (mmap on) | 1,825 | 4,056 |
| llama-server with Ollama's flags (`--load-mode none`) | 1,232 | 2,770 |

With mmap, about half of llama-server's memory is the model file mapped from the page cache (951 of 1,714 MiB for
Qwen, 1,935 of 3,751 MiB for Llama, see [`results/memory-detail.tsv`](results/memory-detail.tsv)). The kernel can
reclaim those pages, so the RSS number overstates the real pressure.

Other numbers:
- Request overhead outside the engine (client wall time minus the engine's own prompt and generation time): 12-15 ms
  for llama-server, 16-20 ms for Ollama with Qwen; 41-46 ms and 47-52 ms with Llama. Ollama's Go proxy adds 3-10 ms.
- Cold start to first token, model file already in the page cache, 3 tries each:
  Qwen 2.68-2.74 s (Ollama) vs 2.16-2.26 s (llama-server); Llama 5.34-6.12 s vs 4.38-4.73 s
  ([`results/coldstart.tsv`](results/coldstart.tsv)).
- Thread sweep with `llama-bench` ([`results/thread-sweep.md`](results/thread-sweep.md)): prompt processing peaks at
  2 threads (the physical cores); generation gains 6-7% from 2 to 4 threads.
- `ollama serve` itself (the Go process, without a model) peaked at 68 MiB.

## What is in here

| File | What it does |
| --- | --- |
| `setup.sh` | Downloads Ollama v0.40.3, builds llama.cpp at commit `966baae`, fetches both GGUF files into `vendor/` and `models/` (git-ignored). |
| `bench.py` | Standard library only. Sends the same raw prompt to Ollama (`/api/generate`, `raw: true`) and llama-server (`/completion`), alternating order, and records each server's own timings plus wall time. |
| `run_all.sh` | Starts a private Ollama on port 11500 and a llama-server on 11600 for each config (defaults, matched, 4 threads), runs `bench.py`, records peak RSS. Every llama-server runs in a systemd user scope capped at 6 GB. |
| `same_flags.sh` | Starts llama-server with the flags Ollama itself passes to its bundled llama-server, benchmarks at 2 and 4 threads, and records anonymous vs file-backed memory. |
| `sweep.sh` | `llama-bench` at 1 to 4 threads. |
| `coldstart.sh` | Time from nothing loaded to the first token, for both. |
| `summarize.py` | Turns `results/runs.jsonl` into the Markdown table. |
| `results/` | Raw runs, memory, sweep, cold start, and the server logs (paths scrubbed). |

## How to run it

Needs Linux x86-64, `cmake`, a C++ compiler, Python 3, `curl`, `bc`, and systemd (for the memory cap). About 3 GB of
downloads. Nothing is installed system-wide.

```sh
./setup.sh
./run_all.sh        # about 25 minutes on the laptop above
./same_flags.sh     # about 20 minutes
./sweep.sh
./coldstart.sh
python3 summarize.py
```

`RUNS=10 ./run_all.sh` changes the number of measured runs.

## Caveats

- Same GGUF files, but not the same llama.cpp build: Ollama 0.40.3 bundles llama.cpp b11351, and the standalone build
  is a newer commit (`966baae`, 2026-10-11).
- Restarting a server with identical settings moved generation speed by up to about 6% (compare the Qwen "defaults"
  and "same context" rows for llama.cpp). Treat differences below that as noise.
- One run of the Llama 4-thread Ollama-flags config was disturbed (3.2 t/s for llama.cpp, 3.8 for Ollama in the same
  round); the medians above are not affected much, the ranges in `summary.md` show it.
- 2 models, 1 prompt, 1 machine. CPU only: GPU offload is a different comparison.

MIT license, like the rest of this repository.
