# Small LLMs on a 2-core laptop CPU: 10 models measured

**Finding:** on a 2-core laptop CPU, Gemma 4 E2B was the best trade-off of 10 small models at Q4: 7.1 tokens/s, 3.9 GB
of RAM and 18 of 20 sanity questions right. Llama 3.2 1B ran at 15.9 tokens/s in 1 GB but got 14 of 20; Gemma 4 E4B got
all 20 at 3.9 tokens/s and only fit in 6 GB with llama-server's prompt cache turned off.

Write-up: https://aitoollabs.dev/posts/best-small-llm-for-cpu/

Hardware: Intel Core i5-7200U (2 cores, 4 threads, 15 W laptop part), 19 GB RAM, no GPU used. Run on 2026-10-13 with
llama.cpp b11554 (commit `966baae`). The laptop was not idle: another program kept one of the 4 logical CPUs busy
during every measurement (27-30% of total CPU before each speed round, more during the first pass). Expect somewhat
higher numbers on an idle machine of the same kind.

## Results

All models are Q4_K_M GGUF files from bartowski's Hugging Face repos, run with 2 threads, a 4,096-token context and
1 slot. Speed is the median of 3 interleaved rounds of `llama-bench` (every model once per round). First token is the
median of 3 streamed chat requests with a 362 to 429-token prompt (the same note, tokenized by each model). Memory is
the peak RSS of llama-server with `--load-mode none --cache-ram 0`.

| Model | Params (B) | File (GiB) | Prompt t/s | Generation t/s | First token (s) | Peak RAM (MiB) | Sanity check |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Qwen 3.5 0.8B | 0.77 | 0.53 | 76.8 | 18.9 | 5.3 | 765 | 12/20 |
| Llama 3.2 1B | 1.24 | 0.74 | 48.6 | 15.9 | 8.5 | 1,032 | 14/20 |
| Qwen 2.5 1.5B | 1.54 | 0.91 | 31.1 | 13.0 | 14.3 | 1,173 | 15/20 |
| Qwen 3.5 2B | 1.94 | 1.29 | 26.0 | 9.1 | 12.7 | 1,517 | 14/20 |
| Gemma 4 E2B | 4.65 | 3.21 | 18.5 | 7.1 | 17.0 | 3,880 | 18/20 |
| SmolLM3 3B | 3.08 | 1.78 | 14.9 | 6.2 | 33.5 | 2,269 | 16/20 |
| Llama 3.2 3B | 3.21 | 1.87 | 14.6 | 5.8 | 28.2 | 2,517 | 16/20 |
| Phi-4 mini | 3.84 | 2.31 | 12.1 | 5.3 | 28.9 | 3,043 | 15/20 |
| Qwen 3.5 4B | 4.33 | 2.80 | 12.0 | 4.2 | 40.8 | 3,168 | 19/20 |
| Gemma 4 E4B | 7.52 | 5.02 | 11.2 | 3.9 | 36.2 | 6,002 | 20/20 |

Ranges, the first (busier) pass and the per-question table: [`results/summary.md`](results/summary.md).

Other findings:
- **llama-server's prompt cache costs RAM.** With the default `--cache-ram 8192`, peak memory after 24 requests was
  1,684 MiB for Qwen 3.5 0.8B (765 with `--cache-ram 0`), 2,432 for Qwen 3.5 2B (1,517) and 5,557 for Qwen 3.5 4B
  (3,168). Gemma 4 E4B was OOM-killed at the 6 GB cap with the cache on; with it off, 24 requests added only 77 MiB.
- **Too many threads collapse generation when the CPU is shared.** With another program keeping one logical CPU busy,
  Qwen 3.5 0.8B generated 17.84 tokens/s at 2 threads, 19.18 at 3 and 0.37 at 4. An earlier run under heavier load
  gave 15.81 and 0.31 ([`results/thread-check.md`](results/thread-check.md)).
- **Every model passed extraction, JSON, sentiment, SQL and the bug fix.** Letter-level tasks (reverse a word, count
  the r's) and multi-step arithmetic split them.

## What is in here

| File | What it does |
| --- | --- |
| `setup.sh` | Builds llama.cpp b11554 and downloads the 10 GGUF files listed in `models.tsv` into `vendor/` and `models/` (git-ignored, about 20 GiB). |
| `models.tsv` | Model names, file names and source repos. |
| `run_all.sh` | First pass per model: `llama-bench` (pp512, tg128, 3 repetitions), then llama-server with default prompt cache: load time, peak memory, first-token latency and the 20 questions. |
| `mem_check.sh` | Memory pass with `--cache-ram 0`: RSS after load and peak after 4 chat requests (all 20 questions with `FULL=<model>`). |
| `speed_rounds.sh` | Interleaved speed pass: each round runs `llama-bench` (pp256, tg64) once per model. |
| `thread_check.sh` | `llama-bench` at 2, 3 and 4 threads on the smallest model, with the background CPU load recorded first. |
| `serve_bench.py` | Standard library only. Streams chat requests for first-token latency and asks the 20 questions with thinking off. |
| `questions.jsonl` | The 20 questions, each with its grading rule. |
| `summarize.py` | Builds `results/summary.md` from the raw files. |
| `results/` | Raw `llama-bench` JSON lines, every answer, memory tables, background load and model checksums. Server logs stay local (git-ignored). |

Every server and benchmark runs in a systemd user scope with `MemoryMax=6G`, so a model that needs more is killed alone.

## How to run it

Needs Linux x86-64, `cmake`, a C++ compiler, Python 3, `curl`, `bc` and systemd (for the memory cap).

```sh
./setup.sh
./run_all.sh          # about 90 minutes on the laptop above, under background load
./mem_check.sh
./speed_rounds.sh     # about 22 minutes for 3 rounds
./thread_check.sh
python3 summarize.py
```

## The sanity check

20 short questions with one right answer: arithmetic, dates, facts, a translation, character-level tasks, extraction,
JSON output, sentiment, SQL and a one-line bug fix. Temperature 0, thinking turned off (`--reasoning off` and
`enable_thinking: false`), at most 200 tokens. Numbers are graded by the first number in the answer, because each
question asks for the number alone. One verbose but correct answer (SmolLM3, pencils) was regraded by hand; see
`results/grading-overrides.tsv`. It is a quick check of everyday reliability, not a benchmark: one or two questions of
difference between models means little.

## Caveats

- One laptop, one quantization (Q4_K_M), 2 threads, a 4,096-token context. GPU offload is a different comparison.
- The machine had background load (see above). The speed rounds interleave models so it hits all of them alike, and
  generation speed stayed within 16% of each model's median in every round (prompt speed within 21%), but absolute
  numbers would be higher on an idle machine. Qwen 2.5 1.5B generated 13.0 tokens/s here versus 14.1 (tg128, 2
  threads) in the previous day's run of the same build.
- Gemma 4 E4B's questions ran in `mem_check.sh` (prompt cache off) because the first pass was killed; same settings
  otherwise.
- Thinking models (Qwen 3.5, SmolLM3) were tested with thinking off. With thinking on, they may answer more questions
  right, at the cost of many more generated tokens on a CPU that makes 4 to 19 per second.

MIT license, like the rest of this repository. The models have their own licenses; this repo does not include them.
