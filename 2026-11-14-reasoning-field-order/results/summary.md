# Summary

560 replies, 0 HTTP errors. Greedy decoding, GSM8K test problems from the start of the file.

| Server | Model | Condition | Right | Right incl. cut-off | Parsed | Key order in replies | Mean output tokens | Mean s/request | Hit max_tokens |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| llama-server-b11554 | Qwen 3.5 0.8B | free | 28/50 | 28/50 | 49/50 | (text) | 331 | 49.1 | 2 |
| llama-server-b11554 | Qwen 3.5 0.8B | answer_only | 2/50 | 2/50 | 50/50 | `answer` 50 | 12 | 4.6 | 0 |
| llama-server-b11554 | Qwen 3.5 0.8B | reasoning_first | 20/50 | 20/50 | 46/50 | `reasoning,answer` 46; `-` 4 | 226 | 32.9 | 4 |
| llama-server-b11554 | Qwen 3.5 0.8B | answer_first | 0/50 | 1/50 | 28/50 | `answer,reasoning` 28; `-` 22 | 554 | 80.4 | 22 |
| llama-server-b11554 | Qwen 3.5 0.8B | prefixed | 22/50 | 22/50 | 46/50 | `a_reasoning,b_answer` 46; `-` 4 | 226 | 33.2 | 4 |
| ollama-0.40.16 | Qwen 3.5 0.8B | answer_only | 1/50 | 1/50 | 50/50 | `answer` 50 | 12 | 4.5 | 0 |
| ollama-0.40.16 | Qwen 3.5 0.8B | reasoning_first | 1/50 | 2/50 | 32/50 | `answer,reasoning` 32; `-` 18 | 503 | 73.0 | 18 |
| ollama-0.40.16 | Qwen 3.5 0.8B | prefixed | 20/50 | 20/50 | 47/50 | `a_reasoning,b_answer` 47; `-` 3 | 216 | 32.3 | 3 |
| llama-server-b11554 | Qwen 3.5 2B | free | 15/20 | 15/20 | 19/20 | (text) | 418 | 136.6 | 1 |
| llama-server-b11554 | Qwen 3.5 2B | answer_only | 3/20 | 3/20 | 20/20 | `answer` 20 | 13 | 11.5 | 0 |
| llama-server-b11554 | Qwen 3.5 2B | reasoning_first | 15/20 | 15/20 | 20/20 | `reasoning,answer` 20 | 170 | 55.8 | 0 |
| llama-server-b11554 | Qwen 3.5 2B | answer_first | 3/20 | 3/20 | 19/20 | `answer,reasoning` 19; `-` 1 | 352 | 113.5 | 1 |
| llama-server-b11554 | Qwen 3.5 2B | prefixed | 13/20 | 13/20 | 19/20 | `a_reasoning,b_answer` 19; `-` 1 | 189 | 61.4 | 1 |
| ollama-0.40.16 | Qwen 3.5 2B | answer_only | 3/20 | 3/20 | 20/20 | `answer` 20 | 13 | 10.1 | 0 |
| ollama-0.40.16 | Qwen 3.5 2B | reasoning_first | 3/20 | 3/20 | 19/20 | `answer,reasoning` 19; `-` 1 | 338 | 106.1 | 1 |
| ollama-0.40.16 | Qwen 3.5 2B | prefixed | 15/20 | 15/20 | 19/20 | `a_reasoning,b_answer` 19; `-` 1 | 204 | 62.0 | 1 |

## Paired comparisons

b = right only in the first condition, c = right only in the second; p = exact two-sided sign test.

| Model | First | Second | Problems | Right (first / second) | b | c | p |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Qwen 3.5 0.8B | LS reasoning_first | LS answer_first | 50 | 20 / 0 | 20 | 0 | 0.000 |
| Qwen 3.5 0.8B | LS reasoning_first | LS answer_only | 50 | 20 / 2 | 19 | 1 | 0.000 |
| Qwen 3.5 0.8B | LS answer_first | LS answer_only | 50 | 0 / 2 | 0 | 2 | 0.500 |
| Qwen 3.5 0.8B | LS free | LS reasoning_first | 50 | 28 / 20 | 12 | 4 | 0.077 |
| Qwen 3.5 0.8B | LS reasoning_first | LS prefixed | 50 | 20 / 22 | 2 | 4 | 0.688 |
| Qwen 3.5 0.8B | Ollama prefixed | Ollama reasoning_first | 50 | 20 / 1 | 19 | 0 | 0.000 |
| Qwen 3.5 0.8B | Ollama prefixed | Ollama answer_only | 50 | 20 / 1 | 19 | 0 | 0.000 |
| Qwen 3.5 0.8B | Ollama reasoning_first | Ollama answer_only | 50 | 1 / 1 | 1 | 1 | 1.000 |
| Qwen 3.5 0.8B | Ollama reasoning_first | LS reasoning_first | 50 | 1 / 20 | 1 | 20 | 0.000 |
| Qwen 3.5 0.8B | Ollama prefixed | LS prefixed | 50 | 20 / 22 | 2 | 4 | 0.688 |
| Qwen 3.5 2B | LS reasoning_first | LS answer_first | 20 | 15 / 3 | 12 | 0 | 0.000 |
| Qwen 3.5 2B | LS reasoning_first | LS answer_only | 20 | 15 / 3 | 12 | 0 | 0.000 |
| Qwen 3.5 2B | LS answer_first | LS answer_only | 20 | 3 / 3 | 0 | 0 | 1.000 |
| Qwen 3.5 2B | LS free | LS reasoning_first | 20 | 15 / 15 | 1 | 1 | 1.000 |
| Qwen 3.5 2B | LS reasoning_first | LS prefixed | 20 | 15 / 13 | 3 | 1 | 0.625 |
| Qwen 3.5 2B | Ollama prefixed | Ollama reasoning_first | 20 | 15 / 3 | 12 | 0 | 0.000 |
| Qwen 3.5 2B | Ollama prefixed | Ollama answer_only | 20 | 15 / 3 | 12 | 0 | 0.000 |
| Qwen 3.5 2B | Ollama reasoning_first | Ollama answer_only | 20 | 3 / 3 | 0 | 0 | 1.000 |
| Qwen 3.5 2B | Ollama reasoning_first | LS reasoning_first | 20 | 3 / 15 | 0 | 12 | 0.000 |
| Qwen 3.5 2B | Ollama prefixed | LS prefixed | 20 | 15 / 13 | 3 | 1 | 0.625 |

## Answer written before the reasoning

Replies whose keys put the answer first (cut-off replies are not included here; see 'Right incl. cut-off'). 'Reasoning ends on the right number' = the last number in the reasoning text equals the gold answer; 'contradicts' = the reasoning's last number differs from the committed answer.

| Server | Model | Condition | Answer-first replies | Answer wrong | ...of those, reasoning ends on the right number | Reasoning's last number contradicts the answer |
| --- | --- | --- | --- | --- | --- | --- |
| llama-server-b11554 | Qwen 3.5 0.8B | answer_first | 28 | 28 | 19 | 24 |
| ollama-0.40.16 | Qwen 3.5 0.8B | reasoning_first | 32 | 31 | 17 | 30 |
| llama-server-b11554 | Qwen 3.5 2B | answer_first | 19 | 16 | 12 | 14 |
| ollama-0.40.16 | Qwen 3.5 2B | reasoning_first | 19 | 16 | 12 | 15 |
