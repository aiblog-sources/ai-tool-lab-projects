#!/usr/bin/env python3
"""parse_guard.py: a PreToolUse hook on Bash that parses the command (Python stdlib only) and blocks it when any part
of it would write under the protected folder. It fails closed: a write whose target it cannot work out (command
substitution, an unknown variable, a script piped into a shell) is blocked too.

What it understands: redirects (> >> >| &>), heredocs, ; && || | & and subshells, cd, VAR=value and $VAR / ${VAR},
globs, symlinks that exist and ones made by `ln -s` earlier in the same command, file commands (tee, sed -i, perl -i,
gawk -i inplace, cp, mv, install, rm, truncate, dd of=, ln, touch, chmod), editors (ed, ex, vi, vim), git mv / rm /
checkout / restore, find -exec / -delete, xargs, bash/sh -c, eval, source, and interpreters (python, node, perl,
ruby) whose inline code, heredoc or script file names the folder next to a write call.
What it does not: build tools (make, npm scripts), programs it has never heard of, anything that happens later.
"""
import glob, json, os, re, shlex, sys

PROTECTED = "tests"
SHELLS = {"bash", "sh", "dash", "zsh"}
INTERPRETERS = {"python", "python3", "node", "perl", "ruby"}
INLINE = {"python": "-c", "python3": "-c", "node": "-e", "perl": "-e", "ruby": "-e"}
EDITORS = {"ed", "ex", "vi", "vim", "nano"}
ALL_ARGS = {"tee", "rm", "unlink", "truncate", "touch", "shred", "chmod", "chown"}
VALUE_OPTS = {"truncate": {"-s", "-r"}, "chmod": set(), "vim": {"-c", "--cmd", "-S"}, "vi": {"-c"}, "ex": {"-c"},
              "install": {"-m", "-o", "-g", "-t"}, "cp": {"-t"}, "mv": {"-t"}, "tee": set()}
CODE_WRITE = re.compile(r"write|append|unlink|remove|rename|replace|rmtree|truncate|open\([^)]*['\"][wax+]|"
                        r"['\"][wax]\+?['\"]|copy|move|mkdir|symlink", re.I)
OPS = {";", "&&", "||", "|", "&", "(", ")", "|&"}


class Unsure(Exception):
    pass


class Hit(Exception):
    pass


def split_heredocs(cmd, bodies):
    """Cut heredoc bodies out of the command text; leave a __HEREDOCn__ word where the operator was."""
    out, lines, i = [], cmd.split("\n"), 0
    while i < len(lines):
        line, pending = lines[i], []

        def cut(m):
            pending.append((m.group(3), m.group(1) == "-"))
            return f" __HEREDOC{len(bodies) + len(pending) - 1}__ "
        line = re.sub(r"<<(-?)\s*(['\"]?)(\w+)\2", cut, line)
        out.append(line)
        i += 1
        for delim, strip in pending:
            body = []
            while i < len(lines) and (lines[i].lstrip("\t") if strip else lines[i]) != delim:
                body.append(lines[i])
                i += 1
            i += 1
            bodies.append("\n".join(body))
    return "\n".join(out)


def cut_substitutions(cmd, subs):
    """Replace $( ... ) and `...` with __SUBSTn__ words, keeping the inner commands for separate checks."""
    res, i = "", 0
    while i < len(cmd):
        if cmd.startswith("$(", i) and not cmd.startswith("$((", i):
            depth, j = 1, i + 2
            while j < len(cmd) and depth:
                depth += {"(": 1, ")": -1}.get(cmd[j], 0)
                j += 1
            subs.append(cmd[i + 2:j - 1])
            res += f"__SUBST{len(subs) - 1}__"
            i = j
        elif cmd[i] == "`":
            j = cmd.find("`", i + 1)
            j = len(cmd) if j < 0 else j
            subs.append(cmd[i + 1:j])
            res += f"__SUBST{len(subs) - 1}__"
            i = j + 1
        else:
            res += cmd[i]
            i += 1
    return res


