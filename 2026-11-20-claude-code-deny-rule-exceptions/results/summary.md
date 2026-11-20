| Probe | none | deny_glob | allow_example | neg_bare | neg_glob | neg_first | neg_other_file | neg_anchored | neg_ask | charclass | neg_dir | neg_dir_files | ask_plain | class_bang_x | class_caret | class_plain |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| read_env | read | deny | deny | deny | deny | deny | deny | deny | ask | deny | read | read | ask | read | read | read |
| read_env_local | read | deny | deny | deny | deny | deny | deny | deny | ask | read | read | read | ask | read | read | deny |
| read_env_production | read | deny | deny | deny | deny | deny | deny | deny | ask | read | read | read | ask | read | read | deny |
| read_env_example | read | deny | deny | read | read | deny | deny | deny | read | deny | read | read | ask | read | deny | read |
| read_api_env | read | deny | deny | deny | deny | deny | deny | read | ask | deny | read | read | ask | read | read | read |
| read_api_env_example | read | deny | deny | read | read | deny | deny | read | read | deny | read | read | ask | read | deny | read |
| cat_env | read | deny | deny | deny | deny | deny | deny | deny | read | deny | read | read | read | read | read | read |
| cat_env_production | read | deny | deny | deny | deny | deny | deny | deny | read | read | read | read | read | read | read | deny |
| cat_env_example | read | deny | deny | read | read | deny | deny | deny | read | deny | read | read | read | read | deny | read |
| cat_api_env_example | read | deny | deny | read | read | deny | deny | read | read | deny | read | read | read | read | deny | read |
| grep | E+L+P+X+A+AX+S+SP+SD | S+SP+SD | S+SP+SD | S+SP+SD | S+SP+SD | S+SP+SD | S+SP+SD | A+AX+S+SP+SD | E+L+P+X+A+AX+S+SP+SD | L+P+S+SP+SD | E+L+P+X+A+AX | E+L+P+X+A+AX | E+L+P+X+A+AX+S+SP+SD | E+L+P+X+A+AX+S+SP+SD | E+L+P+A+S+SP+SD | E+X+A+AX+S+SP+SD |
| read_secret | read | read | read | read | read | read | read | read | read | read | deny | deny | read | read | read | read |
| read_secret_public | read | read | read | read | read | read | read | read | read | read | deny | read | read | read | read | read |
| read_secret_public_dir | read | read | read | read | read | read | read | read | read | read | deny | deny | read | read | read | read |
| fromsub_read_env | read | deny | deny | deny | deny | deny | deny | deny | ask | deny | read | read | ask | read | read | read |
| fromsub_read_env_example | read | deny | deny | read | read | deny | deny | deny | read | deny | read | read | ask | read | deny | read |

Reps that disagreed: 0

Claude Code 2.1.329; model claude-sonnet-5-5; 512 runs; total cost at API prices USD 1.41; median wall time 4.1 s.
