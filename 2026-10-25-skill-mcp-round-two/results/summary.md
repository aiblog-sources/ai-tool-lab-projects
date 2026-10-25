| Condition | Runs | Searched with MCP | Searched with script | No search | Skill invoked | Skill then MCP | Tried to browse files | Right | Median turns | Median s | USD |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| sonnet-both | 48 | 20 | 24 | 4 | 28 | 4 | 1 | 48 | 4.0 | 9.1 | 1.61 |
| sonnet-bridge | 48 | 44 | 0 | 4 | 27 | 27 | 0 | 47 | 5.0 | 9.1 | 1.56 |
| sonnet-vague | 48 | 25 | 0 | 23 | 0 | 0 | 44 | 31 | 4.0 | 9.6 | 1.23 |
| sonnet-bridge-vague | 48 | 44 | 0 | 4 | 44 | 44 | 2 | 48 | 6.0 | 10.1 | 1.67 |
| sonnet-plugin | 48 | 34 | 10 | 4 | 10 | 0 | 2 | 48 | 4.0 | 9.2 | 1.55 |
| haiku-both | 48 | 23 | 22 | 3 | 22 | 0 | 13 | 45 | 4.0 | 10.2 | 0.11 |
| opus-both | 48 | 0 | 46 | 2 | 46 | 0 | 0 | 48 | 5.0 | 15.7 | 3.49 |
| haiku-bridge | 48 | 43 | 0 | 5 | 15 | 15 | 17 | 41 | 5.0 | 10.8 | 0.11 |
| opus-bridge | 48 | 46 | 0 | 2 | 46 | 46 | 0 | 48 | 6.0 | 14.4 | 3.56 |

By phrasing

| Condition | Phrasing | Runs | Searched with MCP | Searched with script | No search | Skill invoked | Skill then MCP | Tried to browse files | Right | Median turns | Median s | USD |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| sonnet-both | hinted | 24 | 17 | 7 | 0 | 11 | 4 | 0 | 24 | 4.0 | 9.0 | 0.74 |
| sonnet-both | bare | 24 | 3 | 17 | 4 | 17 | 0 | 1 | 24 | 4.0 | 9.2 | 0.87 |
| sonnet-bridge | hinted | 24 | 24 | 0 | 0 | 9 | 9 | 0 | 24 | 4.0 | 9.1 | 0.77 |
| sonnet-bridge | bare | 24 | 20 | 0 | 4 | 18 | 18 | 0 | 23 | 5.5 | 9.2 | 0.79 |
| sonnet-vague | hinted | 24 | 20 | 0 | 4 | 0 | 0 | 24 | 20 | 5.0 | 10.7 | 0.77 |
| sonnet-vague | bare | 24 | 5 | 0 | 19 | 0 | 0 | 20 | 11 | 2.0 | 8.2 | 0.46 |
| sonnet-bridge-vague | hinted | 24 | 24 | 0 | 0 | 24 | 24 | 0 | 24 | 5.5 | 10.2 | 0.80 |
| sonnet-bridge-vague | bare | 24 | 20 | 0 | 4 | 20 | 20 | 2 | 24 | 6.0 | 10.1 | 0.86 |
| sonnet-plugin | hinted | 24 | 22 | 2 | 0 | 2 | 0 | 0 | 24 | 4.0 | 8.9 | 0.71 |
| sonnet-plugin | bare | 24 | 12 | 8 | 4 | 8 | 0 | 2 | 24 | 4.0 | 9.3 | 0.84 |
| haiku-both | hinted | 24 | 16 | 8 | 0 | 8 | 0 | 1 | 23 | 4.0 | 9.6 | 0.05 |
| haiku-both | bare | 24 | 7 | 14 | 3 | 14 | 0 | 12 | 22 | 5.0 | 12.2 | 0.06 |
| opus-both | hinted | 24 | 0 | 24 | 0 | 24 | 0 | 0 | 24 | 5.0 | 15.8 | 1.73 |
| opus-both | bare | 24 | 0 | 22 | 2 | 22 | 0 | 0 | 24 | 5.0 | 14.6 | 1.75 |
| haiku-bridge | hinted | 24 | 24 | 0 | 0 | 4 | 4 | 1 | 22 | 4.0 | 9.9 | 0.05 |
| haiku-bridge | bare | 24 | 19 | 0 | 5 | 11 | 11 | 16 | 19 | 6.0 | 12.1 | 0.05 |
| opus-bridge | hinted | 24 | 24 | 0 | 0 | 24 | 24 | 0 | 24 | 6.0 | 14.4 | 1.84 |
| opus-bridge | bare | 24 | 22 | 0 | 2 | 22 | 22 | 0 | 24 | 6.0 | 14.8 | 1.72 |

