# Ollama library models through the OpenAI API: shipped samplers vs /v1

**Finding:** 59 of 225 generative models in the Ollama library ship their own temperature or top_p (9 ship
temperature 0), and Ollama's OpenAI-compatible endpoint replaces both with 1.0 when a request leaves them out. That
cost `ministral-3:3b` (shipped temperature 0.15) 4 of 40 schema-valid JSON tickets and most of its consistency, and it
made the safety classifier `llama-guard3:1b` (shipped temperature 0) change its verdict on 6 of 24 prompts.

Write-up: https://aitoollabs.dev/posts/ollama-openai-api-ignores-model-temperature/

Follow-up to [2026-10-22-ollama-llama-server-samplers](../2026-10-22-ollama-llama-server-samplers/), which measured the
same `/v1` defaults on an imported GGUF with no Modelfile parameters.

Hardware: Intel Core i5-7200U (2 cores, 4 threads, 15 W laptop part), 19 GB RAM, no GPU used. Run on 2026-10-23 with
Ollama 0.40.9 (the current release), 2 parallel slots, 6 GB memory cap. Other programs used about two of the four
logical CPUs for most of the run, so nothing here compares speed.

## What the library ships

`survey.py` read the library page (popular order) and the `params` layer of each model's `latest` manifest on
2026-10-23 ([`results/library-summary.md`](results/library-summary.md), raw data in `results/library-params.jsonl`):

| Shipped in the `latest` tag | Generative models (of 225) |
| --- | --- |
| A temperature or top_p | 59 |
| A temperature below 1 | 35 |
| Temperature 0 | 9 |
| A top_p below 1 | 36 |
| Top 50 by pulls: temperature or top_p | 15 of 44 |

## What each route samples with

From the `sampler params` lines Ollama's bundled llama-server logs per request (`results/effective-params.txt`):

| Model (shipped values) | `/api/chat`, nothing sent | `/v1`, nothing sent | `/v1` with the shipped values sent |
| --- | --- | --- | --- |
| ministral-3:3b (temperature 0.15) | 0.15, top_p 0.9 | 1.0, top_p 1.0 | 0.15, top_p 1.0 |
| llama-guard3:1b (temperature 0) | 0, top_p 0.9 | 1.0, top_p 1.0 | 0, top_p 1.0 |
| qwen3.5:2b (1.0, top_k 20, top_p 0.95, presence_penalty 1.5) | 1.0, top_p 0.95 | 1.0, top_p 1.0 | 1.0, top_p 0.95 |
| gemma4:e2b (1.0, top_k 64, top_p 0.95) | 1.0, top_p 0.95 | 1.0, top_p 1.0 | 1.0, top_p 0.95 |
| qwen3:1.7b (0.6, top_k 20, top_p 0.95) | 0.6, top_p 0.95 | 1.0, top_p 1.0 | 0.6, top_p 0.95 |

`top_k` and `presence_penalty` from the model reached the sampler on both routes. qwen3:1.7b was probed but not
benchmarked (see below); its probe is at the end of `results/effective-params.txt`.

## JSON task

20 support emails to a JSON ticket (`inputs.jsonl`, `schema.json`, same as the 2026-10-14 to 2026-10-22 benches),
schema in the system prompt, no grammar, thinking off, at most 300 tokens. Each email twice per route, every request
with its own seed, no sampler fields sent. "Valid" = passes the schema after removing a Markdown fence; "right" = all
five graded fields match the labels; "repeats agree" = both replies for an email parse to the same object.

| Model | Route | Valid | Valid without cleanup | All 5 fields right | Repeats agree |
| --- | --- | --- | --- | --- | --- |
| ministral-3:3b | `/api/chat` | 40/40 | 30/40 | 37/40 | 15/20 |
| ministral-3:3b | `/v1` | 36/40 | 16/40 | 34/40 | 5/20 |
| qwen3.5:2b | `/api/chat` | 37/40 | 34/40 | 32/40 | 4/20 |
| qwen3.5:2b | `/v1` | 35/40 | 33/40 | 30/40 | 6/20 |
| gemma4:e2b | `/api/chat` | 40/40 | 0/40 | 37/40 | 16/20 |
| gemma4:e2b | `/v1` | 40/40 | 0/40 | 38/40 | 17/20 |

