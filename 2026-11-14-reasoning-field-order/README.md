# Reasoning before the answer in a JSON schema: does the order matter, and does Ollama keep it?

**Finding:** with a grammar enforcing the schema, the position of a `reasoning` field decided the score. Qwen 3.5 0.8B
solved 20 of 50 GSM8K problems with `{"reasoning", "answer"}` and 0 of 50 with `{"answer", "reasoning"}` (2 of 50 with
no reasoning field at all); Qwen 3.5 2B solved 15 of 20 against 3 of 20. Ollama 0.40.16 sorts schema keys
alphabetically ([ollama/ollama#18717](https://github.com/ollama/ollama/issues/18717)), so a schema declared
reasoning-first is generated answer-first: the 0.8B solved 1 of 50 with it on Ollama, the 2B 3 of 20. Naming the keys `a_reasoning` and `b_answer` survives
the sort.

Write-up: https://aitoollabs.dev/posts/json-schema-reasoning-field-order/

Hardware: a 2-core, 4-thread laptop CPU (15 W part), 19 GB RAM, no GPU used. Run on 2026-11-14 with upstream
llama.cpp b11554 (`llama-server`) and Ollama 0.40.16 (official Linux build, CPU libraries only). Two parallel slots,
2 threads, 6 GB memory cap, 8,192 context on both servers. Other programs used 30% to 41% of total CPU during the
runs (`results/server.tsv`), so the seconds are only comparable between conditions of the same run.

## Results

Greedy, one run per problem, the first 50 GSM8K test problems for the 0.8B and the first 20 for the 2B. "Right incl.
cut-off" also credits replies that hit the 1,024-token limit after the answer was already written.

| Server | Condition | Keys as generated | 0.8B right (of 50) | 0.8B incl. cut-off | 0.8B cut off | 2B right (of 20) |
| --- | --- | --- | --- | --- | --- | --- |
| llama-server b11554 | `free` (plain text) | (text) | 28 | 28 | 2 | 15 |
| llama-server b11554 | `answer_only` | `answer` | 2 | 2 | 0 | 3 |
| llama-server b11554 | `reasoning_first` | `reasoning, answer` | **20** | 20 | 4 | **15** |
| llama-server b11554 | `answer_first` | `answer, reasoning` | **0** | 1 | 22 | **3** |
| llama-server b11554 | `prefixed` | `a_reasoning, b_answer` | 22 | 22 | 4 | 13 |
| Ollama 0.40.16 | `answer_only` | `answer` | 1 | 1 | 0 | 3 |
| Ollama 0.40.16 | `reasoning_first` (declared) | `answer, reasoning` | **1** | 2 | 18 | **3** |
| Ollama 0.40.16 | `prefixed` | `a_reasoning, b_answer` | **20** | 20 | 3 | **15** |

Paired by problem (exact sign test): reasoning-first beat answer-first 20 to 0 (0.8B) and 12 to 0 (2B) on
llama-server; on Ollama the prefixed keys beat the declared reasoning-first schema 19 to 0 and 12 to 0. The prefixed
control moved the llama-server score by 2 problems on both models, which is the noise level of this test.

In the answer-first replies that finished, the answer was usually wrong while the reasoning written after it was not:
on llama-server the 0.8B got 28 of 28 such answers wrong and its reasoning ended on the right number in 19; the 2B
got 16 of 19 wrong, with the reasoning ending right in 12. Cut-off answer-first replies were the model re-checking an
answer it had already written ("re-read" appears in 19 of the 0.8B's 22).

`results/server.tsv` minutes cover only the part of each stage after the last restart (the 0.8B llama-server stage
was restarted once at 50 problems instead of 100, and the 2B stage once at 20 instead of 50).

Per condition, paired sign tests and the answer-first analysis: [`results/summary.md`](results/summary.md). Every
reply: `results/raw.jsonl`. Key order on both Ollama APIs: [`results/order-probe.txt`](results/order-probe.txt).

## What it does

`bench.py` sends the first N problems of the GSM8K test set to an OpenAI-compatible `/v1/chat/completions` endpoint
under these conditions (greedy, seed 1234, thinking off via `reasoning_effort: none` and `--reasoning off`, at most
1,024 output tokens):

| Condition | `response_format` schema properties, in declared order |
| --- | --- |
| `free` | none; the prompt asks for step-by-step reasoning and a last line `Answer: <number>` |
| `answer_only` | `answer` (number) |
| `reasoning_first` | `reasoning` (string), `answer` (number) |
| `answer_first` | `answer` (number), `reasoning` (string) |
| `prefixed` | `a_reasoning` (string), `b_answer` (number) |

The system prompt shows the schema as declared and asks for the keys in that order. A reply is right when its answer
equals GSM8K's final number. `summarize.py` also credits replies cut off at the token limit when the answer was
already written ("right incl. cut-off"), records the key order of every reply, and for answer-first replies checks
whether the reasoning written after the answer ends on the right number.

`probe_order.py` sends the reasoning-first and prefixed schemas to a running Ollama through both `/v1`
(`response_format`) and the native `/api/chat` (`format`) and prints the key order of the reply.

## How to run it

Needs Python 3 (standard library only), a llama.cpp build with `llama-server`, an Ollama binary, and the two GGUF
files from `models.tsv` (bartowski's Q4_K_M). `run.sh` expects them under `vendor/` and `models/` and the GSM8K test
file at `data/test.jsonl`:

```bash
mkdir -p data && curl -sL -o data/test.jsonl \
  https://raw.githubusercontent.com/openai/grade-school-math/master/grade_school_math/data/test.jsonl
SERVER=llama-server ONLY="Qwen 3.5 0.8B" LIMIT=50 ./run.sh
SERVER=ollama ONLY="Qwen 3.5 0.8B" LIMIT=50 OL_CONDS=answer_only,reasoning_first,prefixed ./run.sh
python3 summarize.py            # writes results/summary.md
```

`run.sh` is resumable: requests already in `results/raw.jsonl` are skipped.

## Limits

- Qwen 3.5 2B ran on the first 20 problems only (about 3 minutes per problem across the five conditions on this CPU).
- Greedy, one run per problem. The `prefixed` control (same schema, other key names) moved the score by 2 problems
  on both models, so differences that small are noise.
- GSM8K is public and old; the models may have seen it. Plain-text reasoning still beat answer-only by 28 to 2 on
  the 0.8B.
- One task and one model family, thinking off.

GSM8K is by OpenAI, MIT license (https://github.com/openai/grade-school-math). This project's code is MIT.