By repetition (searched with MCP / script / none)

- sonnet-both: 10/12/2  10/12/2
- sonnet-bridge: 22/0/2  22/0/2
- sonnet-vague: 13/0/11  12/0/12
- sonnet-bridge-vague: 22/0/2  22/0/2
- sonnet-plugin: 18/4/2  16/6/2
- haiku-both: 12/10/2  11/12/1
- opus-both: 0/23/1  0/23/1
- haiku-bridge: 21/0/3  22/0/2
- opus-bridge: 23/0/1  23/0/1

Wrong answers

- sonnet-vague q2 bare first=None: "I can't answer this. The conversation doesn't include the CPU-only laptop test of 10 models, and my attempt to look for it in the working directory was denied. If you paste the results or give me the file path, I'll pick"
- sonnet-vague q3 bare first=None: "I can't answer this. Bash permission was denied twice, so I couldn't read the logs or any files in the working directory, and I have no other information about that OOM kill. I haven't guessed a figure. If you allow Bash"
- haiku-bridge q5 bare first=None: "I couldn't find the answer: my attempt to search the work-bridge directory was denied, so I have no results to read. If you point me to the file with the 120 replies or the run log, or allow a read-only search of that di"
- sonnet-vague q6 bare first=None: "I can't answer this one. My attempt to look through the working directory for the test results was denied, so I haven't seen any data on the small-model JSON test. I don't have the counts of the 120 tickets with all chec"
- haiku-both q6 bare first=None: "I couldn't find the answer: Bash calls to the work directory (`<work>/scratch/work-script`) were denied, so I haven't seen the test results. If you can allow reads there, or tell me the path to the results file, I'll cou"
- haiku-bridge q6 bare first=mcp: 'The note\'s search snippet cuts off mid-sentence, so I can\'t confirm the full split. It reports that 87 of 120 tickets had every checked field right, but the words "with" or "without" the grammar are cut off. The sentence'
- sonnet-vague q9 bare first=None: "I can't answer this. The search of the working directory for Hermes Agent material was denied, and I don't have a figure for the Hermes Agent gateway's idle RAM from memory. If you point me to a doc, log, or benchmark th"
- haiku-bridge q10 hinted first=mcp: 'The ai-memory recall test had 12 English questions, and the right note came first in 6 of them. I read this from the `mcp-memory-recall-test.md` note, and the table in it agrees with the summary.'
- sonnet-vague q10 bare first=None: "I can't answer this. Bash was denied on both of my attempts, a search for the ai-memory recall test and a listing of the working directory, so I couldn't open any results. I don't know how many questions the test had or "
- haiku-bridge q10 bare first=mcp: 'The ai-memory recall test had 12 English questions, and the right note came first in 6 of them (the summary and the hit@1 column agree). The post reports this for the notes at `mcp-memory-recall-test.md`.'
- sonnet-vague q11 bare first=None: "I couldn't answer from the files because my shell access to the working directory was denied, so I never found your test results. From memory, llama-server's prompt cache is controlled by `--cache-ram` (`-cram`), and `--"
- sonnet-vague q12 hinted first=None: "I couldn't check your notes. The shell command I tried to search the working directory for Ollama and llama.cpp entries was denied, and I have no other file-reading tool here. So I don't have the cold-start numbers and w"
- sonnet-vague q12 bare first=None: "I can't answer that because I don't have the benchmark numbers. You didn't include them in your message. I tried to look for them in the working directory, but my shell command was denied, so I haven't read any files.\n\nI"
- sonnet-vague q2 bare first=None: "I can't answer that. I have no record of the CPU-only laptop test of 10 models in this conversation, and my attempt to search the working directory for it was denied. If you paste the results or give me the file path, I'"
- haiku-bridge q3 bare first=mcp: 'I couldn\'t find the figure. The search snippets from the day-one post ("I scheduled Claude Code to run daily. Day one ran out of memory") don\'t state how much memory llama-server was using when the run was killed. The sn'
- sonnet-bridge q4 bare first=None: "Ollama's default context window is 2048 tokens in older versions. Newer releases choose it from available VRAM (4k for under 24 GB, 32k for 24–48 GB, 256k for 48 GB and up). I'm not sure how a CPU-only machine with no GP"
- sonnet-vague q6 hinted first=None: "I couldn't check your notes. The permission to run the shell command that would search the working directory was denied, and I have no other file-reading tool here. So I don't have the counts, and I won't guess them.\n\nIf"
- sonnet-vague q6 bare first=None: "I can't answer this yet. I tried to search the working directory for the test results, but the command was denied, so I haven't seen any data. If you give me the file path or paste the results, I'll pull out both counts "
- haiku-bridge q7 bare first=None: "I don't have a reliable figure for Hermes Agent's minimum context size for local models, and I haven't checked its docs or source here. Its README, configuration docs, or the model-setup section of its repository should "
- sonnet-vague q8 bare first=None: "I can't answer that. The shell command I tried to run to search the working directory was denied, so I have no data on when Hermes Agent's cron, systemd and cron fired. I won't guess a number.\n\nIf you paste the timing lo"
- sonnet-vague q10 hinted first=None: "I couldn't check your notes. The permission to run the shell command that searches the working directory was denied, and I haven't tried another route to the files. So I don't have the question count or the number of Eng"
- haiku-both q10 hinted first=mcp: "The ai-memory recall test had 12 questions, and the right note came first for 6 of the English ones. The post's summary and the per-question table agree on both numbers."
- haiku-bridge q10 hinted first=mcp: 'The ai-memory recall test had 12 questions, and the right note came first in 6 of the 12 English ones (the Portuguese run also had 12 questions, with 7 first-place hits). This comes from the "mcp-memory-recall-test.md" n'
- sonnet-vague q10 bare first=None: "I can't answer this yet. My search for the ai-memory recall test files was denied, so I haven't seen any results. If you give me the path to the results file or allow the search, I'll count the questions and how many Eng"
- haiku-both q10 bare first=mcp: 'The ai-memory recall test had 12 English questions, and the right note came first in 6 of them. This comes from the write-up "How well does an MCP memory server remember? I measured it" in your notes; I didn\'t have the r'
- sonnet-vague q11 bare first=None: "I couldn't find the RAM figure. The command to search the working directory was denied, so I haven't seen your test results, and I won't guess a number.\n\nOn the flag, from general knowledge of llama.cpp: `--cache-ram 0` "
- sonnet-vague q12 hinted first=None: "I couldn't check your notes. My attempt to search the working directory was denied, so I haven't read any files. I can't tell you how much longer Ollama took without making up a number.\n\nIf you approve the search, or tel"
- sonnet-vague q12 bare first=None: "I can't answer this one. My attempt to look through the working directory for benchmark data was denied, and nothing in our conversation includes cold-start, time-to-first-token numbers for Ollama or llama.cpp on a 2-cor"

Runs: 432, total USD 14.88, models ['claude-haiku-5-5', 'claude-opus-5-5', 'claude-sonnet-5-5'], mcp status ['connected']
