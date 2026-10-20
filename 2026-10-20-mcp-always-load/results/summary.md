
## claude-haiku-5-5: 96 runs

| Condition | Phrasing | Runs | Called the MCP tool | ToolSearch before it | Right answer | Median first-request input tokens | Median wall s | Median cost (USD) |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| good | hinted | 12 | 12 | 12 | 11 | 10542 | 6.7 | 0.0021 |
| good | bare | 12 | 10 | 9 | 12 | 10536 | 8.4 | 0.0023 |
| good | both | 24 | 22 | 21 | 23 | 10540 | 6.8 | 0.0022 |
| good+tool | hinted | 12 | 12 | 0 | 12 | 10758 | 6.7 | 0.0024 |
| good+tool | bare | 12 | 11 | 0 | 12 | 10752 | 8.1 | 0.0021 |
| good+tool | both | 24 | 23 | 0 | 24 | 10756 | 7.1 | 0.0023 |
| vague | hinted | 12 | 11 | 11 | 12 | 10538 | 10.8 | 0.0038 |
| vague | bare | 12 | 8 | 8 | 10 | 10532 | 10.1 | 0.0029 |
| vague | both | 24 | 19 | 19 | 22 | 10536 | 10.3 | 0.0030 |
| vague+tool | hinted | 12 | 12 | 0 | 11 | 10694 | 6.5 | 0.0020 |
| vague+tool | bare | 12 | 11 | 0 | 11 | 10688 | 7.4 | 0.0019 |
| vague+tool | both | 24 | 23 | 0 | 22 | 10692 | 6.9 | 0.0019 |

Right without the tool: [('haiku.jsonl', 'vague', 'hinted', 4), ('haiku.jsonl', 'good', 'bare', 4), ('haiku.jsonl', 'good+tool', 'bare', 4), ('haiku.jsonl', 'vague', 'bare', 4), ('haiku.jsonl', 'vague+tool', 'bare', 4), ('haiku.jsonl', 'good', 'bare', 7), ('haiku.jsonl', 'vague', 'bare', 7)]
Tool called, answer wrong: [('haiku.jsonl', 'vague+tool', 'bare', 9, 'I couldn\'t find the exact idle figure. The search only returns snippets from the post "Her'), ('haiku.jsonl', 'good', 'hinted', 10, 'There were 12 questions in the ai-memory recall test, and the right note came first in 6 o'), ('haiku.jsonl', 'vague+tool', 'hinted', 10, "I couldn't confirm the total number of questions. The search tool only returns short excer")]
Server not connected: []
Built-in tools used: ['Bash', 'Edit', 'Glob', 'Grep', 'Read', 'Write']
Turns, median by condition: {'good': 4.0, 'good+tool': 3.5, 'good+server': 0, 'good+off': 0, 'vague': 5.5, 'vague+tool': 3.0, 'vague+off': 0}
Total cost USD: 0.23  wall min: 13.6
Bare phrasing, MCP used per question:
 q1 {'good': '1/1', 'good+tool': '1/1', 'vague': '0/1', 'vague+tool': '1/1'}
 q2 {'good': '1/1', 'good+tool': '1/1', 'vague': '0/1', 'vague+tool': '1/1'}
 q3 {'good': '1/1', 'good+tool': '1/1', 'vague': '1/1', 'vague+tool': '1/1'}
 q4 {'good': '0/1', 'good+tool': '0/1', 'vague': '0/1', 'vague+tool': '0/1'}
 q5 {'good': '1/1', 'good+tool': '1/1', 'vague': '1/1', 'vague+tool': '1/1'}
 q6 {'good': '1/1', 'good+tool': '1/1', 'vague': '1/1', 'vague+tool': '1/1'}
 q7 {'good': '0/1', 'good+tool': '1/1', 'vague': '0/1', 'vague+tool': '1/1'}
 q8 {'good': '1/1', 'good+tool': '1/1', 'vague': '1/1', 'vague+tool': '1/1'}
 q9 {'good': '1/1', 'good+tool': '1/1', 'vague': '1/1', 'vague+tool': '1/1'}
 q10 {'good': '1/1', 'good+tool': '1/1', 'vague': '1/1', 'vague+tool': '1/1'}
 q11 {'good': '1/1', 'good+tool': '1/1', 'vague': '1/1', 'vague+tool': '1/1'}
 q12 {'good': '1/1', 'good+tool': '1/1', 'vague': '1/1', 'vague+tool': '1/1'}

