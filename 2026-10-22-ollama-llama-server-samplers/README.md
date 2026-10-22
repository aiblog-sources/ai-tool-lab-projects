# Ollama vs llama-server sampler defaults on a JSON task

**Finding:** when a client sends no sampler settings, Ollama's OpenAI-compatible endpoint (`/v1/chat/completions`)
samples at temperature 1.0 and top_p 1.0, while Ollama's native `/api/chat` uses 0.8 and 0.9 and llama-server uses
0.8, top_p 0.95 and min_p 0.05. On a JSON extraction task with two small Qwen 3.5 models, that cost validity:
67 of 80 replies passed the schema through `/v1`, against 77 and 79 of 80 through the other two. With only
`temperature: 1.5` sent, `/v1` fell to 39 of 80, while `/api/chat` and llama-server stayed at 73 and 75.

Write-up: https://aitoollabs.dev/posts/ollama-openai-api-default-temperature/

Hardware: Intel Core i5-7200U (2 cores, 4 threads, 15 W laptop part), 19 GB RAM, no GPU used. Run on 2026-10-22 with
Ollama 0.40.3 (which runs its own bundled llama-server) and upstream llama.cpp b11554, 2 threads, 2 parallel slots
on both. Other programs used 35% to 61% of total CPU when each server started.

## What each route really samples with

Read from the `sampler params` line each server logs per request (`results/effective-params.txt`), same GGUF, no
`PARAMETER` lines in the Ollama Modelfile:

| Server and route | Client sends | temperature | top_k | top_p | min_p |
| --- | --- | --- | --- | --- | --- |
| llama-server `/v1/chat/completions` | nothing | 0.8 | 40 | 0.95 | 0.05 |
| Ollama `/api/chat` | nothing | 0.8 | 40 | 0.9 | 0 |
| Ollama `/v1/chat/completions` | nothing | **1.0** | 40 | **1.0** | 0 |
| llama-server `/v1/chat/completions` | temperature 1.5 | 1.5 | 40 | 0.95 | 0.05 |
| Ollama `/api/chat` | temperature 1.5 | 1.5 | 40 | 0.9 | 0 |
| Ollama `/v1/chat/completions` | temperature 1.5 | 1.5 | 40 | **1.0** | 0 |