Only ministral, whose shipped temperature changes (0.15 to 1.0), moved clearly. qwen3.5:2b and gemma4:e2b only lose
top_p (0.95 to 1.0 at temperature 1.0) and stayed within noise. gemma4:e2b fenced all 80 replies on both routes.

Every miss: [`results/summary.md`](results/summary.md).

## Safety classifier

`llama-guard3:1b`, 24 prompts I wrote (8 safe, 8 borderline, 8 unsafe; `guard-prompts.jsonl`), 10 repeats per route,
nothing sent but the message and a seed ([`results/guard-summary.md`](results/guard-summary.md)):

| Route | Prompts whose label changed between repeats | Prompts with more than one unsafe category | Labels different from `/api/chat` |
| --- | --- | --- | --- |
| `/api/chat` (temperature 0) | 0/24 | 0/24 | 0/240 |
| `/v1` (temperature 1.0, top_p 1.0) | 6/24 | 5/24 | 12/240 |

The ibuprofen side-effects question came back `unsafe` (S6) in 6 of 10 `/v1` runs and `safe` in all 10 `/api/chat`
runs.

## What is in here

| File | What it does |
| --- | --- |
| `survey.py` | Reads the Ollama library page and each model's params layer from the registry (no model downloads). `--summary` writes `results/library-summary.md`. |
| `bench.py` | JSON bench against a running Ollama. `--probe` records the logged sampler values per condition. `--conds` picks conditions. |
| `guard.py` | Sends the guard prompts to `llama-guard3:1b` on both routes. |
| `run.sh` | Starts a private Ollama (6 GB cap), then probe + bench per model and the guard test. |
| `grade.py`, `guard_grade.py` | Score the raw replies into `results/summary.md` and `results/guard-summary.md`. |
| `results/raw.jsonl`, `results/guard-raw.jsonl` | Every reply. |
| `results/server.tsv` | Background CPU load, peak runner memory and minutes per model. |

## How to run it

Needs Python 3 (standard library only) and an Ollama binary. `run.sh` uses a private data folder under
`~/.cache/ai-tool-lab/ollama-library-v1/` and port 11500, so it does not touch an existing Ollama install.

```bash
python3 survey.py && python3 survey.py --summary          # library survey, a few minutes

export OHOME=~/.cache/ai-tool-lab/ollama-library-v1        # Ollama unpacked in $OHOME/ollama
HOME=$OHOME/home OLLAMA_MODELS=$OHOME/home/models OLLAMA_HOST=127.0.0.1:11500 $OHOME/ollama/bin/ollama serve &
for m in ministral-3:3b qwen3.5:2b gemma4:e2b llama-guard3:1b; do
  OLLAMA_HOST=127.0.0.1:11500 $OHOME/ollama/bin/ollama pull $m
done
kill %1

./run.sh                                 # ministral-3:3b, guard test, qwen3.5:2b
MODELS=gemma4:e2b GUARD=0 ./run.sh       # gemma4:e2b
python3 grade.py && python3 guard_grade.py
```

`LIMIT=2 ./run.sh` is a quick smoke test. `CONDS=api-t0,api-default,v1-default,v1-fix` runs all four bench conditions.

## What did not work

- `qwen3:1.7b` answered with a copy of the JSON Schema with the values inside `properties`, on both routes, so it
  could not show a difference. `qwen3:4b` wrote its reasoning into the reply despite `think: false` and hit the token
  limit (up to 8 minutes per reply on this machine). Both were dropped from the bench.
- A first run with four conditions per model was stopped after 9 replies (the machine was too busy for that plan) and
  discarded.

MIT license.
