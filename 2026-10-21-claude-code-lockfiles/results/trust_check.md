# Project allow rules in an untrusted workspace (Claude Code 2.1.306)

Found while running the `deny_npmls` condition. The first batch (12 runs, kept in `runs.csv` as
`deny_npmls_untrusted`) put `"allow": ["Bash(npm ls:*)"]` next to the deny rules in `.claude/settings.json`.
Every `npm ls` call came back "This command requires approval" and all 8 version questions went unanswered,
while the deny rules from the same file were applied.

A manual check in a fresh copy of the fixture, `claude -p "Run exactly: npm ls express --package-lock-only ..."`
with `--setting-sources project`:

| Where the allow rule was | Result |
| --- | --- |
| `.claude/settings.json`, `Bash(npm ls *)` | stderr: `Ignoring 1 permissions.allow entry from .claude/settings.json: this workspace has not been trusted. Run Claude Code interactively here once and accept the trust dialog, or set projects["<folder>"].hasTrustDialogAccepted: true in ~/.claude.json.` Command denied. |
| `.claude/settings.json`, `Bash(npm ls:*)` | Same message, command denied. |
| `--allowedTools "Bash(npm ls:*)"` on the command line | Ran, answered 4.22.3. |
| `.claude/settings.local.json` with `--setting-sources project,local` | Ran, answered 4.22.3. |

The `deny_npmls` condition in the results passes the allow rule with `--allowedTools`.

Also seen: `.claudeignore` appears 0 times in the Claude Code 2.1.306 executable (`grep -c claudeignore`), so
nothing in Claude Code reads that file. Claude saw the file name in `ls` or `git ls-files` output in 6 of 18
`.claudeignore` runs and never opened it.
