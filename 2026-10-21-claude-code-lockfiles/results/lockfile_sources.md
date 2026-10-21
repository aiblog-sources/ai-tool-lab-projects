# Lockfiles measured in lock_tokens.csv

Counted on 2026-10-21 with `bench/lock_tokens.py` (Claude Sonnet 5.5, which shares its tokenizer with Haiku 5.5,
Opus 5.5 and Opus 4.7). The files are not copied here; fetch them at these commits.

| File in the CSV | Source |
| --- | --- |
| npm__package-lock.json, yarn__yarn.lock, pnpm__pnpm-lock.yaml, fixture__package.json | The fixture's `package.json` (10 direct dependencies), locked with npm 10, Yarn 1.22.22 and pnpm 10.6 on 2026-10-21 |
| BurntSushi_ripgrep__Cargo.lock | https://github.com/BurntSushi/ripgrep/blob/3fce3b5bb0236da2df6d99672afb8a719642eca7/Cargo.lock |
| cli_cli__go.sum | https://github.com/cli/cli/blob/ad27e286ab6eb101f192176defdfea7475e2a267/go.sum |
| pallets_flask__uv.lock | https://github.com/pallets/flask/blob/d086db856be187255b8ec61ef409357393020f32/uv.lock |
| python-poetry_poetry__poetry.lock | https://github.com/python-poetry/poetry/blob/20d69404423790b7c1546c54d9ce90a72883fe36/poetry.lock |
| rails_rails__Gemfile.lock | https://github.com/rails/rails/blob/3f0b675158e8284241bb1213c00375bc1fb9c52f/Gemfile.lock |
| composer_composer__composer.lock | https://github.com/composer/composer/blob/515bd9bbe01ce7c697e0688053400ceb3839cc97/composer.lock |
