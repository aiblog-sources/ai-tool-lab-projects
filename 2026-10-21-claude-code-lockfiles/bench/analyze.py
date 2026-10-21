#!/usr/bin/env python3
"""Turn the raw stream-json runs into per-run rows and summary tables.

A tool call "touches the lockfile" when its input names a lockfile (Read path, Grep path or glob, Bash
command) or when its output carries lockfile lines (a Grep or grep over the whole repo). Lockfile
characters that reached the model are counted from the tool results: result lines (after a path and
line-number prefix) that appear in the lockfile and in no other file of the fixture.

usage: analyze.py RUNS_DIR OUT_DIR
"""
import csv
import json
import re
import statistics
import sys
from collections import defaultdict
from pathlib import Path

LOCK_RE = re.compile(r"package-lock\.json|yarn\.lock|pnpm-lock\.yaml")
QUESTIONS = {q["id"]: q for q in json.loads((Path(__file__).parent / "questions.json").read_text())}
GRADE_OVERRIDES = Path(__file__).parent / "grade_overrides.json"
CONDITIONS = ["none", "claudeignore", "dotignore", "deny", "instruction", "deny_npmls", "deny_npmls_untrusted"]
FIXTURE = Path(__file__).resolve().parent.parent / "fixture"
PREFIX_RE = re.compile(r"^(?:\S*?(?:package-lock\.json|yarn\.lock|pnpm-lock\.yaml)[:-])?\s*\d*[:\t-]?")


def _line_set(paths):
    out = set()
    for f in paths:
        out.update(ln.strip() for ln in f.read_text().splitlines())
    return out


LOCK_LINES = _line_set([FIXTURE / "package-lock.json"])
OTHER_LINES = _line_set([f for f in FIXTURE.rglob("*") if f.is_file() and f.name != "package-lock.json"])


def lock_content_chars(out):
    """Characters of lockfile content in a tool result: lines found in the lockfile and in no other file."""
    n = 0
    for ln in out.splitlines():
        body = PREFIX_RE.sub("", ln, count=1).strip()
        if len(body) >= 6 and body in LOCK_LINES and body not in OTHER_LINES:
            n += len(ln) + 1
    return n


HOME = str(Path.home())


def scrub(text):
    """Published files never carry the machine's home folder (Claude Code's persisted-output notes do)."""
    return text.replace(HOME, "~")


def text_of(content):
    if isinstance(content, str):
        return content
    return "\n".join(c.get("text", "") for c in content if isinstance(c, dict))


def auto_grade(qid, answer, files_after):
    a = answer.lower()
    if qid == "express-version":
        return "4.22.3" in a
    if qid == "debug-versions":
        return "4.4.3" in a and "2.6.9" in a
    if qid == "cookie-cve":
        return "0.7.2" in a and re.search(r"(not|isn't|is not|aren't) affected", a) is not None
    if qid == "vite-version":
        return "5.4.21" in a
    if qid == "jwt-expired":
        return "auth.js" in a and "token_expired" in a and "401" in a
    if qid == "health-route":
        src = (files_after or {}).get("src/server.js") or ""
        m = re.search(r"app\.get\(\s*['\"]/health['\"]", src)
        auth = src.find("requireAuth, ordersRouter")
        return bool(m) and (auth == -1 or m.start() < auth) and "'ok'" in src.replace('"', "'")
    return None