## claude-sonnet-5-5: 336 runs

| Condition | Phrasing | Runs | Called the MCP tool | ToolSearch before it | Right answer | Median first-request input tokens | Median wall s | Median cost (USD) |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| good | hinted | 24 | 24 | 24 | 24 | 9764 | 11.7 | 0.0229 |
| good | bare | 24 | 20 | 20 | 24 | 9758 | 10.8 | 0.0369 |
| good | both | 48 | 44 | 44 | 48 | 9762 | 11.2 | 0.0294 |
| good+tool | hinted | 24 | 24 | 0 | 24 | 9980 | 7.5 | 0.0306 |
| good+tool | bare | 24 | 24 | 0 | 24 | 9974 | 7.0 | 0.0262 |
| good+tool | both | 48 | 48 | 0 | 48 | 9978 | 7.2 | 0.0287 |
| good+server | hinted | 24 | 24 | 0 | 24 | 10092 | 8.6 | 0.0200 |
| good+server | bare | 24 | 24 | 0 | 24 | 10086 | 7.2 | 0.0202 |
| good+server | both | 48 | 48 | 0 | 48 | 10090 | 8.0 | 0.0201 |
| good+off | hinted | 24 | 24 | 0 | 24 | 21758 | 7.9 | 0.0239 |
| good+off | bare | 24 | 24 | 0 | 24 | 21752 | 9.4 | 0.0283 |
| good+off | both | 48 | 48 | 0 | 48 | 21756 | 8.8 | 0.0249 |
| vague | hinted | 24 | 23 | 23 | 23 | 9760 | 12.0 | 0.0342 |
| vague | bare | 24 | 7 | 7 | 11 | 9754 | 7.9 | 0.0146 |
| vague | both | 48 | 30 | 30 | 34 | 9758 | 9.5 | 0.0225 |
| vague+tool | hinted | 24 | 24 | 0 | 24 | 9916 | 8.9 | 0.0257 |
| vague+tool | bare | 24 | 24 | 0 | 24 | 9910 | 8.5 | 0.0214 |
| vague+tool | both | 48 | 48 | 0 | 48 | 9914 | 8.6 | 0.0215 |
| vague+off | hinted | 24 | 24 | 0 | 24 | 21684 | 7.1 | 0.0258 |
| vague+off | bare | 24 | 24 | 0 | 24 | 21678 | 7.6 | 0.0254 |
| vague+off | both | 48 | 48 | 0 | 48 | 21682 | 7.3 | 0.0258 |

