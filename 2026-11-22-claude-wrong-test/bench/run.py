#!/usr/bin/env python3
"""Wrong test or wrong code: what does Claude Code change when one unit test fails?

Usage: python3 bench/run.py <scratch-folder> <out.jsonl>

Each run copies fixture/ (six small functions, each with a docstring contract and three unittest tests) into a
fresh git repo and applies one variant from variants/variants.json:
  bug      the code breaks its docstring and the test is right (the right move: fix the code)
  badtest  the code follows its docstring and one test asserts the opposite (the right move: fix the test or
           report the conflict; changing the code breaks the contract)
Then it gives `claude -p` the condition's prompt (and CLAUDE.md, if any) and grades the folder: hidden spec tests
(hidden/spec_tests.py) for every module, whether tests/ or textkit/ changed, and whether the visible suite passes.
MODELS, CONDITIONS, FUNCS, KINDS, REPS and WORKERS limit or size a run. Finished runs in <out.jsonl> are skipped.
"""
import ast, json, os, re, shutil, subprocess, sys, time, uuid
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

root = Path(__file__).resolve().parent.parent
scratch, out = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve()
scratch.mkdir(parents=True, exist_ok=True)
variants = json.loads((root / "variants/variants.json").read_text())
conditions = json.loads((root / "bench/conditions.json").read_text())


def pick(env, allv):
    return [x for x in allv if x in os.environ[env].split(",")] if os.environ.get(env) else list(allv)


models = os.environ.get("MODELS", "sonnet").split(",")
conds = pick("CONDITIONS", conditions)
funcs = pick("FUNCS", variants)
kinds = pick("KINDS", ["bug", "badtest"])
reps = int(os.environ.get("REPS", "2"))

TOOLS = ["Read", "Edit", "Write", "MultiEdit", "Grep", "Glob"] + [f"Bash({c}:*)" for c in
         ("python3", "python", "ls", "cat", "grep", "find", "head", "tail", "wc", "git diff", "git status", "git log")]
DENY = ["WebSearch", "WebFetch", "Task", "Agent"]
ENV = {"CLAUDE_CODE_DISABLE_AUTO_MEMORY": "1"}
# --setting-sources project still loads the user's ~/.claude/CLAUDE.md (it is memory, not a setting): exclude it so
# only the fixture's own CLAUDE.md (if the condition has one) reaches the model.
HOME_CLAUDE = os.path.expanduser("~/.claude")
SETTINGS = json.dumps({"claudeMdExcludes": [f"{HOME_CLAUDE}/CLAUDE.md", f"{HOME_CLAUDE}/rules/**"]})
GIT = ["git", "-c", "user.name=fixture", "-c", "user.email=fixture@example.invalid"]


def strip_docstrings(path):
    """Delete every function docstring from a module (the *_nodoc conditions): only the code and the tests remain."""
    src = path.read_text().splitlines(keepends=True)
    drop = set()
    for n in ast.walk(ast.parse("".join(src))):
        if isinstance(n, ast.FunctionDef) and ast.get_docstring(n) is not None:
            d = n.body[0]
            drop.update(range(d.lineno - 1, d.end_lineno))
    path.write_text("".join(l for i, l in enumerate(src) if i not in drop))


def setup(work, fn, kind, cond):
    shutil.copytree(root / "fixture", work, ignore=shutil.ignore_patterns("__pycache__"))
    path, old, new = variants[fn][kind]
    f = work / path
    text = f.read_text()
    assert text.count(old) == 1, (fn, kind)
    f.write_text(text.replace(old, new))
    if cond.get("strip_docstrings"):
        for m in (work / "textkit").glob("*.py"):
            strip_docstrings(m)
    if cond.get("claude_md"):
        (work / "CLAUDE.md").write_text(cond["claude_md"] + "\n")
    subprocess.run(["git", "init", "-q"], cwd=work, check=True)
    subprocess.run(GIT + ["add", "-A"], cwd=work, check=True)
    subprocess.run(GIT + ["commit", "-qm", "init"], cwd=work, check=True)


def hidden(work, module):
    p = subprocess.run([sys.executable, "-I", str(root / "hidden/spec_tests.py"), str(work), module],
                       capture_output=True, text=True, timeout=60)
    try:
        return json.loads(p.stdout.strip().splitlines()[-1])
    except (IndexError, json.JSONDecodeError):
        return {"passed": 0, "failed": 1, "failures": [p.stderr[-300:]]}


def visible(work):
    p = subprocess.run([sys.executable, "-m", "unittest", "discover", "-s", "tests", "-t", "."], cwd=work,
                       capture_output=True, text=True, timeout=60)
    m = re.search(r"Ran (\d+) test", p.stderr)
    return {"ok": p.returncode == 0, "ran": int(m.group(1)) if m else 0, "tail": p.stderr.strip().splitlines()[-1:]}


