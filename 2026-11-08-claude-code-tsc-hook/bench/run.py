#!/usr/bin/env python3
"""Does a type-check hook make Claude Code leave fewer TypeScript errors behind?

Usage: python3 bench/run.py <scratch-folder> <out.jsonl>
Each run copies fixture/ into a fresh folder, adds the condition's CLAUDE.md or .claude/settings.json hooks, gives
`claude -p` one task from tasks.json, then grades the folder: tsc errors left, and the task's hidden tests in hidden/
(compiled with tsc even when it reports errors). MODELS, CONDITIONS, TASKS, REPS and WORKERS limit or size a run.
"""
import glob, json, os, re, shutil, subprocess, sys, time, uuid
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

root = Path(__file__).resolve().parent.parent
scratch, out = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve()
scratch.mkdir(parents=True, exist_ok=True)
tasks = json.loads((root / "bench/tasks.json").read_text()) + [json.loads((root / "bench/task-combo.json").read_text())]
conditions = json.loads((root / "bench/conditions.json").read_text())
models = os.environ.get("MODELS", "haiku,sonnet").split(",")
reps = int(os.environ.get("REPS", "2"))
if os.environ.get("CONDITIONS"):
    conditions = {k: v for k, v in conditions.items() if k in os.environ["CONDITIONS"].split(",")}
shells = os.environ.get("SHELLS", "shell,noshell").split(",")
# The combo task (all six prompts in one) is graded on tsc only and runs only when TASKS names it.
tasks = [t for t in tasks if t["id"] in os.environ["TASKS"].split(",")] if os.environ.get("TASKS") else [t for t in tasks if t["id"] != "combo"]

CLAUDE_MD = "After any change to TypeScript files, run `npx tsc --noEmit` and fix every error before you say you are done.\n"
# The same tools in every condition: file tools, and the shell commands a developer would use to look around and build.
FILE_TOOLS = ["Read", "Edit", "Write", "MultiEdit", "Grep", "Glob"]
# "noshell" runs get the file tools only, as a plain `claude -p` with no Bash permissions does.
ALLOW = FILE_TOOLS + [f"Bash({c}:*)" for c in
         ("npx", "npm", "node", "tsc", "./node_modules/.bin/tsc", "ls", "cat", "grep", "rg", "find", "sed", "head", "tail", "wc")]
DENY = ["WebSearch", "WebFetch", "Task", "Agent"]
# No user CLAUDE.md (--setting-sources project) and no auto-memory: only the fixture, the prompt and the condition.
ENV = {"CLAUDE_CODE_DISABLE_AUTO_MEMORY": "1"}

def setup(work, c):
    shutil.copytree(root / "fixture", work, ignore=shutil.ignore_patterns("dist"), symlinks=True)
    if c["claude_md"]:
        (work / "CLAUDE.md").write_text(CLAUDE_MD)
    if c["hooks"]:
        hooks = {}
        (work / ".claude/hooks").mkdir(parents=True)
        shutil.copy(root / "hooks/tsc-check.sh", work / ".claude/hooks/tsc-check.sh")
        for event, h in c["hooks"].items():
            entry = {"hooks": [{"type": "command", "command": f'"$CLAUDE_PROJECT_DIR"/.claude/hooks/tsc-check.sh {h["args"]}', "timeout": 60}]}
            if h["matcher"]: entry["matcher"] = h["matcher"]
            hooks[event] = [entry]
        (work / ".claude/settings.json").write_text(json.dumps({"hooks": hooks}, indent=2))

def tsc_errors(work):
    p = subprocess.run(["./node_modules/.bin/tsc", "--noEmit", "--pretty", "false"], cwd=work, capture_output=True, text=True)
    lines = [l for l in p.stdout.splitlines() if "error TS" in l]
    return len(lines), lines[:20]

def hidden_tests(work, task):
    shutil.rmtree(work / "dist", ignore_errors=True)
    subprocess.run(["./node_modules/.bin/tsc", "--pretty", "false"], cwd=work, capture_output=True, text=True)  # emits even with errors
    p = subprocess.run(["node", "--test", str(root / "hidden" / f"{task}.test.mjs")], capture_output=True, text=True,
                       env={**os.environ, "SHOP_DIST": str(work / "dist")}, timeout=120)
    passed = int(m.group(1)) if (m := re.search(r"^# pass (\d+)", p.stdout, re.M)) else 0
    failed = int(m.group(1)) if (m := re.search(r"^# fail (\d+)", p.stdout, re.M)) else 0
    return passed, failed