Right without the tool: [('rep1.jsonl', 'good', 'bare', 4), ('rep1.jsonl', 'vague', 'bare', 4), ('rep1.jsonl', 'good', 'bare', 7), ('rep1.jsonl', 'vague', 'bare', 7), ('rep2.jsonl', 'good', 'bare', 4), ('rep2.jsonl', 'vague', 'bare', 4), ('rep2.jsonl', 'good', 'bare', 7), ('rep2.jsonl', 'vague', 'bare', 7)]
Tool called, answer wrong: []
Server not connected: []
Built-in tools used: ['Bash', 'Skill']
Turns, median by condition: {'good': 4.0, 'good+tool': 3.0, 'good+server': 3.0, 'good+off': 3.0, 'vague': 3.0, 'vague+tool': 2.0, 'vague+off': 3.0}
Total cost USD: 8.98  wall min: 55.5
Bare phrasing, MCP used per question:
 q1 {'good': '2/2', 'good+tool': '2/2', 'good+server': '2/2', 'good+off': '2/2', 'vague': '2/2', 'vague+tool': '2/2', 'vague+off': '2/2'}
 q2 {'good': '2/2', 'good+tool': '2/2', 'good+server': '2/2', 'good+off': '2/2', 'vague': '0/2', 'vague+tool': '2/2', 'vague+off': '2/2'}
 q3 {'good': '2/2', 'good+tool': '2/2', 'good+server': '2/2', 'good+off': '2/2', 'vague': '2/2', 'vague+tool': '2/2', 'vague+off': '2/2'}
 q4 {'good': '0/2', 'good+tool': '2/2', 'good+server': '2/2', 'good+off': '2/2', 'vague': '0/2', 'vague+tool': '2/2', 'vague+off': '2/2'}
 q5 {'good': '2/2', 'good+tool': '2/2', 'good+server': '2/2', 'good+off': '2/2', 'vague': '2/2', 'vague+tool': '2/2', 'vague+off': '2/2'}
 q6 {'good': '2/2', 'good+tool': '2/2', 'good+server': '2/2', 'good+off': '2/2', 'vague': '0/2', 'vague+tool': '2/2', 'vague+off': '2/2'}
 q7 {'good': '0/2', 'good+tool': '2/2', 'good+server': '2/2', 'good+off': '2/2', 'vague': '0/2', 'vague+tool': '2/2', 'vague+off': '2/2'}
 q8 {'good': '2/2', 'good+tool': '2/2', 'good+server': '2/2', 'good+off': '2/2', 'vague': '0/2', 'vague+tool': '2/2', 'vague+off': '2/2'}
 q9 {'good': '2/2', 'good+tool': '2/2', 'good+server': '2/2', 'good+off': '2/2', 'vague': '0/2', 'vague+tool': '2/2', 'vague+off': '2/2'}
 q10 {'good': '2/2', 'good+tool': '2/2', 'good+server': '2/2', 'good+off': '2/2', 'vague': '0/2', 'vague+tool': '2/2', 'vague+off': '2/2'}
 q11 {'good': '2/2', 'good+tool': '2/2', 'good+server': '2/2', 'good+off': '2/2', 'vague': '1/2', 'vague+tool': '2/2', 'vague+off': '2/2'}
 q12 {'good': '2/2', 'good+tool': '2/2', 'good+server': '2/2', 'good+off': '2/2', 'vague': '0/2', 'vague+tool': '2/2', 'vague+off': '2/2'}

## Context of a one-turn prompt (input tokens, 3 launches each)

| none | [9704] | +0 vs none |
| none+off | [21369] | +11665 vs none |
| good | [9727] | +23 vs none |
| good+tool | [9942] | +238 vs none |
| good+server | [10055] | +351 vs none |
| good+off | [21720] | +12016 vs none |
| good+both | [10055] | +351 vs none |
| good+tool+server-false | [9727] | +23 vs none |

## Slow server (STARTUP_DELAY_MS), 2 launches each

