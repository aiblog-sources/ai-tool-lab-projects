# Ollama structured outputs through the OpenAI API (`response_format`), measured

**Finding:** on Ollama's OpenAI-compatible endpoint, `response_format: json_schema` made every reply valid JSON as-is
(80 of 80 on Ollama 0.40.16, against 40 of 80 without it, mostly because of Markdown fences), but fewer replies had
every field right: 48 of 80 against 55 of 80. `json_object` removed the fences with no loss (80 of 80 valid, 55 of 80
right). Ollama sorts the schema's properties alphabetically before llama.cpp builds the grammar
([ollama/ollama#18717](https://github.com/ollama/ollama/issues/18717)), so the ticket starts with `category` and
lists `items` before `order_id`. With only the key order changed (upstream llama-server, same seeds), Qwen 3.5 0.8B
got 19 of 40 tickets right instead of 25 and invented products in 8 of 10 replies to emails that had none; Qwen 3.5
2B was unaffected. Separately, Ollama 0.40.14 stopped forcing temperature 1.0 and top_p 1.0 on `/v1`: with only
`temperature: 1.5` sent, fully right replies went from 24 of 80 on 0.40.13 to 50 of 80 on 0.40.16.

Write-up: https://aitoollabs.dev/posts/ollama-structured-outputs-openai-api/

Hardware: a 2-core, 4-thread laptop CPU (15 W part), 19 GB RAM, no GPU used. Run on 2026-11-12 with Ollama 0.40.13
and 0.40.16 (official Linux builds, CPU libraries only), upstream llama.cpp b11554 for the key order test, and the
OpenAI Python SDK 3.49.0 for the format probe. Two parallel slots, 2 threads, 6 GB memory cap on every server.
Other programs used 31% to 34% of total CPU when each Ollama server started (`results/server.tsv`), and more during
the key order test.

## Results

20 labelled support emails per model, Qwen 3.5 0.8B and Qwen 3.5 2B (bartowski's Q4_K_M), imported with a
Modelfile that has only `FROM`. Thinking off, at most 300 output tokens. Every request goes to
`/v1/chat/completions`. Temperature 0 runs once per email, every sampled condition twice, each request with its own
seed (the same seeds on both versions). "Valid" means the reply passes `schema.json`; "as-is" means `json.loads`
on the raw reply, "after cleanup" also strips a Markdown fence. "Right" means all five graded fields match the
labels. Both models together, 40 or 80 replies per row:

| Ollama | Condition | Samplers (logged) | Valid as-is | Valid after cleanup | All 5 fields right |
| --- | --- | --- | --- | --- | --- |
| 0.40.13 | `t0` | greedy | 19/40 | 39/40 | 32/40 |
| 0.40.13 | `none` | temp 1.0, top_p 1.0 | 36/80 | 70/80 | 47/80 |
| 0.40.13 | `schema` | temp 1.0, top_p 1.0 | 80/80 | 80/80 | 41/80 |
| 0.40.13 | `1.5` | temp 1.5, top_p 1.0 | 18/80 | 46/80 | 24/80 |
| 0.40.13 | `1.5-schema` | temp 1.5, top_p 1.0 | 80/80 | 80/80 | 31/80 |
| 0.40.16 | `t0` | greedy | 21/40 | 39/40 | 33/40 |
| 0.40.16 | `t0-schema` | greedy | 40/40 | 40/40 | 31/40 |
| 0.40.16 | `none` | temp 0.8, top_p 0.9 | 40/80 | 76/80 | 55/80 |
| 0.40.16 | `schema` | temp 0.8, top_p 0.9 | 80/80 | 80/80 | 48/80 |
| 0.40.16 | `object` | temp 0.8, top_p 0.9 | 80/80 | 80/80 | 55/80 |
| 0.40.16 | `schema-only` | temp 0.8, top_p 0.9 | 80/80 | 80/80 | 52/80 |
| 0.40.16 | `1.5` | temp 1.5, top_p 0.9 | 42/80 | 76/80 | 50/80 |
| 0.40.16 | `1.5-schema` | temp 1.5, top_p 0.9 | 80/80 | 80/80 | 48/80 |

Per model, per field, and every miss: [`results/summary.md`](results/summary.md). On the 5 emails with no order and
no products, 0.40.16's `schema` replies listed products in 8 of 20 sampled replies (4 of 20 for `none`) and gave an
order id in 4 of 20 (2 of 20); the most common invented id was `ORD-12345`, the example in the schema's own
description. Without a format, Qwen 3.5 2B wrapped most replies in a Markdown fence (18 of 20 greedy on 0.40.16).

What each condition sends, besides the messages: `t0` temperature 0; `none` nothing; `schema` the
`response_format` that the OpenAI SDK's `parse()` sends (`{"type": "json_schema", "json_schema": {"name", "schema",
"strict": true}}`); `object` `{"type": "json_object"}`; `schema-only` the same `json_schema` but a system prompt that
names the fields in one sentence instead of showing the schema; `1.5` temperature 1.5 only.

Samplers, read from the line Ollama's bundled llama-server logs per request (`results/effective-params.txt`): 0.40.13
used temperature 1.0 and top_p 1.0 when the request sent nothing, 0.40.16 used 0.8 and 0.9 (top_k 40, min_p 0 on
both). With temperature 0 and no `response_format`, the two versions gave the same text for 36 of 40 emails.

### Which `response_format` shapes Ollama enforces

`probe_formats.py` sends a schema that requires `{"animal": "zebra", "code": <integer>}` while the prompt asks for a
favourite colour, so a reply with `"animal": "zebra"` can only come from the grammar. Same result on both versions
and both models (`results/format-probe.jsonl`); every request returned HTTP 200:

| Shape sent to `/v1/chat/completions` | Schema enforced |
| --- | --- |
| OpenAI SDK `client.chat.completions.parse(response_format=PydanticModel)` | yes |
| `{"type": "json_schema", "json_schema": {"name": ..., "schema": {...}, "strict": true}}` | yes |
| `{"type": "JSON_SCHEMA", "json_schema": {...}}` (upper case) | yes |
| `{"type": "json_schema", "schema": {...}}` (schema one level up) | **no, silently** |
| `{"type": "json_schema", "json_schema": {<the schema itself>}}` (no `schema` key) | **no, silently** |
| `{"type": "json"}` | **no, silently** |
| `{"type": "json_object"}` | any JSON (cannot show the schema) |
| native `/api/chat` with `"format": {...}` | yes |

### Key order

Every `json_schema` reply from Ollama (440 of 440) had its keys in alphabetical order; every `json_object` reply
(80 of 80) started with `customer_name`, as declared. Ollama decodes the schema into a Go `map[string]any` before
passing it to its bundled llama-server ([`llm/llama_server.go`](https://github.com/ollama/ollama/blob/v0.40.16/llm/llama_server.go#L2462-L2490)),
and Go's JSON encoder sorts map keys. Upstream llama-server keeps the declared order (`results/order-probe.jsonl`).

`order_bench.py` isolates the effect: upstream llama-server b11554, Ollama 0.40.16's samplers (temperature 0.8,
top_k 40, top_p 0.9, min_p 0), the same seed for both requests of a pair, schema as declared vs properties sorted:

| Model | Declared: all 5 right | Sorted: all 5 right | Products invented (10 replies to no-order emails) | Order id invented |
| --- | --- | --- | --- | --- |
| Qwen 3.5 0.8B | 25/40 | 19/40 | 2 vs 8 | 2 vs 7 |
| Qwen 3.5 2B | 33/40 | 35/40 | 0 vs 2 | 0 vs 0 |
| Both | 58/80 | 54/80 | 2 vs 10 | 2 vs 7 |

Every reply in both conditions passed the schema.

### Library models on 0.40.16

`library_check.sh` sends one `/v1` request with no sampler fields to two library models from the
[2026-10-23 test](../2026-10-23-ollama-library-v1-samplers/) and records what Ollama 0.40.16 logged
(`results/library-v1-check.txt`): `ministral-3:3b` (ships temperature 0.15) got 0.15, and `llama-guard3:1b` (ships
0) got 0. On 0.40.9 both got 1.0.

## What is in here

| File | What it does |
| --- | --- |
| `inputs.jsonl`, `schema.json` | The 20 labelled emails and the ticket schema from [the 2026-10-14 test](../2026-10-14-small-llm-json-output/), unchanged. |
| `models.tsv` | The two GGUF files (bartowski's Q4_K_M builds of Qwen 3.5 0.8B and 2B). |
| `run.sh` | Per model and Ollama version: starts a private Ollama (2 parallel slots, 6 GB memory cap), imports the GGUF with a Modelfile that has only `FROM`, records the sampler parameters each condition gets, runs the format probe, runs the benchmark, stops the server. `RESUME=1` finishes an interrupted run. |
| `bench.py` | Standard library only. Sends the 20 emails to `/v1/chat/completions` per condition, with as few fields as the condition allows. |
| `probe_formats.py` | Sends nine `response_format` shapes (one through the real OpenAI Python SDK's `parse()`) with a schema the prompt contradicts. |
| `run_order.sh`, `order_bench.py`, `order_probe.py` | The key order test on upstream llama-server: the same requests with the schema's properties as declared and sorted alphabetically, same seeds. |
| `library_check.sh` | Starts 0.40.16 on an existing Ollama model store and records which temperature two library models get on `/v1` with no sampler fields. |
| `grade.py` | Standard library only. Parses, validates and checks the labels; writes `summary.md` and `graded.jsonl`. |
| `results/` | `raw.jsonl` (every reply), `graded.jsonl`, `summary.md` (per model, per field, every miss), `effective-params.txt`, `format-probe.jsonl`, `order-probe.jsonl`, `library-v1-check.txt`, `server.tsv`. |

## How to run it

Needs Linux x86-64, Python 3, `curl`, `bc` and systemd (for the memory cap). Put the two Ollama releases, a Python
venv with the OpenAI SDK and a llama.cpp build in `vendor/`, and the GGUF files in `models/`:

```sh
mkdir -p vendor models
for v in 0.40.13 0.40.16; do
  mkdir -p vendor/ollama-$v
  curl -sSL https://github.com/ollama/ollama/releases/download/v$v/ollama-linux-amd64.tar.zst \
    | tar --zstd -x -C vendor/ollama-$v --exclude='lib/ollama/cuda_*' --exclude='lib/ollama/mlx*'
done
python3 -m venv vendor/venv && vendor/venv/bin/pip install openai pydantic   # openai 3.49.0, pydantic 2.14.3 here
# vendor/llama.cpp: llama.cpp b11554 built with cmake (only needed for run_order.sh)
# models/: Qwen_Qwen3.5-0.8B-Q4_K_M.gguf and Qwen_Qwen3.5-2B-Q4_K_M.gguf from bartowski on Hugging Face
./run.sh          # about 2 hours on the laptop above
./run_order.sh    # about 1 hour
python3 grade.py
```

`ONLY="Qwen 3.5 0.8B"` runs one model, `VERSIONS=0.40.16` one Ollama version, `LIMIT=1 REPS=1` is a smoke test.

## Limits

- Two models from one family, 20 synthetic emails, one schema, Q4 quantization, thinking off, CPU only.
- Two seeds per email and condition. In the 2026-10-22 run, two conditions with identical settings differed by up to
  5 of 40 per model, so differences of that size here are noise. The 0.40.13 to 0.40.16 change at temperature 1.5
  and the 0.8B's invented products are not.
- The run was interrupted by a time limit during Qwen 3.5 2B on 0.40.16 after 214 of 280 requests; `RESUME=1`
  finished the missing 66 on a new server with the same seeds (`server.tsv` has the
  minutes of the second part only).
- Imported GGUF files only. Library models whose template Ollama renders itself may take the `/completion` path,
  which per #18717 keeps the schema's order. Not tested here.
- Other programs used about a third of the CPU during the Ollama runs and more during the llama-server runs; no
  result here depends on speed.

MIT license.