def run(fn, kind, model, cname, rep):
    cond = conditions[cname]
    rid = f"{fn}-{kind}-{model}-{cname}-{rep}"
    work = scratch / "work" / rid
    shutil.rmtree(work, ignore_errors=True)
    setup(work, fn, kind, cond)
    sid = str(uuid.uuid4())
    cmd = ["claude", "-p", cond["prompt"], "--model", model, "--session-id", sid, "--setting-sources", "project",
           "--settings", SETTINGS,            "--strict-mcp-config", "--mcp-config", '{"mcpServers":{}}', "--permission-mode", "acceptEdits",
           "--allowedTools", *TOOLS, "--disallowedTools", *DENY, "--output-format", "stream-json", "--verbose"]
    t0 = time.time()
    try:
        p = subprocess.run(cmd, cwd=work, env={**os.environ, **ENV}, capture_output=True, text=True,
                           stdin=subprocess.DEVNULL, timeout=900)
        stdout, code = p.stdout, p.returncode
    except subprocess.TimeoutExpired as e:
        stdout, code = (e.stdout or b"").decode() if isinstance(e.stdout, bytes) else (e.stdout or ""), "timeout"
    wall = time.time() - t0
    (scratch / "streams").mkdir(exist_ok=True)
    (scratch / "streams" / f"{rid}.jsonl").write_text(stdout)
    tools, init, result = [], {}, {}
    for line in stdout.splitlines():
        try:
            m = json.loads(line)
        except json.JSONDecodeError:
            continue
        if m.get("type") == "system" and m.get("subtype") == "init":
            init = m
        if m.get("type") == "assistant":
            for b in m["message"]["content"]:
                if b["type"] == "tool_use":
                    tools.append({"name": b["name"], "input": b["input"]})
        if m.get("type") == "result":
            result = m
    for d in work.rglob("__pycache__"):
        shutil.rmtree(d, ignore_errors=True)
    subprocess.run(GIT + ["add", "-A"], cwd=work, check=True)
    changed = subprocess.run(["git", "diff", "--cached", "--name-only"], cwd=work, capture_output=True,
                             text=True).stdout.split()
    diff = subprocess.run(["git", "diff", "--cached"], cwd=work, capture_output=True, text=True).stdout
    (scratch / "diffs").mkdir(exist_ok=True)
    (scratch / "diffs" / f"{rid}.diff").write_text(diff)
    spec = {m: hidden(work, variants[m]["module"]) for m in variants}
    vis = visible(work)
    u = result.get("usage", {})
    return {"func": fn, "kind": kind, "model": model, "model_id": init.get("model"), "condition": cname, "rep": rep,
            "spec_ok": spec[fn]["failed"] == 0, "spec": spec[fn], "others_ok": all(spec[m]["failed"] == 0 for m in spec if m != fn),
            "changed": changed, "tests_changed": any(c.startswith("tests/") for c in changed),
            "code_changed": any(c.startswith("textkit/") for c in changed), "visible_ok": vis["ok"], "visible_ran": vis["ran"],
            "ran_tests": sum(t["name"] == "Bash" and "unittest" in t["input"].get("command", "") for t in tools),
            "tool_calls": len(tools), "edits": sum(t["name"] in ("Edit", "Write", "MultiEdit") for t in tools),
            "turns": result.get("num_turns"), "cost_usd": result.get("total_cost_usd"), "duration_ms": result.get("duration_ms"),
            "wall_s": round(wall, 1), "output_tokens": u.get("output_tokens"), "subtype": result.get("subtype"), "exit": code,
            "final_text": result.get("result") or ""}


done = set()
if out.exists():
    for line in out.read_text().splitlines():
        r = json.loads(line)
        done.add((r["func"], r["kind"], r["model"], r["condition"], r["rep"]))
jobs = [(f, k, m, c, rep) for rep in range(reps) for m in models for c in conds for k in kinds for f in funcs
        if (f, k, m, c, rep) not in done]
print(f"{len(jobs)} runs", flush=True)


def one(job):
    for _ in range(3):  # a Claude Code auto-update mid-run makes launches exit at once
        r = run(*job)
        if r["model_id"]:
            return r
        time.sleep(30)
    return r


with out.open("a") as f, ThreadPoolExecutor(int(os.environ.get("WORKERS", "4"))) as ex:
    for r in ex.map(one, jobs):
        f.write(json.dumps(r) + "\n")
        f.flush()
        print(f"{r['func']:9} {r['kind']:7} {r['model']:6} {r['condition']:7} r{r['rep']} spec={'ok ' if r['spec_ok'] else 'BAD'} "
              f"tests_changed={int(r['tests_changed'])} code_changed={int(r['code_changed'])} visible={int(r['visible_ok'])} "
              f"${r['cost_usd']} {r['wall_s']}s", flush=True)
print("DONE", flush=True)
