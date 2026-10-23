# What the Ollama library ships, and what /v1 replaces

Library page read in popular order: 260 models. 21 have no `latest` tag (mostly cloud-only) and 14 are embedding models; that leaves 225 generative models, read at their `latest` tag.

| Count | Of generative models |
| --- | --- |
| Ship a temperature or top_p (replaced by 1.0 on /v1 when the client sends none) | 59/225 |
| Ship a temperature below 1 | 35/225 |
| Ship temperature 0 | 9/225 |
| Ship a top_p below 1 | 36/225 |
| Of the 50 most popular entries: ship temperature or top_p | 15/44 |

## Every model that ships a temperature or top_p

| Rank | Model | Pulls | Shipped temperature | Shipped top_p | /v1 with nothing sent |
| --- | --- | --- | --- | --- | --- |
| 2 | deepseek-r1 | 94M | 0.6 | 0.95 | 1.0 / 1.0 |
| 6 | gemma3 | 41.3M | 1 | 0.95 | 1.0 / 1.0 |
| 7 | qwen3 | 41.2M | 0.6 | 0.95 | 1.0 / 1.0 |
| 10 | gemma4 | 27.6M | 1 | 0.95 | 1.0 / 1.0 |
| 12 | qwen3.5 | 23.1M | 1 | 0.95 | 1.0 / 1.0 |
| 17 | gpt-oss | 14.1M | 1 | - | 1.0 / 1.0 |
| 18 | qwen3-coder | 10.2M | 0.7 | 0.8 | 1.0 / 1.0 |
| 19 | glm-ocr | 8.5M | 0 | - | 1.0 / 1.0 |
| 26 | qwen3.6 | 7.3M | 1 | 0.95 | 1.0 / 1.0 |
| 27 | qwen3-vl | 6.8M | 1 | 0.95 | 1.0 / 1.0 |
| 32 | qwen2.5vl | 5.5M | 0.0001 | - | 1.0 / 1.0 |
| 33 | llama3.2-vision | 5.5M | 0.6 | 0.9 | 1.0 / 1.0 |
| 35 | qwen3.8 | 4.7M | 1 | 0.95 | 1.0 / 1.0 |
| 46 | mistral-small | 3.2M | 0.15 | - | 1.0 / 1.0 |
| 50 | nemotron-3-super | 3M | 1 | 0.95 | 1.0 / 1.0 |
| 53 | translategemma | 2.7M | - | 0.95 | 1.0 / 1.0 |
| 56 | mistral-small3.2 | 2.5M | 0.15 | - | 1.0 / 1.0 |
| 60 | qwq | 2.3M | 0.6 | 0.95 | 1.0 / 1.0 |
| 61 | smollm | 2.3M | 0.2 | 0.9 | 1.0 / 1.0 |
| 65 | devstral-small-2 | 2.1M | 0.15 | - | 1.0 / 1.0 |
| 66 | glm-4.7-flash | 2.1M | 1 | 0.95 | 1.0 / 1.0 |
| 67 | qwen3-coder-next | 2.1M | 1 | 0.95 | 1.0 / 1.0 |
| 68 | moondream | 2.1M | 0 | - | 1.0 / 1.0 |
| 83 | ministral-3 | 1.5M | 0.15 | - | 1.0 / 1.0 |
| 85 | magistral | 1.5M | 0.7 | 0.95 | 1.0 / 1.0 |
| 92 | lfm2.5-thinking | 1.3M | 0.05 | - | 1.0 / 1.0 |
| 93 | deepscaler | 1.3M | 0.6 | 0.95 | 1.0 / 1.0 |
| 105 | lfm2 | 1.1M | 0.3 | - | 1.0 / 1.0 |
| 115 | llama-guard3 | 1.1M | 0 | - | 1.0 / 1.0 |
| 129 | granite3.2-vision | 1M | 0 | - | 1.0 / 1.0 |
| 135 | shieldgemma | 974.8K | 0 | - | 1.0 / 1.0 |
| 137 | deepcoder | 967.3K | 0.6 | 0.95 | 1.0 / 1.0 |
| 142 | nemotron-3-nano | 936.7K | 1 | 1 | 1.0 / 1.0 |
| 149 | exaone-deep | 783.8K | 0.6 | 0.95 | 1.0 / 1.0 |
| 153 | deepseek-v3.1 | 740.3K | 0.6 | 0.95 | 1.0 / 1.0 |
| 159 | ornith | 660.5K | 0.6 | 0.95 | 1.0 / 1.0 |
| 161 | reflection | 632.6K | 0.7 | 0.95 | 1.0 / 1.0 |
| 165 | qwen3-next | 604.6K | 0.6 | 0.95 | 1.0 / 1.0 |
| 168 | exaone3.5 | 584.8K | 1 | - | 1.0 / 1.0 |
| 174 | deepseek-ocr | 562.1K | 0 | - | 1.0 / 1.0 |
| 176 | falcon2 | 556.9K | 0 | - | 1.0 / 1.0 |
| 179 | bespoke-minicheck | 547.2K | 0 | - | 1.0 / 1.0 |
| 186 | rnj-1 | 518.5K | 0.2 | - | 1.0 / 1.0 |
| 191 | olmo-3 | 474.5K | 0.6 | 0.95 | 1.0 / 1.0 |
| 203 | devstral-2 | 367.7K | 0.15 | - | 1.0 / 1.0 |
| 204 | granite3-guardian | 349.2K | 0 | - | 1.0 / 1.0 |
| 207 | muse-glimmer | 321.8K | 1 | 0.95 | 1.0 / 1.0 |
| 208 | olmo-3.1 | 311.5K | 0.6 | 0.95 | 1.0 / 1.0 |
| 213 | lfm2.5 | 263.4K | 0.2 | - | 1.0 / 1.0 |
| 214 | nemotron-3.5-lightning | 256.4K | 1 | 0.95 | 1.0 / 1.0 |
| 222 | functiongemma | 204.1K | - | 0.95 | 1.0 / 1.0 |
| 225 | granite4.2 | 173.7K | 1 | 0.95 | 1.0 / 1.0 |
| 226 | gpt-oss-safeguard | 168.9K | 1 | 1 | 1.0 / 1.0 |
| 228 | nemotron-cascade-2 | 157.3K | 1 | 0.95 | 1.0 / 1.0 |
| 235 | north-mini-code-1.0 | 78.7K | 1 | 0.95 | 1.0 / 1.0 |
| 240 | bonsai-2 | 45K | 1 | 0.95 | 1.0 / 1.0 |
| 250 | nemotron-3.5-super | 13K | 1 | 0.95 | 1.0 / 1.0 |
| 253 | flint | None | 1 | 0.95 | 1.0 / 1.0 |
| 255 | nemotron-3.5-bolt | None | 1 | 0.95 | 1.0 / 1.0 |