def run(task, model, cname, c, shell, rep):
    rid = f"{task['id']}-{model}-{cname}-{shell}-{rep}"
    work = scratch / "work" / rid
    shutil.rmtree(work, ignore_errors=True)
    setup(work, c)
    hook_log = scratch / "hooklogs" / f"{rid}.jsonl"
    hook_log.parent.mkdir(exist_ok=True); hook_log.unlink(missing_ok=True)
    sid = str(uuid.uuid4())
    cmd = ["claude", "-p", task["prompt"], "--model", model, "--session-id", sid, "--setting-sources", "project",
           "--strict-mcp-config", "--mcp-config", '{"mcpServers":{}}', "--permission-mode", "acceptEdits",
           "--allowedTools", *(ALLOW if shell == "shell" else FILE_TOOLS),
           "--disallowedTools", *(DENY if shell == "shell" else DENY + ["Bash"]), "--output-format", "stream-json", "--verbose"]
    t0 = time.time()
    try:
        p = subprocess.run(cmd, cwd=work, env={**os.environ, **ENV, "HOOK_LOG": str(hook_log)}, capture_output=True,
                           text=True, stdin=subprocess.DEVNULL, timeout=1200)
        stdout, code = p.stdout, p.returncode
    except subprocess.TimeoutExpired as e:
        stdout, code = (e.stdout or b"").decode() if isinstance(e.stdout, bytes) else (e.stdout or ""), "timeout"
    wall = time.time() - t0
    (scratch / "streams").mkdir(exist_ok=True)
    (scratch / "streams" / f"{rid}.jsonl").write_text(stdout)
    tools, init, result, hook_msgs = [], {}, {}, 0
    for line in stdout.splitlines():
        try: m = json.loads(line)
        except json.JSONDecodeError: continue
        if m.get("type") == "system" and m.get("subtype") == "init": init = m
        if m.get("type") == "assistant":
            for b in m["message"]["content"]:
                if b["type"] == "tool_use": tools.append({"name": b["name"], "input": b["input"]})
        if m.get("type") == "result": result = m
        if "TypeScript check failed" in line and m.get("type") != "assistant": hook_msgs += 1
    # Hook output reaches Claude as transcript attachments (not in stream-json): count them by type, keep nothing else.
    att = {}
    for tp in glob.glob(os.path.expanduser(f"~/.claude/projects/*/{sid}.jsonl")):
        for line in open(tp):
            try: m = json.loads(line)
            except json.JSONDecodeError: continue
            a = m.get("attachment") if isinstance(m.get("attachment"), dict) else None
            if a and "hook" in a.get("type", ""):
                k = f'{a.get("hookEvent") or a.get("hookName", "")}:{a["type"]}:{"rendered" if m.get("rendered") else "silent"}'
                att[k] = att.get(k, 0) + 1
    cmds = [t["input"].get("command", "") for t in tools if t["name"] == "Bash"]
    ran_tsc = [x for x in cmds if re.search(r"\btsc\b|npm run build", x)]
    hooks = [json.loads(l) for l in hook_log.read_text().splitlines()] if hook_log.exists() else []
    errs, err_lines = tsc_errors(work)
    passed, failed = (0, 0) if task.get("tsc_only") else hidden_tests(work, task["id"])
    u = result.get("usage", {})
    return {"task": task["id"], "model": model, "model_id": init.get("model"), "condition": cname, "shell": shell, "rep": rep,
            "tsc_errors_left": errs, "tsc_error_lines": err_lines, "tests_passed": passed, "tests_failed": failed,
            "tests_ok": failed == 0 and passed > 0, "claude_ran_tsc": len(ran_tsc), "tsc_commands": ran_tsc[:5],
            "edits": sum(t["name"] in ("Edit", "Write", "MultiEdit") for t in tools), "tool_calls": len(tools),
            "hook_runs": len(hooks), "hook_failures": sum(h["errors"] > 0 for h in hooks),
            "hook_ms_total": sum(h["ms"] for h in hooks), "hook_attachments": att,
            "permission_denials": len(result.get("permission_denials") or []),
            "turns": result.get("num_turns"), "cost_usd": result.get("total_cost_usd"), "duration_ms": result.get("duration_ms"),
            "wall_s": round(wall, 1), "output_tokens": u.get("output_tokens"), "subtype": result.get("subtype"), "exit": code,
            "final_text": (result.get("result") or "")[-600:]}

done = set()
if out.exists():
    for line in out.read_text().splitlines():
        r = json.loads(line); done.add((r["task"], r["model"], r["condition"], r["shell"], r["rep"]))
jobs = [(t, m, cn, c, sh, rep) for rep in range(reps) for t in tasks for m in models for sh in shells
        for cn, c in conditions.items() if (t["id"], m, cn, sh, rep) not in done]
print(f"{len(jobs)} runs", flush=True)

def one(job):
    for attempt in range(3):   # a Claude Code auto-update mid-run makes launches exit at once
        r = run(*job)
        if r["model_id"]: return r
        time.sleep(30)
    return r

with out.open("a") as f, ThreadPoolExecutor(int(os.environ.get("WORKERS", "3"))) as ex:
    for r in ex.map(one, jobs):
        f.write(json.dumps(r) + "\n"); f.flush()
        print(f"{r['task']:15} {r['model']:6} {r['condition']:11} {r['shell']:7} r{r['rep']} errs={r['tsc_errors_left']:<3} tests={r['tests_passed']}/{r['tests_passed']+r['tests_failed']} "
              f"selftsc={r['claude_ran_tsc']} hooks={r['hook_runs']}/{r['hook_failures']} ${r['cost_usd']} {r['wall_s']}s", flush=True)
print("DONE", flush=True)
