# Structured JSON from small local LLMs: grammar vs prompt

**Finding:** six small models (0.8B to 4B, Q4, llama.cpp on a 2-core laptop CPU) turned 20 support emails into JSON.
llama-server's `json_schema` grammar made every reply valid against the schema (120/120), but it did not make the
answers more accurate: 87 of 120 tickets had all five checked fields right with the grammar, 88 without it. With the
grammar and no schema in the prompt, accuracy fell to 47 of 120 and models invented order numbers for 13 of 30
emails that had none.

Write-up: https://aitoollabs.dev/posts/small-llm-json-output/

Hardware: Intel Core i5-7200U (2 cores, 4 threads, 15 W laptop part), 19 GB RAM, no GPU used. Run on 2026-10-14
with llama.cpp b11554 (commit `966baae`), 2 threads. Other programs used 26% to 68% of total CPU when each model
started, so the speed columns only compare modes within one model.

## Results

20 emails per model and mode, temperature 0, thinking off, at most 300 output tokens (no reply hit the limit).

| Model | Mode | Parses as-is | Parses after cleanup | Passes schema | All 5 fields right |
| --- | --- | --- | --- | --- | --- |
| Qwen 3.5 0.8B | prompt | 20/20 | 20/20 | 19/20 | 12/20 |
| Qwen 3.5 0.8B | grammar | 20/20 | 20/20 | 20/20 | 12/20 |
| Qwen 3.5 0.8B | grammar-only | 20/20 | 20/20 | 20/20 | 3/20 |
| Llama 3.2 1B | prompt | 5/20 | 20/20 | 17/20 | 8/20 |
| Llama 3.2 1B | grammar | 20/20 | 20/20 | 20/20 | 8/20 |
| Llama 3.2 1B | grammar-only | 20/20 | 20/20 | 20/20 | 1/20 |
| Qwen 2.5 1.5B | prompt | 20/20 | 20/20 | 20/20 | 15/20 |
| Qwen 2.5 1.5B | grammar | 20/20 | 20/20 | 20/20 | 15/20 |
| Qwen 2.5 1.5B | grammar-only | 20/20 | 20/20 | 20/20 | 5/20 |
| Qwen 3.5 2B | prompt | 1/20 | 20/20 | 20/20 | 19/20 |
| Qwen 3.5 2B | grammar | 20/20 | 20/20 | 20/20 | 18/20 |
| Qwen 3.5 2B | grammar-only | 20/20 | 20/20 | 20/20 | 13/20 |
| Gemma 4 E2B | prompt | 0/20 | 20/20 | 20/20 | 15/20 |
| Gemma 4 E2B | grammar | 20/20 | 20/20 | 20/20 | 15/20 |
| Gemma 4 E2B | grammar-only | 20/20 | 20/20 | 20/20 | 11/20 |
| Qwen 3.5 4B | prompt | 20/20 | 20/20 | 20/20 | 19/20 |
| Qwen 3.5 4B | grammar | 20/20 | 20/20 | 20/20 | 19/20 |
| Qwen 3.5 4B | grammar-only | 20/20 | 20/20 | 20/20 | 14/20 |

Modes:
- **prompt**: the JSON Schema is in the system prompt; nothing constrains the output.
- **grammar**: the same prompt plus `response_format: {type: "json_schema", ...}`, which llama-server compiles into a
  grammar that masks every token that would break the schema.
- **grammar-only**: the grammar, with a one-line system prompt that does not show the schema.

Other findings:
- **"Parses after cleanup" was always a Markdown code fence.** Gemma 4 E2B (20/20), Qwen 3.5 2B (19/20) and Llama 3.2
  1B (15/20) wrapped the JSON in a fence; stripping it was enough every time.
- **The 4 schema failures without a grammar:** two order numbers in the wrong format, a stray `"$schema"` key, and
  Llama 3.2 1B wrapping the whole ticket in `{"$delete": {...}}` for an account deletion email.
- **The grammar turns some errors into valid wrong answers.** Qwen 3.5 0.8B invented `ORD-123456789` for an email with
  no order number; with the grammar the same reply became `ORD-12345`, which passes the pattern.
- **Speed:** within one model, median generation speed with and without the grammar differed by 2% or less, except
  Llama 3.2 1B, which ran at 3 to 4 tokens/s in every mode (it did 15.9 on the same laptop the day before; I could
  not tell how much of that was background load).
- **The schema costs prompt tokens.** It made the system prompt about 500 to 580 tokens instead of 55 to 74, which took
  12 to 63 s to read on the first request. With llama-server's slot cache, later requests only processed the email
  (41 to 53 tokens).

Per-field accuracy, speed per mode and every miss: [`results/summary.md`](results/summary.md).

## What is in here

| File | What it does |
| --- | --- |
| `inputs.jsonl` | 20 short support emails I wrote, each with hand labels for the five graded fields. |
| `schema.json` | The ticket schema: name, order number (pattern or null), category enum, priority enum, items array, refund flag. |
| `models.tsv` | The six GGUF files (bartowski's Q4_K_M quantizations). |
| `run.sh` | Starts one llama-server per model (3 slots, 2 threads, `--cache-ram 0`, 6 GB memory cap) and runs the bench. |
| `json_bench.py` | Standard library only. Sends every email in the three modes, one server slot per mode, and saves the raw replies. |
| `grade.py` | Standard library only. Parses, validates against `schema.json` and checks the labels. Writes `results/summary.md`. |
| `results/` | `raw.jsonl` (every reply with server timings), `graded.jsonl`, `summary.md`, `server.tsv` (background CPU, peak RAM, minutes per model). |

`grade.py` has its own small validator for the keywords `schema.json` uses. I checked it against the `jsonschema`
Python package (4.26.0, Draft 2020-12): both agreed on all 360 replies.

## How to run it

Needs Linux x86-64, Python 3, `curl`, `bc`, systemd (for the memory cap) and the llama.cpp build and GGUF files from
[`../2026-10-13-small-llms-laptop-cpu/setup.sh`](../2026-10-13-small-llms-laptop-cpu/setup.sh). Link or copy its
`vendor/` and `models/` folders here, then:

```sh
./run.sh              # about 1 hour 50 minutes for 6 models on the laptop above, under background load
python3 grade.py
```

`ONLY="Qwen 3.5 4B" ./run.sh` runs one model; `LIMIT=2` sends only the first two emails.

## Grading

A reply counts as right when five fields match the labels: `customer_name` (case-insensitive), `order_id` (exact,
or null), `category` (two emails accept two categories, for example a broken blender with a refund request is
`technical` or `billing`), `refund_requested`, and `items` (same number of items, each labelled product found by
keyword with the right quantity). `priority` is in the schema but not graded. The emails and labels are synthetic and
written by me, so they are cleaner than real support mail.

MIT license.
