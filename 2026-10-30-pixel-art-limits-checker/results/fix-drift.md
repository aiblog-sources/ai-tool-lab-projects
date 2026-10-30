# Fix drift

Passing fixes, mean RGB change vs v0: plain 10.0 (n=11, median 5.3); pixcheck 13.0 (n=12, median 7.7)

| Job | v0 colors | Stage | Passes | Colors | Mean RGB change vs v0 |
| --- | --- | --- | --- | --- | --- |
| limits-landscape-nes-haiku-5-5 | 8 | plain1 | yes | 7 | 0.8 |
| limits-landscape-nes-haiku-5-5 | 8 | check1 | yes | 9 | 42.1 |
| naive-dungeon-nes-haiku-5-5 | 351 | plain1 | no | 19 | 4.3 |
| naive-dungeon-nes-haiku-5-5 | 351 | check1 | yes | 13 | 8.8 |
| naive-dungeon-nes-sonnet-5-5 | 14 | plain1 | yes | 9 | 32.8 |
| naive-dungeon-nes-sonnet-5-5 | 14 | check1 | yes | 11 | 44.7 |
| naive-dungeon-snes-haiku-5-5 | 78 | plain1 | yes | 46 | 10.1 |
| naive-dungeon-snes-haiku-5-5 | 78 | check1 | yes | 13 | 3.7 |
| naive-dungeon-snes-opus-5-5 | 71 | plain1 | yes | 53 | 0.1 |
| naive-dungeon-snes-opus-5-5 | 71 | check1 | yes | 59 | 2.0 |
| naive-dungeon-snes-sonnet-5-5 | 60 | plain1 | yes | 45 | 2.7 |
| naive-dungeon-snes-sonnet-5-5 | 60 | check1 | yes | 14 | 6.8 |
| naive-knight-nes-haiku-5-5 | 7 | plain1 | yes | 5 | 18.7 |
| naive-knight-nes-haiku-5-5 | 7 | check1 | yes | 6 | 8.0 |
| naive-knight-nes-sonnet-5-5 | 6 | plain1 | yes | 3 | 11.3 |
| naive-knight-nes-sonnet-5-5 | 6 | check1 | yes | 4 | 15.5 |
| naive-landscape-nes-haiku-5-5 | 21 | plain1 | yes | 12 | 5.3 |
| naive-landscape-nes-haiku-5-5 | 21 | check1 | no | 13 | 7.5 |
| naive-landscape-nes-haiku-5-5 | 21 | check2 | yes | 13 | 7.5 |
| naive-landscape-nes-sonnet-5-5 | 19 | plain1 | yes | 8 | 25.5 |
| naive-landscape-nes-sonnet-5-5 | 19 | check1 | yes | 9 | 15.8 |
| naive-landscape-snes-opus-5-5 | 77 | plain1 | yes | 57 | 1.3 |
| naive-landscape-snes-opus-5-5 | 77 | check1 | yes | 64 | 0.4 |
| naive-landscape-snes-sonnet-5-5 | 159 | plain1 | yes | 65 | 1.4 |
| naive-landscape-snes-sonnet-5-5 | 159 | check1 | yes | 52 | 1.0 |