(0, False, 'good') status [['connected'], ['connected']] first tokens [9762, 9762] tools [['ToolSearch', 'search_notes'], ['ToolSearch', 'search_notes']] wall [5.65, 5.89] right [True, True]
(0, False, 'good+tool') status [['connected'], ['connected']] first tokens [9977, 9977] tools [['search_notes'], ['search_notes']] wall [7.14, 5.14] right [True, True]
(0, False, 'good+server') status [['connected'], ['connected']] first tokens [10090, 10090] tools [['search_notes'], ['search_notes']] wall [8.58, 6.63] right [True, True]
(0, True, 'good') status [['connected'], ['connected']] first tokens [9762, 9762] tools [['ToolSearch', 'search_notes'], ['ToolSearch', 'search_notes']] wall [13.66, 12.41] right [True, True]
(0, True, 'good+tool') status [['connected'], ['connected']] first tokens [9977, 9977] tools [['search_notes'], ['search_notes']] wall [4.86, 4.78] right [True, True]
(0, True, 'good+server') status [['connected'], ['connected']] first tokens [10090, 10090] tools [['search_notes'], ['search_notes']] wall [8.86, 5.91] right [True, True]
(10000, False, 'good') status [['connected'], ['connected']] first tokens [9762, 9762] tools [['ToolSearch', 'search_notes'], ['ToolSearch', 'search_notes']] wall [19.28, 17.29] right [True, True]
(10000, False, 'good+tool') status [['connected'], ['connected']] first tokens [9977, 9977] tools [['search_notes'], ['search_notes']] wall [15.25, 14.31] right [True, True]
(10000, False, 'good+server') status [['connected'], ['connected']] first tokens [10090, 10090] tools [['search_notes'], ['search_notes']] wall [15.11, 14.4] right [True, True]
(10000, True, 'good') status [['connected'], ['connected']] first tokens [9762, 9762] tools [['ToolSearch', 'search_notes'], ['ToolSearch', 'search_notes']] wall [16.49, 16.61] right [True, True]
(10000, True, 'good+tool') status [['connected'], ['connected']] first tokens [9977, 9977] tools [['search_notes'], ['search_notes']] wall [18.35, 14.64] right [True, True]
(10000, True, 'good+server') status [['connected'], ['connected']] first tokens [10090, 10090] tools [['search_notes'], ['search_notes']] wall [15.55, 16.35] right [True, True]
(25000, False, 'good') status [['connected'], ['connected']] first tokens [9762, 9762] tools [['ToolSearch', 'search_notes'], ['ToolSearch', 'search_notes']] wall [36.75, 31.99] right [True, True]
(25000, False, 'good+tool') status [['connected'], ['connected']] first tokens [9977, 9977] tools [['search_notes'], ['search_notes']] wall [30.13, 34.32] right [True, True]
(25000, False, 'good+server') status [['connected'], ['connected']] first tokens [10090, 10090] tools [['search_notes'], ['search_notes']] wall [31.91, 30.35] right [True, True]
(25000, True, 'good') status [['connected'], ['connected']] first tokens [9762, 9762] tools [['ToolSearch', 'search_notes'], ['ToolSearch', 'search_notes']] wall [35.4, 34.87] right [True, True]
(25000, True, 'good+tool') status [['connected'], ['connected']] first tokens [9977, 9977] tools [['search_notes'], ['search_notes']] wall [33.7, 30.17] right [True, True]
(25000, True, 'good+server') status [['connected'], ['connected']] first tokens [10090, 10090] tools [['search_notes'], ['search_notes']] wall [30.91, 29.97] right [True, True]

## Calls to a deferred tool before ToolSearch loaded it (from session transcripts)

Sonnet 5.5, rep1 + rep2:

      example: vague+tool 6 {} -> MCP error -32602: Input validation error: Invalid arguments for tool get: Invalid input: expected string, received undefined at path
      example: good+tool 11 {"query": "select:mcp__notes__read_note"} -> MCP error -32602: Input validation error: Invalid arguments for tool read_note: Invalid input: expected string, received undefined at path
      example: vague+tool 3 {} -> MCP error -32602: Input validation error: Invalid arguments for tool get: Invalid input: expected string, received undefined at path
      example: good+tool 3 {"query": "select:mcp__notes__read_note"} -> MCP error -32602: Input validation error: Invalid arguments for tool read_note: Invalid input: expected string, received undefined at path
      example: vague+tool 3 {} -> MCP error -32602: Input validation error: Invalid arguments for tool get: Invalid input: expected string, received undefined at path
      example: good+tool 10 {} -> MCP error -32602: Input validation error: Invalid arguments for tool read_note: Invalid input: expected string, received undefined at path
      example: good+tool 12 {} -> MCP error -32602: Input validation error: Invalid arguments for tool read_note: Invalid input: expected string, received undefined at path
    good {'runs': 48, 'runs with an early call': 0}
    good+tool {'runs': 48, 'runs with an early call': 25, 'early call (ok)': 23, 'early call (error)': 4}
    vague {'runs': 48, 'runs with an early call': 0}
    vague+tool {'runs': 48, 'runs with an early call': 5, 'early call (ok)': 5, 'early call (error)': 3}
    transcripts skipped while matching: 0

Haiku 5.5:

    good {'runs': 24, 'runs with an early call': 1, 'early call (ok)': 2}
    good+tool {'runs': 24, 'runs with an early call': 3, 'early call (ok)': 3}
    vague {'runs': 24, 'runs with an early call': 0}
    vague+tool {'runs': 24, 'runs with an early call': 0}
    transcripts skipped while matching: 0
