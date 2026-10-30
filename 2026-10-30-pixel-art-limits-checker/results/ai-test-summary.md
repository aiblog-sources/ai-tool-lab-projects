# AI test summary (re-checked with the final pixcheck)

First try (v0) passes, per variant and model, out of 12 (3 tasks x 4 consoles):

| Variant | Model | Ran | Pass v0 | Fixed by plain round | Fixed by pixcheck rounds | Cost USD | Calls |
| --- | --- | --- | --- | --- | --- | --- | --- |
| limits | haiku-5-5 | 12/12 | 11 | 1/1 | 1/1 | 0.09 | 14 |
| limits | opus-5-5 | 12/12 | 12 | 0/0 | 0/0 | 2.56 | 15 |
| limits | sonnet-5-5 | 12/12 | 12 | 0/0 | 0/0 | 0.76 | 12 |
| naive | haiku-5-5 | 12/12 | 8 | 3/4 | 4/4 | 0.10 | 21 |
| naive | opus-5-5 | 12/12 | 10 | 2/2 | 2/2 | 2.29 | 16 |
| naive | sonnet-5-5 | 12/12 | 7 | 5/5 | 5/5 | 1.44 | 22 |

Per job (v0 re-check; colors, palettes used of allowed, unique tiles of budget; first error):

