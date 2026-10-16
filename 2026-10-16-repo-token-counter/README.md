# How many tokens is my codebase? A counter with real Claude numbers

**Finding:** current Claude models (Opus 4.7 and later, including Haiku, Sonnet and Opus 5.5) use **1.58 tokens
for every OpenAI `o200k_base` token on source code**, measured on 20 files from 5 public repos. Tools that count with
`o200k_base` (Repomix's default) or with "characters divided by 4" undercount a Claude prompt by about a third.
On two repos the ratios were not measured on, this counter's estimate landed within +7.3% and +0.2% of the real count.

Write-up: https://aitoollabs.dev/posts/count-tokens-in-codebase/

| Sample (Sonnet 5.5 vs Sonnet 4.6) | o200k | Claude 4.7+ | 4.7+ / o200k | Claude 4.6 | 4.6 / o200k | 4.7+ / 4.6 |
| --- | --- | --- | --- | --- | --- | --- |
| 20 code files (Go, JS, Python, Rust, TS) | 45,006 | 71,330 | 1.58 | 55,610 | 1.24 | 1.28 |
| 5 Markdown and reST files | 44,621 | 61,523 | 1.38 | 49,027 | 1.10 | 1.25 |
| 5 config files (YAML, JSON, TOML) | 3,391 | 4,829 | 1.42 | 3,887 | 1.15 | 1.24 |
| 1 SVG logo | 35,199 | 38,119 | 1.08 | 35,526 | 1.01 | 1.07 |

Haiku 5.5, Sonnet 5.5, Opus 5.5 and Opus 4.7 returned identical counts for all 33 files. Full tables:
[`results/summary.md`](results/summary.md).

## What it does

`repotokens.py` walks a repository (tracked files only when it is a git checkout, so `.gitignore` applies), skips
binaries, and prints:

- tokens per file type with OpenAI's `o200k_base` (exact for the GPT-5 family),
- an estimate for Claude 4.7 and later and for Claude 4.6 and earlier, from the measured ratios in
  [`claude_ratios.json`](claude_ratios.json),
- the largest files and their share (one SVG logo is 77% of the `requests` repo),
- what sending the repo once as input costs on 8 models, with long-context prices and a context-window check, from
  [`prices.json`](prices.json) (official pricing pages, checked 2026-10-16).

```
$ python repotokens.py ~/src/itsdangerous
...
Tokens: o200k_base (GPT-5 family) 110,978,
        Claude 4.7 and later (estimate) 155,311, Claude 4.6 and earlier (estimate) 132,298, chars/4 rule 70,635

Largest files (o200k tokens):
     74,518   67.1%  uv.lock  (lockfile)
      6,984    6.3%  docs/_static/itsdangerous-name.svg
  ...
  One file is over a quarter of the total; if a model does not need it, try --exclude 'uv.lock' or --no-lockfiles.
```

Options: `--no-lockfiles`, `--exclude GLOB` (repeatable), `--hf-tokenizer path/to/tokenizer.json` to count with an
open model's tokenizer too, `--json` for totals only.

## How to run it

Python 3.10 or newer.

```bash
python -m venv venv
./venv/bin/pip install -r requirements.txt
./venv/bin/python repotokens.py /path/to/repo
```

The first run downloads the `o200k_base` file once (tiktoken caches it). Counting ripgrep (228 text files, 3.2 million
characters) took 3.4 s on a 2-core laptop CPU that was busy with other work.

## How the Claude numbers were measured

Anthropic does not publish an offline tokenizer for current Claude models. `claude -p --output-format json` reports
the input tokens of each run, so `measure_claude.py` sends a file followed by a short question and subtracts a run
with the question alone. The system prompt is replaced with one line and all tools are off, so the baseline is small
(450 to 606 tokens depending on the model) and was identical before and after every batch.

```bash
# needs Claude Code logged in; sends each file once per model
./venv/bin/python measure_claude.py results/claude_counts.csv repos/requests repos/express repos/ky repos/cobra repos/ripgrep
./venv/bin/python measure_claude.py results/claude_counts_extra.csv repos/ky::media/logo.svg repos/ripgrep::Cargo.lock ...
./venv/bin/python validate_claude.py results/validation.json repos/itsdangerous repos/hexyl
./venv/bin/python analyze.py qwen3/tokenizer.json xenova-claude/tokenizer.json repos/
./venv/bin/python repo_totals.py results/repo_files.csv qwen3/tokenizer.json deepseek-v3/tokenizer.json repos/*
```

One gotcha cost a full pass: a prompt that starts with `/` (any file opening with a `//` comment) makes `claude -p`
send about 300 fewer tokens of its own context, so the subtraction came out wrong for those files (one 1,000-character Go file
came out at 122 tokens instead of 427, a license header alone at minus 87 tokens). Every prompt now starts with a fixed line. The bad pass
is kept in [`results/discarded/`](results/discarded/).

Repos used (shallow clones, commit in brackets): psf/requests (611c616), expressjs/express (a419aa7),
sindresorhus/ky (3541888), spf13/cobra (adbc881), BurntSushi/ripgrep (3fce3b5); held out for the check:
pallets/itsdangerous (672971d), sharkdp/hexyl (6ecc29b). Only token counts are stored here, not their code.

## Files

| File | What it is |
| --- | --- |
| `repotokens.py` | The CLI |
| `prices.json` | Input prices per million tokens, with sources and the date checked |
| `claude_ratios.json` | Claude tokens per `o200k_base` token by file type (written by `analyze.py`) |
| `measure_claude.py` | Counts real Claude tokens for files through `claude -p` |
| `validate_claude.py` | Sends whole repos to Claude to check the estimate |
| `repo_totals.py` | Counts every file of the repos with o200k, cl100k, Qwen3 and DeepSeek-V3 |
| `analyze.py` | Builds `claude_ratios.json` and `results/summary.md` |
| `results/` | Raw CSV and JSON from the runs, CLI outputs |

## Limits

- 33 files and 2 held-out repos. The ratios are averages; single files ranged from 1.08 (SVG) to 1.84 (a Go file).
- Lockfiles are estimated from one `Cargo.lock`; `uv.lock` in itsdangerous is the likely reason that estimate came
  out 7.3% high.
- GPT-5.4 and Gemini counts use `o200k_base`. tiktoken 0.12.0 maps the GPT-5 family to it; newer GPT and Gemini
  tokenizers are not published offline, so treat those costs as approximate.
- Prices change. Check `prices.json` against the source pages before relying on a number.

MIT license (see the repository root).
