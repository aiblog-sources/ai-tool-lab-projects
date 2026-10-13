# Results

llama.cpp b11554, 2 threads, Q4_K_M, CPU only. Speed from llama-bench (3 repetitions, mean and range).
Latency, memory and the sanity check from llama-server (`-c 4096 -np 1 --load-mode none`).

| Model | Params (B) | File (GiB) | Prompt t/s (pp512) | Generation t/s (tg128) | Load s | Peak RSS MiB | First token s (median) | Sanity check | Background CPU % |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Qwen 3.5 0.8B | 0.77 | 0.53 | 52.3 (50.3-55.6) | 15.1 (15.1-15.2) | 3.7 | 1,684 | 7.31 | 12/20 | 43.2 |
| Llama 3.2 1B | 1.24 | 0.74 | 32.6 (28.5-36.7) | 6.4 (5.3-8.6) | 4.3 | 1,103 | 14.62 | 14/20 | 30.5 |
| Qwen 2.5 1.5B | 1.54 | 0.91 | 20.5 (16.0-29.1) | 12.0 (11.6-12.3) | 3.5 | 1,240 | 17.22 | 15/20 | 42.2 |
| Qwen 3.5 2B | 1.94 | 1.29 | 25.0 (22.1-30.0) | 7.6 (7.6-7.7) | 5.0 | 2,432 | 17.46 | 14/20 | 54.0 |
| Gemma 4 E2B | 4.65 | 3.21 | 14.0 (13.1-15.8) | 4.9 (3.2-6.1) | 10.8 | 3,943 | 24.08 | 18/20 | 55.8 |
| SmolLM3 3B | 3.08 | 1.78 | 4.9 (4.7-5.0) | 1.9 (1.2-2.7) | 9.1 | 2,434 | 37.05 | 16/20 | 100.0 |
| Llama 3.2 3B | 3.21 | 1.87 | 11.8 (10.8-12.5) | 5.4 (5.2-5.7) | 5.3 | 2,759 | 32.84 | 16/20 | 55.0 |
| Phi-4 mini | 3.84 | 2.31 | 10.7 (9.8-12.3) | 4.1 (4.0-4.1) | 8.4 | 3,323 | 37.34 | 15/20 | 54.0 |
| Qwen 3.5 4B | 4.33 | 2.80 | 8.5 (8.2-8.9) | 3.4 (3.4-3.5) | 7.4 | 5,557 | 45.14 | 19/20 | 54.3 |
| Gemma 4 E4B | 7.52 | 5.02 | 9.6 (9.4-9.9) | 3.7 (3.7-3.8) | n/a | n/a | n/a | 20/20 | 27.8 |

## Sanity check, per question

| Question | Qwen 3.5 0.8B | Llama 3.2 1B | Qwen 2.5 1.5B | Qwen 3.5 2B | Gemma 4 E2B | SmolLM3 3B | Llama 3.2 3B | Phi-4 mini | Qwen 3.5 4B | Gemma 4 E4B |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| mult | FAIL | pass | pass | pass | pass | pass | pass | pass | pass | pass |
| train | FAIL | pass | FAIL | pass | pass | pass | pass | pass | pass | pass |
| pencils | FAIL | FAIL | FAIL | FAIL | pass | pass | pass | FAIL | pass | pass |
| percent | FAIL | pass | pass | FAIL | pass | pass | pass | pass | pass | pass |
| km | pass | FAIL | pass | FAIL | FAIL | pass | pass | pass | pass | pass |
| decimal | pass | FAIL | pass | pass | pass | pass | FAIL | pass | pass | pass |
| prime | pass | pass | pass | pass | FAIL | pass | pass | pass | pass | pass |
| weekday | FAIL | FAIL | FAIL | FAIL | pass | FAIL | pass | FAIL | FAIL | pass |
| capital | pass | pass | pass | pass | pass | FAIL | pass | pass | pass | pass |
| planet | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass |
| french | pass | pass | pass | pass | pass | pass | FAIL | pass | pass | pass |
| reverse | FAIL | FAIL | FAIL | FAIL | pass | FAIL | pass | FAIL | pass | pass |
| count_r | FAIL | pass | pass | FAIL | pass | FAIL | FAIL | FAIL | pass | pass |
| sort | pass | pass | FAIL | pass | pass | pass | pass | pass | pass | pass |
| email | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass |
| json | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass |
| sentiment | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass |
| len | FAIL | FAIL | pass | pass | pass | pass | FAIL | FAIL | pass | pass |
| sql | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass |
| bugfix | pass | pass | pass | pass | pass | pass | pass | pass | pass | pass |

## Interleaved speed rounds (speed_rounds.sh)

Each round runs every model once (pp256, tg64, 2 threads), so background load hits all of them alike.
Median of the rounds, with min-max.

| Model | Prompt t/s (pp256) | Generation t/s (tg64) | Rounds |
| --- | --- | --- | --- |
| Qwen 3.5 0.8B | 76.8 (63.2-77.6) | 18.9 (17.8-19.2) | 3 |
| Llama 3.2 1B | 48.6 (38.6-48.7) | 15.9 (14.5-16.2) | 3 |
| Qwen 2.5 1.5B | 31.1 (30.5-31.6) | 13.0 (11.7-13.2) | 3 |
| Qwen 3.5 2B | 26.0 (24.9-29.7) | 9.1 (8.8-9.2) | 3 |
| Gemma 4 E2B | 18.5 (18.2-21.0) | 7.1 (6.9-7.9) | 3 |
| SmolLM3 3B | 14.9 (12.8-16.4) | 6.2 (6.1-7.2) | 3 |
| Llama 3.2 3B | 14.6 (14.1-14.8) | 5.8 (5.8-6.7) | 3 |
| Phi-4 mini | 12.1 (11.5-12.4) | 5.3 (4.6-5.3) | 3 |
| Qwen 3.5 4B | 12.0 (10.2-12.1) | 4.2 (3.9-4.2) | 3 |
| Gemma 4 E4B | 11.2 (11.1-11.3) | 3.9 (3.8-4.2) | 3 |

## Memory with the prompt cache off (mem_check.sh, `--cache-ram 0`)

| Model | RSS after load MiB | Peak RSS MiB | Peak with default prompt cache MiB | Load s |
| --- | --- | --- | --- | --- |
| Qwen 3.5 0.8B | 707 | 765 | 1,684 | 2.3 |
| Llama 3.2 1B | 985 | 1,032 | 1,103 | 2.8 |
| Qwen 2.5 1.5B | 1,125 | 1,173 | 1,240 | 3.8 |
| Qwen 3.5 2B | 1,445 | 1,517 | 2,432 | 5.1 |
| Gemma 4 E2B | 3,778 | 3,880 | 3,943 | 7.4 |
| SmolLM3 3B | 2,204 | 2,269 | 2,434 | 6.6 |
| Llama 3.2 3B | 2,465 | 2,517 | 2,759 | 6.3 |
| Phi-4 mini | 2,996 | 3,043 | 3,323 | 6.8 |
| Qwen 3.5 4B | 3,052 | 3,168 | 5,557 | 9.1 |
| Gemma 4 E4B | 5,925 | 6,002 | killed at the 6 GB cap | 11.8 |