| Job | v0 | Colors | Palettes | Tiles | First error | Fix stages (plain1 / check1 / check2) |
| --- | --- | --- | --- | --- | --- | --- |
| limits-dungeon-md-haiku-5-5 | pass | 16 | 1/4 | 71/1425 |  | - / - / - |
| limits-dungeon-md-opus-5-5 | pass | 15 | 1/4 | 1104/1425 |  | - / - / - |
| limits-dungeon-md-sonnet-5-5 | pass | 16 | 1/4 | 128/1425 |  | - / - / - |
| limits-dungeon-nes-haiku-5-5 | pass | 8 | 3/4 | 16/256 |  | - / - / - |
| limits-dungeon-nes-opus-5-5 | pass | 9 | 4/4 | 83/256 |  | - / - / - |
| limits-dungeon-nes-sonnet-5-5 | pass | 9 | 3/4 | 38/256 |  | - / - / - |
| limits-dungeon-sms-haiku-5-5 | pass | 12 | 1/2 | 22/448 |  | - / - / - |
| limits-dungeon-sms-opus-5-5 | pass | 15 | 1/2 | 85/448 |  | - / - / - |
| limits-dungeon-sms-sonnet-5-5 | pass | 10 | 1/2 | 52/448 |  | - / - / - |
| limits-dungeon-snes-haiku-5-5 | pass | 15 | 1/8 | 22/1024 |  | - / - / - |
| limits-dungeon-snes-opus-5-5 | pass | 27 | 3/8 | 834/1024 |  | - / - / - |
| limits-dungeon-snes-sonnet-5-5 | pass | 24 | 3/8 | 510/1024 |  | - / - / - |
| limits-knight-md-haiku-5-5 | pass | 11 | 1/4 | 31/1425 |  | - / - / - |
| limits-knight-md-opus-5-5 | pass | 12 | 1/4 | 35/1425 |  | - / - / - |
| limits-knight-md-sonnet-5-5 | pass | 14 | 1/4 | 30/1425 |  | - / - / - |
| limits-knight-nes-haiku-5-5 | pass | 3 | 1/4 | 12/256 |  | - / - / - |
| limits-knight-nes-opus-5-5 | pass | 3 | 1/4 | 10/256 |  | - / - / - |
| limits-knight-nes-sonnet-5-5 | pass | 3 | 1/4 | 12/256 |  | - / - / - |
| limits-knight-sms-haiku-5-5 | pass | 8 | 1/1 | 10/448 |  | - / - / - |
| limits-knight-sms-opus-5-5 | pass | 10 | 1/1 | 9/448 |  | - / - / - |
| limits-knight-sms-sonnet-5-5 | pass | 10 | 1/1 | 12/448 |  | - / - / - |
| limits-knight-snes-haiku-5-5 | pass | 12 | 1/8 | 26/512 |  | - / - / - |
| limits-knight-snes-opus-5-5 | pass | 12 | 1/8 | 39/512 |  | - / - / - |
| limits-knight-snes-sonnet-5-5 | pass | 14 | 1/8 | 27/512 |  | - / - / - |
| limits-landscape-md-haiku-5-5 | pass | 16 | 1/4 | 251/1425 |  | - / - / - |
| limits-landscape-md-opus-5-5 | pass | 19 | 2/4 | 606/1425 |  | - / - / - |
| limits-landscape-md-sonnet-5-5 | pass | 31 | 4/4 | 396/1425 |  | - / - / - |
| limits-landscape-nes-haiku-5-5 | FAIL | 8 | 3/4 | 258/256 | 258 unique 8x8 tiles, budget 256 (no tile flipping on NES backgrounds) | pass / pass / - |
| limits-landscape-nes-opus-5-5 | pass | 9 | 4/4 | 180/256 |  | - / - / - |
| limits-landscape-nes-sonnet-5-5 | pass | 11 | 4/4 | 253/256 |  | - / - / - |
| limits-landscape-sms-haiku-5-5 | pass | 14 | 1/2 | 50/448 |  | - / - / - |
| limits-landscape-sms-opus-5-5 | pass | 16 | 1/2 | 340/448 |  | - / - / - |
| limits-landscape-sms-sonnet-5-5 | pass | 11 | 1/2 | 224/448 |  | - / - / - |
| limits-landscape-snes-haiku-5-5 | pass | 13 | 1/8 | 345/1024 |  | - / - / - |
| limits-landscape-snes-opus-5-5 | pass | 46 | 8/8 | 652/1024 |  | fail / no image / pass |
| limits-landscape-snes-sonnet-5-5 | pass | 104 | 8/8 | 638/1024 |  | - / - / - |
| naive-dungeon-md-haiku-5-5 | pass | 26 | 2/4 | 740/1425 |  | - / - / - |
| naive-dungeon-md-opus-5-5 | pass | 15 | 1/4 | 740/1425 |  | - / - / - |
| naive-dungeon-md-sonnet-5-5 | pass | 29 | 4/4 | 787/1425 |  | - / - / - |
| naive-dungeon-nes-haiku-5-5 | FAIL | 351 | 4/4 | 511/256 | 57 of 240 16x16 attribute areas use more than 3 colors plus the shared backdrop (worst: x=48 y=32 with 107 plus backdrop) | fail / pass / - |
| naive-dungeon-nes-opus-5-5 | pass | 8 | 3/4 | 47/256 |  | - / - / - |
| naive-dungeon-nes-sonnet-5-5 | FAIL | 14 | 2/4 | 653/256 | 46 of 240 16x16 attribute areas use more than 3 colors plus the shared backdrop (worst: x=64 y=16 with 11 plus backdrop) | pass / pass / - |
| naive-dungeon-sms-haiku-5-5 | pass | 11 | 1/2 | 55/448 |  | - / - / - |
| naive-dungeon-sms-opus-5-5 | pass | 16 | 1/2 | 83/448 |  | - / - / - |
| naive-dungeon-sms-sonnet-5-5 | pass | 14 | 1/2 | 383/448 |  | - / - / - |
| naive-dungeon-snes-haiku-5-5 | FAIL | 78 | 8/8 | 336/1024 | the tile color sets do not fit in 8 palettes of 15 colors: 256 tiles left without a palette (an exact search proved no packing exists) | pass / pass / - |
| naive-dungeon-snes-opus-5-5 | FAIL | 71 | 8/8 | 610/1024 | 2 of 896 tiles use more than 15 colors plus the shared backdrop (worst: x=192 y=32 with 17 plus backdrop) | pass / pass / - |
| naive-dungeon-snes-sonnet-5-5 | FAIL | 60 | 8/8 | 391/1024 | the tile color sets do not fit in 8 palettes of 15 colors: 87 tiles left without a palette (an exact search proved no packing exists) | pass / pass / - |
| naive-knight-md-haiku-5-5 | pass | 11 | 1/4 | 27/1425 |  | - / - / - |
| naive-knight-md-opus-5-5 | pass | 12 | 1/4 | 33/1425 |  | - / - / - |
| naive-knight-md-sonnet-5-5 | pass | 11 | 1/4 | 25/1425 |  | - / - / - |
| naive-knight-nes-haiku-5-5 | FAIL | 7 | 0/4 | 10/256 | 16 of 16 8x8 sprite tiles use more than 3 colors + transparent (worst: x=0 y=0 with 6) | pass / pass / - |
| naive-knight-nes-opus-5-5 | pass | 3 | 1/4 | 12/256 |  | - / - / - |
| naive-knight-nes-sonnet-5-5 | FAIL | 6 | 0/4 | 10/256 | 16 of 16 8x8 sprite tiles use more than 3 colors + transparent (worst: x=0 y=8 with 5) | pass / pass / - |
| naive-knight-sms-haiku-5-5 | pass | 8 | 1/1 | 12/448 |  | - / - / - |
| naive-knight-sms-opus-5-5 | pass | 10 | 1/1 | 10/448 |  | - / - / - |
| naive-knight-sms-sonnet-5-5 | pass | 8 | 1/1 | 10/448 |  | - / - / - |
| naive-knight-snes-haiku-5-5 | pass | 9 | 1/8 | 27/512 |  | - / - / - |
| naive-knight-snes-opus-5-5 | pass | 14 | 1/8 | 31/512 |  | - / - / - |
| naive-knight-snes-sonnet-5-5 | pass | 10 | 1/8 | 25/512 |  | - / - / - |
| naive-landscape-md-haiku-5-5 | pass | 32 | 4/4 | 195/1425 |  | - / - / - |
| naive-landscape-md-opus-5-5 | pass | 16 | 1/4 | 584/1425 |  | - / - / - |
| naive-landscape-md-sonnet-5-5 | pass | 20 | 2/4 | 452/1425 |  | - / - / - |
| naive-landscape-nes-haiku-5-5 | FAIL | 21 | 4/4 | 255/256 | 40 of 240 16x16 attribute areas use more than 3 colors plus the shared backdrop (worst: x=48 y=96 with 6 plus backdrop) | pass / fail / pass |
| naive-landscape-nes-opus-5-5 | pass | 6 | 3/4 | 237/256 |  | - / - / - |
| naive-landscape-nes-sonnet-5-5 | FAIL | 19 | 4/4 | 271/256 | 28 of 240 16x16 attribute areas use more than 3 colors plus the shared backdrop (worst: x=112 y=96 with 6 plus backdrop) | pass / pass / - |
| naive-landscape-sms-haiku-5-5 | pass | 12 | 1/2 | 172/448 |  | - / - / - |
| naive-landscape-sms-opus-5-5 | pass | 13 | 1/2 | 394/448 |  | - / - / - |
| naive-landscape-sms-sonnet-5-5 | pass | 11 | 1/2 | 377/448 |  | - / - / - |
| naive-landscape-snes-haiku-5-5 | pass | 14 | 1/8 | 260/1024 |  | - / - / - |
| naive-landscape-snes-opus-5-5 | FAIL | 77 | 8/8 | 445/1024 | the tile color sets do not fit in 8 palettes of 15 colors: 405 tiles left without a palette (an exact search proved no packing exists) | pass / pass / - |
| naive-landscape-snes-sonnet-5-5 | FAIL | 159 | 8/8 | 727/1024 | 2 of 896 tiles use more than 15 colors plus the shared backdrop (worst: x=16 y=160 with 18 plus backdrop) | pass / pass / - |
