192 runs, model ['claude-sonnet-5-5']

| Condition | Phrasing | Runs | Called the MCP tool | Right answer | Used Bash/Grep/Glob | Median turns | Median cost (USD) |
| --- | --- | --- | --- | --- | --- | --- | --- |
| good | hinted | 24 | 24 | 24 | 0 | 4.0 | 0.033 |
| good | bare | 24 | 19 | 23 | 12 | 4.0 | 0.039 |
| vague | hinted | 24 | 17 | 17 | 24 | 6.0 | 0.035 |
| vague | bare | 24 | 3 | 7 | 20 | 2.0 | 0.021 |
| vague+instr | hinted | 24 | 24 | 24 | 6 | 3.0 | 0.028 |
| vague+instr | bare | 24 | 22 | 24 | 6 | 3.0 | 0.026 |
| good-upfront | hinted | 24 | 24 | 24 | 2 | 2.5 | 0.024 |
| good-upfront | bare | 24 | 24 | 24 | 0 | 3.0 | 0.038 |

Right answers without calling the tool: [('good', 'bare', 4), ('vague', 'bare', 4), ('vague+instr', 'bare', 4), ('good', 'bare', 7), ('vague', 'bare', 7), ('good', 'bare', 4), ('vague', 'bare', 4), ('vague+instr', 'bare', 4), ('good', 'bare', 7), ('vague', 'bare', 7)]
Called the tool but answer wrong: []
Server not connected: []
Tool calls (MCP) per run, median by condition: {'good': 2.0, 'vague': 0.0, 'vague+instr': 1.0, 'good-upfront': 2.0}
Total cost USD: 6.27  total wall min: 31.3
Per question, bare phrasing, MCP used (of runs):
 q1 {'good': '2/2', 'vague': '1/2', 'vague+instr': '2/2', 'good-upfront': '2/2'}
 q2 {'good': '2/2', 'vague': '0/2', 'vague+instr': '2/2', 'good-upfront': '2/2'}
 q3 {'good': '2/2', 'vague': '0/2', 'vague+instr': '2/2', 'good-upfront': '2/2'}
 q4 {'good': '0/2', 'vague': '0/2', 'vague+instr': '0/2', 'good-upfront': '2/2'}
 q5 {'good': '1/2', 'vague': '0/2', 'vague+instr': '2/2', 'good-upfront': '2/2'}
 q6 {'good': '2/2', 'vague': '1/2', 'vague+instr': '2/2', 'good-upfront': '2/2'}
 q7 {'good': '0/2', 'vague': '0/2', 'vague+instr': '2/2', 'good-upfront': '2/2'}
 q8 {'good': '2/2', 'vague': '0/2', 'vague+instr': '2/2', 'good-upfront': '2/2'}
 q9 {'good': '2/2', 'vague': '0/2', 'vague+instr': '2/2', 'good-upfront': '2/2'}
 q10 {'good': '2/2', 'vague': '1/2', 'vague+instr': '2/2', 'good-upfront': '2/2'}
 q11 {'good': '2/2', 'vague': '0/2', 'vague+instr': '2/2', 'good-upfront': '2/2'}
 q12 {'good': '2/2', 'vague': '0/2', 'vague+instr': '2/2', 'good-upfront': '2/2'}