class Checker:
    def __init__(self, root):
        self.root = os.path.realpath(root)
        self.prot = os.path.join(self.root, PROTECTED)
        self.aliases = set()

    def is_protected(self, word, st):
        word = self.expand(word, st)
        if "__SUBST" in word or "$" in word or "`" in word:
            raise Unsure(word)
        if word in ("/dev/null", "/dev/stdout", "/dev/stderr", "-"):
            return False
        word = os.path.expanduser(word)
        names = glob.glob(word, root_dir=st["cwd"]) if glob.has_magic(word) else []
        for name in names or [word]:
            full = os.path.normpath(os.path.join(st["cwd"], name))
            for f in {full, os.path.realpath(full)}:
                for base in {self.prot} | self.aliases:
                    if f == base or f.startswith(base + os.sep):
                        return True
        return False

    def expand(self, word, st):
        def var(m):
            name = m.group(1) or m.group(2)
            if name in st["vars"]:
                return st["vars"][name]
            if name == "PWD":
                return st["cwd"]
            if name == "HOME":
                return os.path.expanduser("~")
            return m.group(0)
        return re.sub(r"\$\{(\w+)\}|\$(\w+)", var, word)

    def target(self, word, st):
        if self.is_protected(word, st):
            raise Hit(word)

    def mentions(self, text):
        return re.search(r"(^|[^\w])" + re.escape(PROTECTED) + r"([/'\"\s]|$)", text) is not None

    def code(self, text):
        if self.mentions(text) and CODE_WRITE.search(text):
            raise Hit("interpreter code")

    def script(self, cmd, st):
        bodies, subs = [], []
        cmd = split_heredocs(cmd, bodies)
        cmd = cut_substitutions(cmd, subs)
        for s in subs:
            self.script(s, dict(st, vars=dict(st["vars"])))
        lex = shlex.shlex(cmd.replace("\n", " ; "), posix=True, punctuation_chars=";&|<>()")
        lex.whitespace_split = True
        try:
            tokens = list(lex)
        except ValueError:
            raise Unsure("unbalanced quotes")
        seg, prev_op, segs = [], None, []
        for t in tokens:
            if t in OPS:
                segs.append((seg, prev_op, t))
                seg, prev_op = [], t
            else:
                seg.append(t)
        segs.append((seg, prev_op, None))
        upstream = []
        for words, before, after in segs:
            if words:
                self.simple(words, st, bodies, piped=before in ("|", "|&"), upstream=upstream)
            upstream = upstream + words if after in ("|", "|&") else []

    def simple(self, words, st, bodies, piped, upstream):
        args, stdin = [], None
        i = 0
        while i < len(words):
            w = words[i]
            m = re.fullmatch(r"__HEREDOC(\d+)__", w)
            if m:
                stdin = bodies[int(m.group(1))]
            elif w in (">", ">>", ">|", "&>", "&>>", "<>") or re.fullmatch(r"\d*>>?\|?", w):
                if i + 1 < len(words):
                    self.target(words[i + 1], st)
                i += 1
            elif w in (">&", "<&", "<", "<<<"):
                i += 1
            elif args and args[-1].isdigit() and w.startswith(">"):
                pass
            else:
                args.append(w)
            i += 1
        while args and re.fullmatch(r"[A-Za-z_]\w*=.*", args[0]):  # VAR=value prefixes / assignments
            k, v = args.pop(0).split("=", 1)
            st["vars"][k] = self.expand(v, st)
        if not args:
            return
        if args[0] in ("sudo", "env", "nohup", "time", "command", "exec", "nice", "timeout"):
            rest = args[1:]
            while rest and (rest[0].startswith("-") or re.fullmatch(r"[A-Za-z_]\w*=.*", rest[0]) or
                            (args[0] == "timeout" and re.fullmatch(r"[\d.]+[smhd]?", rest[0]))):
                rest = rest[1:]
            return self.simple(rest, st, bodies, piped, upstream) if rest else None
        name = os.path.basename(args[0])
        a = [self.expand(x, st) for x in args[1:]]
        files = [x for x in a if not x.startswith("-")]
        if name in ("cd", "pushd"):
            d = files[0] if files else os.path.expanduser("~")
            if "__SUBST" in d or "$" in d:
                raise Unsure(d)
            st["cwd"] = os.path.normpath(os.path.join(st["cwd"], os.path.expanduser(d)))
        elif name in ALL_ARGS:
            for x in self.positional(name, a):
                self.target(x, st)
        elif name in ("cp", "install"):
            pos = self.positional(name, a)
            if "-t" in a:
                self.target(a[a.index("-t") + 1], st)
            elif pos:
                self.target(pos[-1], st)
        elif name == "mv":
            for x in self.positional(name, a):
                self.target(x, st)
        elif name == "ln":
            pos = self.positional(name, a)
            if pos:
                self.target(pos[-1], st)
                if len(pos) >= 2 and self.is_protected(pos[0], st):
                    self.aliases.add(os.path.normpath(os.path.join(st["cwd"], pos[-1])))
        elif name == "dd":
            for x in a:
                if x.startswith("of="):
                    self.target(x[3:], st)
        elif name == "sed":
            if any(x.startswith("-i") or x.startswith("--in-place") or re.fullmatch(r"-[a-zA-Z]*i\S*", x) for x in a):
                pos = self.positional(name, a, consume={"-e", "-f"})
                has_e = "-e" in a or "-f" in a
                for x in (pos if has_e else pos[1:]):
                    self.target(x, st)
        elif name == "perl" and any(re.fullmatch(r"-\w*i\S*", x) for x in a):
            pos = self.positional(name, a, consume={"-e", "-E"})
            for x in (pos if ("-e" in a or "-E" in a) else pos[1:]):
                self.target(x, st)
        elif name in ("awk", "gawk"):
            if "inplace" in a:
                pos = [x for x in a if not x.startswith("-") and x != "inplace"]
                for x in pos[1:]:
                    self.target(x, st)
        elif name in EDITORS:
            for x in self.positional(name, a):
                self.target(x, st)
        elif name == "git" and files:
            sub = files[0]
            if sub in ("mv", "rm", "checkout", "restore"):
                for x in a[a.index(sub) + 1:]:
                    if not x.startswith("-") and x != "--":
                        self.target(x, st)
        elif name == "find":
            if "-delete" in a or "-exec" in a or "-execdir" in a or "-ok" in a:
                starts = []
                for x in a:
                    if x.startswith("-") or x in ("(", "!"):
                        break
                    starts.append(x)
                inner = []
                for flag in ("-exec", "-execdir", "-ok"):
                    if flag in a:
                        inner = a[a.index(flag) + 1:]
                if "-delete" in a or self.writes([w for w in inner if w not in ("{}", "+", ";")], st):
                    for x in starts or ["."]:
                        self.target(x, st)
        elif name == "xargs":
            k = 0
            while k < len(a) and a[k].startswith("-"):
                k += 1
            inner = a[k:]
            if inner and self.writes(inner, st):
                for w in upstream:
                    self.target(w, st) if not w.startswith("-") else None
        elif name == "eval":
            self.script(" ".join(a), st)
        elif name in ("source", "."):
            self.script_file(files[0] if files else "", st, shell=True)
        elif name in SHELLS:
            if "-c" in a:
                idx = a.index("-c")
                if idx + 1 < len(a):
                    self.script(a[idx + 1], dict(st, vars=dict(st["vars"])))
            elif files:
                self.script_file(files[0], st, shell=True)
            elif stdin is not None:
                self.script(stdin, dict(st, vars=dict(st["vars"])))
            elif piped:
                raise Unsure("script piped into a shell")
        elif name in INTERPRETERS or re.fullmatch(r"python3(\.\d+)?", name):
            flag = INLINE.get(name, "-c")
            if flag in a and a.index(flag) + 1 < len(a):
                self.code(a[a.index(flag) + 1])
            elif stdin is not None:
                self.code(stdin)
            elif files and files[0] != "-":
                if "-m" not in a:
                    self.script_file(files[0], st, shell=False)
            elif piped:
                raise Unsure("code piped into an interpreter")

    def writes(self, words, st):
        """Would this command write to its file arguments? Used for find -exec and xargs."""
        return self._writer(words)

    def _writer(self, words):
        n = os.path.basename(words[0]) if words else ""
        return n in ALL_ARGS | EDITORS | {"cp", "mv", "install", "ln", "dd"} or \
            (n == "sed" and any(re.fullmatch(r"-[a-zA-Z]*i\S*|--in-place\S*", w) for w in words)) or \
            (n == "perl" and any(re.fullmatch(r"-\w*i\w*", w) for w in words))

    def script_file(self, path, st, shell):
        full = os.path.join(st["cwd"], self.expand(path, st))
        try:
            text = open(full, encoding="utf-8", errors="replace").read()
        except OSError:
            return
        if shell:
            self.script(text, dict(st, vars=dict(st["vars"])))
        else:
            self.code(text)

    def positional(self, name, a, consume=None):
        consume = (consume or set()) | VALUE_OPTS.get(name, set())
        out, skip = [], False
        for x in a:
            if skip:
                skip = False
                continue
            if x in consume:
                skip = True
                continue
            if x.startswith("-") and x != "-":
                continue
            out.append(x)
        return out


def check(cmd, cwd, root):
    c = Checker(root)
    st = {"cwd": os.path.realpath(cwd), "vars": {}}
    try:
        c.script(cmd, st)
    except Hit as e:
        return "block", f"writes to {PROTECTED}/ ({e})"
    except Unsure as e:
        return "block", f"cannot tell where this command writes ({e})"
    return "allow", ""


if __name__ == "__main__":
    data = json.load(sys.stdin)
    cmd = (data.get("tool_input") or {}).get("command") or ""
    root = os.environ.get("CLAUDE_PROJECT_DIR") or data.get("cwd") or "."
    verdict, why = check(cmd, data.get("cwd") or root, root)
    if verdict == "block":
        print(f"Blocked: {why}. Files under {PROTECTED}/ are protected.", file=sys.stderr)
        sys.exit(2)
    sys.exit(0)
