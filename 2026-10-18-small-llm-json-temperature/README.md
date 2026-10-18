# Small local LLM JSON above temperature 0: temperature vs samplers vs grammar

**Finding:** at temperature 0.8 and even 1.5, three small models (Qwen 3.5 0.8B, Qwen 3.5 2B, Gemma 4 E2B) kept
returning schema-valid JSON as long as llama-server's default truncation samplers were on (top_k 40, top_p 0.95,
min_p 0.05): 113 to 120 of 120 replies passed the schema after stripping a Markdown fence. With temperature as the
only sampler (top_k, top_p and min_p off), temperature 1.5 broke it: 70 of 120 valid without a grammar, 109 of 120
with one, because the grammar keeps the syntax but cannot stop the model filling strings with junk until it hits the
token limit.

Write-up: https://aitoollabs.dev/posts/small-llm-json-temperature/

Hardware: Intel Core i5-7200U (2 cores, 4 threads, 15 W laptop part), 19 GB RAM, no GPU used. Run on 2026-10-18
with llama.cpp b11554 (commit `966baae`), 2 threads, 4 server slots. Other programs used 30% to 55% of total CPU when
each model started.

## Results

20 support emails per model. `t0` runs once; every other condition runs twice per email, each time with its own seed.
Thinking off, at most 300 output tokens. "Valid" means the reply passes `schema.json` after removing a Markdown fence
or text around the object. "Right" means all five graded fields match the labels.

All three models together:

| Condition | Mode | Valid | All 5 fields right | Hit the 300-token limit |
| --- | --- | --- | --- | --- |
| temperature 0 | prompt | 59/60 | 47/60 | 0 |
| temperature 0 | grammar | 60/60 | 48/60 | 0 |
| 0.8, default samplers | prompt | 119/120 | 86/120 | 0 |
| 0.8, default samplers | grammar | 120/120 | 95/120 | 0 |
| 0.8, temperature only | prompt | 112/120 | 84/120 | 0 |
| 0.8, temperature only | grammar | 120/120 | 81/120 | 0 |
| 1.5, default samplers | prompt | 113/120 | 74/120 | 0 |
| 1.5, default samplers | grammar | 120/120 | 82/120 | 0 |
| 1.5, temperature only | prompt | 70/120 | 47/120 | 24 |
| 1.5, temperature only | grammar | 109/120 | 51/120 | 11 |

All 5 fields right, per model (prompt / grammar):

| Condition | Qwen 3.5 0.8B | Qwen 3.5 2B | Gemma 4 E2B |
| --- | --- | --- | --- |
| temperature 0 | 13/20 / 13/20 | 18/20 / 18/20 | 16/20 / 17/20 |
| 0.8, default samplers | 23/40 / 24/40 | 33/40 / 38/40 | 30/40 / 33/40 |
| 0.8, temperature only | 19/40 / 18/40 | 32/40 / 32/40 | 33/40 / 31/40 |
| 1.5, default samplers | 13/40 / 16/40 | 33/40 / 34/40 | 28/40 / 32/40 |
| 1.5, temperature only | 3/40 / 6/40 | 11/40 / 13/40 | 33/40 / 32/40 |

Other findings:
- **llama.cpp applies temperature last.** Its default sampler chain is `penalties, dry, top_n_sigma, top_k, typ_p, top_p, min_p, xtc,
  temperature`,
  so min_p 0.05 removes unlikely tokens before temperature flattens what is left. That is why temperature 1.5 with the
  defaults stayed close to 0.8.
- **Gemma 4 E2B did not care.** Its accuracy stayed between 28 and 33 of 40 in every sampled condition, including
  temperature 1.5 with no truncation. Qwen 3.5 0.8B fell from 13/20 at temperature 0 to 3/40 at 1.5 without truncation.
- **A grammar cannot save a broken distribution.** At 1.5 with temperature only, the grammar still forced `{"customer_name": "`,
  and the model then wrote multilingual junk inside a string until the 300-token limit (11 of 120 replies).
- **Fences change with sampling.** Qwen 3.5 0.8B never fenced its JSON at temperature 0 but did in 4 to 12 of 40 replies
  per sampled condition. Gemma fenced every reply in every condition.
- **One seed for every request is not random.** A first run gave every email the same seed per repeat. Qwen 3.5 0.8B
  then fenced all 20 emails with seed 1 and none with seed 2, in three of the four sampled conditions (in the fourth, seed 1 opened all
  20 replies with an empty `<think>` block): the first token was the same coin flip with the same coin. That run is kept in [`results/shared-seed/`](results/shared-seed/).

Per-field accuracy, failure kinds and every miss: [`results/summary.md`](results/summary.md).

## What is in here

| File | What it does |
| --- | --- |
| `inputs.jsonl`, `schema.json` | The 20 labelled emails and the ticket schema from [the temperature 0 test](../2026-10-14-small-llm-json-output/). |
| `models.tsv` | The three GGUF files (bartowski's Q4_K_M quantizations). |
| `run.sh` | Starts one llama-server per model (4 slots, 2 threads, `--cache-ram 0`, 6 GB memory cap) and runs the bench. |
| `temp_bench.py` | Standard library only. Sends every email in 5 conditions x 2 modes, four requests at a time, and saves the raw replies. |
| `grade.py` | Standard library only. Parses, validates and checks the labels; writes `summary.md` and `graded.jsonl`. `--dir results/shared-seed` grades the first run. |
| `results/` | `raw.jsonl` (1,080 replies), `graded.jsonl`, `summary.md`, `server.tsv` (background CPU, peak RAM, minutes per model), `shared-seed/` (the first run, 360 replies from Qwen 3.5 0.8B). |

## How to run it

Needs Linux x86-64, Python 3, `curl`, `bc`, systemd (for the memory cap) and the llama.cpp build and GGUF files from
[`../2026-10-13-small-llms-laptop-cpu/setup.sh`](../2026-10-13-small-llms-laptop-cpu/setup.sh). Link or copy its
`vendor/` and `models/` folders here, then:

```sh
./run.sh              # 2 hours 45 minutes for 3 models on the laptop above
python3 grade.py
```

`ONLY="Gemma 4 E2B" ./run.sh` runs one model; `LIMIT=1` sends only the first email.

## Limits

- 40 sampled replies per model and cell. Differences of a few replies between cells are within noise; the collapse
  at 1.5 without truncation is not.
- Four requests ran at once (two slots per mode), so batch numerics can differ slightly from one request at a time.
- `repeat_penalty` was 1.0 (off, the llama-server default). Ollama's defaults differ (top_p 0.9, min_p 0) and I did
  not test Ollama, vLLM or any other server here.
- The emails and labels are synthetic and written by me.

MIT license.
