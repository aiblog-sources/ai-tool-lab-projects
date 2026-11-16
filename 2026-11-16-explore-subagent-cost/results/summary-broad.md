| Question | Condition | Found (per run) | Extras (per run) | Cost (median) | Main context at end (median) | Wall s (median) | Explore used |
|---|---|---|---|---|---|---|---|
| django-middleware | no-subagent | 22/22, 22/22, 22/22 | 0, 0, 0 | $0.021 | 15,897 | 9 | 0/3 |
| django-middleware | default | 22/22, 22/22, 22/22 | 0, 0, 0 | $0.022 | 18,999 | 10 | 0/3 |
| django-middleware | explore | 22/22, 22/22, 22/22 | 0, 3, 1 | $0.065 | 19,694 | 19 | 3/3 |
| django-middleware | explore-haiku | 22/22, 22/22, 22/22 | 3, 0, 0 | $0.031 | 19,649 | 24 | 3/3 |
| django-commands | no-subagent | 33/33, 33/33, 33/33 | 0, 0, 0 | $0.011 | 15,200 | 6 | 0/3 |
| django-commands | default | 33/33, 33/33, 33/33 | 0, 0, 0 | $0.011 | 18,091 | 6 | 0/3 |
| django-commands | explore | 33/33, 33/33, 32/33 | 0, 0, 0 | $0.043 | 19,165 | 13 | 3/3 |
| django-commands | explore-haiku | 33/33, 33/33, 33/33 | 0, 0, 0 | $0.022 | 19,561 | 14 | 3/3 |
| django-signals | no-subagent | 24/24, 24/24, 24/24 | 0, 0, 0 | $0.017 | 15,457 | 8 | 0/3 |
| django-signals | default | 24/24, 24/24, 24/24 | 0, 0, 0 | $0.017 | 18,267 | 8 | 0/3 |
| django-signals | explore | 24/24, 24/24, 24/24 | 0, 0, 0 | $0.048 | 20,075 | 16 | 3/3 |
| django-signals | explore-haiku | 24/24, 24/24, 24/24 | 0, 0, 0 | $0.033 | 19,975 | 21 | 3/3 |

no-subagent: found 237/237, extras 0, cost $0.17 total, median ctx 15,457, median main input all turns 44,865, median sub input 0, median wall 8s, haiku $0.00
default: found 237/237, extras 0, cost $0.20 total, median ctx 18,267, median main input all turns 53,458, median sub input 0, median wall 8s, haiku $0.00
explore: found 236/237, extras 4, cost $0.50 total, median ctx 19,694, median main input all turns 55,315, median sub input 26,358, median wall 16s, haiku $0.00
explore-haiku: found 237/237, extras 3, cost $0.30 total, median ctx 19,693, median main input all turns 54,965, median sub input 18,248, median wall 21s, haiku $0.02

django-middleware extras: [('Correction', 'explore'), ('MiddlewareMixin', 'explore'), ('MiddlewareMixin', 'explore-haiku'), ('_EnsureCsrfCookie', 'explore'), ('_EnsureCsrfCookie', 'explore-haiku'), ('_EnsureCsrfToken', 'explore-haiku')]
django-commands extras: []
django-signals extras: []