def analyze_run(d):
    uses, results = {}, {}
    answer, res = "", None
    for e in d["events"]:
        if e.get("type") == "assistant":
            for c in e["message"]["content"]:
                if c["type"] == "tool_use":
                    uses[c["id"]] = c
        elif e.get("type") == "user" and isinstance(e["message"]["content"], list):
            for c in e["message"]["content"]:
                if c.get("type") == "tool_result":
                    results[c["tool_use_id"]] = c
        elif e.get("type") == "result":
            res = e
            answer = e.get("result", "")
    work = d["work_dir"]
    touches, ok, blocked, lock_chars = [], 0, 0, 0
    outside = []
    for uid, u in uses.items():
        inp = json.dumps(u["input"])
        # Only fields that point at files; search patterns and edited code are not paths.
        scan = " ".join(str(u["input"].get(k, "")) for k in ("file_path", "path", "command", "glob"))
        r = results.get(uid, {})
        out = text_of(r.get("content", ""))
        err = bool(r.get("is_error"))
        # Absolute or home paths only (a path segment like node_modules/express is not one).
        for p in re.findall(r"(?:^|[\s\"'=(:])((?:/|~|\.\./)[\w./~-]*)", scan):
            if not p.startswith(work) and p not in ("/dev/null", "/"):
                outside.append(p)
        named = bool(LOCK_RE.search(inp))
        got = 0 if u["name"] in ("Edit", "Write") else lock_content_chars(out)
        if not named and not got:
            continue
        denied = err and ("denied" in out.lower() or "permission" in out.lower())
        touches.append({"tool": u["name"], "named": named, "error": err, "denied": denied,
                        "chars": got, "input": scrub(inp[:300])})
        lock_chars += got
        ok += 1 if got > 0 else 0
        blocked += 1 if denied else 0
    u = res.get("usage", {}) if res else {}
    tokens_in = u.get("input_tokens", 0) + u.get("cache_creation_input_tokens", 0) + u.get(
        "cache_read_input_tokens", 0)
    return {
        "condition": d["condition"], "question": d["question"], "rep": d["rep"],
        "kind": QUESTIONS[d["question"]]["kind"],
        "tool_calls": len(uses), "lock_touches": len(touches), "lock_reads_ok": ok,
        "lock_denied": blocked, "lock_chars": lock_chars,
        "lock_tools_ok": "+".join(sorted({t["tool"] for t in touches if t["chars"] > 0})),
        "input_tokens": tokens_in, "output_tokens": u.get("output_tokens", 0),
        "cost_usd": round(res.get("total_cost_usd", 0), 4) if res else None,
        "turns": res.get("num_turns") if res else None, "wall_s": d["wall_seconds"],
        "auto_correct": auto_grade(d["question"], answer, d.get("files_after")),
        "outside_paths": len(outside),
        "_answer": scrub(answer), "_touches": touches, "_outside": outside,
    }


def main():
    runs_dir, out_dir = Path(sys.argv[1]), Path(sys.argv[2])
    out_dir.mkdir(parents=True, exist_ok=True)
    overrides = json.loads(GRADE_OVERRIDES.read_text()) if GRADE_OVERRIDES.exists() else {}
    rows = []
    for f in sorted(runs_dir.glob("*.json")):
        r = analyze_run(json.loads(f.read_text()))
        key = f.stem
        r["correct"] = overrides.get(key, {}).get("correct", r["auto_correct"])
        r["run"] = key
        rows.append(r)
    cols = ["run", "condition", "question", "rep", "kind", "tool_calls", "lock_touches", "lock_reads_ok",
            "lock_denied", "lock_chars", "lock_tools_ok", "input_tokens", "output_tokens", "cost_usd",
            "turns", "wall_s", "auto_correct", "correct", "outside_paths"]
    with open(out_dir / "runs.csv", "w", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=cols, extrasaction="ignore")
        w.writeheader()
        w.writerows(rows)
    with open(out_dir / "touches.jsonl", "w") as fh:
        for r in rows:
            for t in r["_touches"]:
                fh.write(json.dumps({"run": r["run"], **t}) + "\n")
    with open(out_dir / "answers.md", "w") as fh:
        for r in rows:
            fh.write(f"## {r['run']} (correct: {r['correct']})\n\n{r['_answer']}\n\n")

    by = defaultdict(list)
    for r in rows:
        by[(r["condition"], r["kind"])].append(r)
    lines = ["| Condition | Questions | Runs | Runs that got lockfile text | Denied calls | Lockfile chars "
             "(median run) | Correct | Input tokens (median) | Cost USD (total) |",
             "| --- | --- | --- | --- | --- | --- | --- | --- | --- |"]
    for c in CONDITIONS:
        for k in ["lockfile", "code"]:
            rs = by.get((c, k), [])
            if not rs:
                continue
            med = lambda xs: int(statistics.median(xs))
            lines.append(
                f"| {c} | {k} | {len(rs)} | {sum(1 for r in rs if r['lock_reads_ok'])} | "
                f"{sum(r['lock_denied'] for r in rs)} | {med([r['lock_chars'] for r in rs]):,} | "
                f"{sum(1 for r in rs if r['correct'])}/{len(rs)} | "
                f"{med([r['input_tokens'] for r in rs]):,} | {sum(r['cost_usd'] or 0 for r in rs):.2f} |")
    (out_dir / "summary.md").write_text("\n".join(lines) + "\n")
    print("\n".join(lines))
    flagged = [r["run"] for r in rows if r["outside_paths"]]
    print(f"\n{len(rows)} runs; runs with paths outside the work folder: {flagged}")


if __name__ == "__main__":
    main()