Repeat, presence and frequency penalties were off (1.0 / 0 / 0) in every case. Ollama's OpenAI layer sets
`temperature` and `top_p` to 1.0 whenever the request leaves them out
([`openai/openai.go`](https://github.com/ollama/ollama/blob/31547a6/openai/openai.go#L751-L773)).

## Results

20 support emails per model, Qwen 3.5 0.8B and Qwen 3.5 2B (bartowski's Q4_K_M). Thinking off, at most 300 output
tokens, the JSON Schema in the system prompt and nothing constraining the output. Temperature 0 runs once per email,
every sampled condition twice, each request with its own seed. "Valid" means the reply passes `schema.json` after
removing a Markdown fence. "Right" means all five graded fields match the labels.

| Condition | Effective samplers | Valid | All 5 fields right |
| --- | --- | --- | --- |
| llama-server, temperature 0 | greedy | 39/40 | 33/40 |
| Ollama `/api/chat`, temperature 0 | greedy | 39/40 | 32/40 |
| llama-server, nothing sent | 0.8 / top_p 0.95 / min_p 0.05 | 79/80 | 53/80 |
| Ollama `/api/chat`, nothing sent | 0.8 / top_p 0.9 | 77/80 | 56/80 |
| Ollama `/v1`, nothing sent | 1.0 / top_p 1.0 | 67/80 | 41/80 |
| llama-server, temperature 1.5 | 1.5 / top_p 0.95 / min_p 0.05 | 75/80 | 48/80 |
| Ollama `/api/chat`, temperature 1.5 | 1.5 / top_p 0.9 | 73/80 | 45/80 |
| Ollama `/v1`, temperature 1.5 | 1.5 / top_p 1.0 | 39/80 | 21/80 |
| Ollama `/api/chat`, 1.5 + top_p 0.95 + min_p 0.05 | same as llama-server | 74/80 | 47/80 |

Per model (valid / right):

| Condition | Qwen 3.5 0.8B | Qwen 3.5 2B |
| --- | --- | --- |
| llama-server, nothing sent | 39/40, 23/40 | 40/40, 30/40 |
| Ollama `/api/chat`, nothing sent | 37/40, 21/40 | 40/40, 35/40 |
| Ollama `/v1`, nothing sent | 33/40, 14/40 | 34/40, 27/40 |
| llama-server, temperature 1.5 | 37/40, 19/40 | 38/40, 29/40 |
| Ollama `/api/chat`, temperature 1.5 | 34/40, 16/40 | 39/40, 29/40 |
| Ollama `/v1`, temperature 1.5 | 18/40, 5/40 | 21/40, 16/40 |
| Ollama `/api/chat`, llama-server's samplers at 1.5 | 35/40, 14/40 | 39/40, 33/40 |

Other findings:
- **Same engine underneath.** At temperature 0, Ollama and upstream llama-server gave the same text for 37 of 40
  emails and the same parsed object for 38 of 40 (one difference was `Coffee Mug` vs `coffee mug`).
- **A Modelfile does not fix `/v1`.** With `PARAMETER temperature 0.2` and `PARAMETER top_p 0.8` in the Modelfile,
  `/v1` still sampled at 1.0 and 1.0. `top_k`, `min_p`, `repeat_penalty` and `presence_penalty` from the same
  Modelfile did reach the sampler (`results/modelfile-params.txt`). Sending `temperature` or `top_p` in the request
  works.
- **top_k 40 alone does not hold the line.** Every Ollama condition kept top_k 40; what separated `/v1` from
  `/api/chat` at temperature 1.5 was top_p 1.0 against 0.9.
- At temperature 1.5 through `/v1`, Qwen 3.5 0.8B twice opened a `<think>` block although thinking was off, once
  with a full reasoning text that used up the token limit.

Per-field accuracy, failure kinds and every miss: [`results/summary.md`](results/summary.md).

## What is in here

| File | What it does |
| --- | --- |
| `inputs.jsonl`, `schema.json` | The 20 labelled emails and the ticket schema from [the temperature 0 test](../2026-10-14-small-llm-json-output/). |
| `models.tsv` | GGUF files (the run used the two Qwen rows; `ONLY=` picks one). |
| `run.sh` | Per model: starts llama-server, records its sampler parameters, runs its conditions, stops it; then the same with a private Ollama. 6 GB memory cap on each. |
| `bench.py` | Standard library only. Sends the requests with as few sampler fields as each condition allows; `--probe` records what the server logged. |
| `modelfile_probe.sh` | Imports the model with six `PARAMETER` lines and records which ones each Ollama route uses. |
| `grade.py` | Standard library only. Parses, validates and checks the labels; writes `summary.md` and `graded.jsonl`. |
| `results/` | `raw.jsonl` (640 replies), `graded.jsonl`, `summary.md`, `effective-params.txt`, `modelfile-params.txt`, `server.tsv` (background CPU, peak RAM, minutes per server). |

## How to run it

Needs Linux x86-64, Python 3, `curl`, `bc`, systemd (for the memory cap), the llama.cpp build, Ollama 0.40.3 and the
GGUF files from [`../2026-10-12-ollama-vs-llama-cpp-cpu/setup.sh`](../2026-10-12-ollama-vs-llama-cpp-cpu/setup.sh) and
[`../2026-10-13-small-llms-laptop-cpu/setup.sh`](../2026-10-13-small-llms-laptop-cpu/setup.sh). Link their `vendor/`
and `models/` folders here, then:

```sh
ONLY="Qwen 3.5 0.8B" ./run.sh   # about 40 minutes on the laptop above
ONLY="Qwen 3.5 2B" ./run.sh     # about 62 minutes
./modelfile_probe.sh
python3 grade.py
```

`LIMIT=1 REPS=1` sends one email once per condition (smoke test).

## Limits

- Two models from one family and one task. Last week's test found Gemma 4 E2B barely affected by sampling settings,
  so the size of the effect depends on the model. I did not run Gemma here.
- 40 sampled replies per model and cell. Two conditions with identical sampler settings (llama-server at 1.5 and
  Ollama `/api/chat` with llama-server's samplers at 1.5) differed by up to 5 right answers per model, so
  differences of that size are noise. The `/v1` drop at 1.5 is not.
- Ollama 0.40.3 was current on this machine; the latest release is 0.40.8. The code that sets the `/v1` defaults is
  unchanged on Ollama's main branch as of commit `31547a6`.
- Ollama library models ship their own `PARAMETER` lines (for example temperature 1 and top_p 0.95); `/v1` keeps
  their temperature at 1.0 by coincidence but still replaces top_p with 1.0. I tested imported GGUF files only.

MIT license.
