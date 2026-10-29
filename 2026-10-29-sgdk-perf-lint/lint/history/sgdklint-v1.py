#!/usr/bin/env python3
"""sgdklint: flag C code that is slow on the Mega Drive, in the code that runs every frame.

usage: sgdklint.py [--all] [--json out.json] path [path ...]

Paths are .c/.h files or folders (searched recursively). Every finding names the
measured cost on a 68000 at 7.67 MHz (NTSC frame = 128,005 cycles), from the
cost-table ROM in bench/ profiled with mdprof (see results/costs.txt).

How "every frame" is decided: an infinite loop (while (TRUE), while (1),
for (;;)) is a game loop; its body and every function it calls, directly or
through other functions, run every frame. So do functions passed to
SYS_setVIntCallback / SYS_setVBlankCallback / SYS_setHIntCallback. Calls through
function pointers are not followed. A finding inside an if/switch in that code is
"guarded" (it may run only on some frames). Code outside the frame path (init,
title screens) is reported only with --all.

No dependencies beyond Python 3.8. MIT.
"""
import json
import re
import sys
from collections import defaultdict
from pathlib import Path

FRAME = 128_005

# measured on the cost-table ROM (results/costs.txt), cycles per call on top of a
# loop that does an add instead
COST = {
    "div_s32": 617, "div_u32": 453, "div_s16": 613, "mod_s32": 1041, "div_s32_const": 623,
    "div_u16": 117, "mul_s32": 268, "mul_s16": 35,
    "float_mul": 2285, "float_div": 1221, "float_add": 1147, "float_to_int": 1046,
    "tile_xy": 324, "tile_fill_per_tile": 18, "tile_datarect_per_tile": 37,
    "text_19": 3673, "sprintf": 12582, "inttostr": 2881,
    "spr_add_release": 1865,
    "load_tiles_64_cpu": 17604, "load_tiles_64_dma": 6046,
    "image_none": 22630, "image_lz4w": 138263, "image_aplib": 549647,
}

RULES = {
    "div": "division or modulo that compiles to a libgcc call",
    "mul32": "32-bit multiply (libgcc __mulsi3)",
    "float": "float/double (software floating point)",
    "tile-loop": "one VDP_setTileMapXY call per tile in a loop",
    "redraw": "image, tileset or map upload every frame",
    "text": "text drawing or number formatting every frame",
    "sprite-alloc": "sprite allocated or released every frame",
    "blocking": "call that waits for frames inside the game loop",
}

T16U = {"u8", "u16", "vu8", "vu16", "uint8_t", "uint16_t", "bool", "BYTE", "WORD"}
T16S = {"s8", "s16", "vs8", "vs16", "fix16", "fastfix16", "f16", "int8_t", "int16_t", "short", "char", "vfix16"}
T32 = {"s32", "u32", "vs32", "vu32", "fix32", "fastfix32", "f32", "int", "long", "int32_t", "uint32_t",
       "size_t", "unsigned", "signed", "vfix32"}
TFLOAT = {"float", "double"}
TYPES = T16U | T16S | T32 | TFLOAT
QUALIFIERS = {"const", "static", "volatile", "register", "extern", "inline", "NO_INLINE", "FORCE_INLINE"}
KEYWORDS = {"if", "for", "while", "do", "switch", "return", "sizeof", "case", "default", "else", "break",
            "continue", "goto", "typedef", "struct", "union", "enum"}

TEXT_CALLS = {"VDP_drawText", "VDP_drawTextBG", "VDP_drawTextEx", "VDP_drawTextFill", "VDP_clearText",
              "sprintf", "intToStr", "uintToStr", "fix16ToStr", "fix32ToStr", "F16_toStr", "F32_toStr",
              "intToHex", "BMP_drawText", "strcat", "strcpy"}
REDRAW_CALLS = {"VDP_drawImage", "VDP_drawImageEx", "VDP_loadTileSet", "VDP_loadTileData", "VDP_loadFont",
                "MAP_create", "unpack", "unpackImage", "unpackTileSet", "unpackTileMap", "VDP_setTileMapEx",
                "VDP_drawBitmap", "VDP_drawBitmapEx", "BMP_loadBitmap", "VDP_loadBMPTileData",
                "VDP_setTileMap"}
SPRITE_ALLOC = {"SPR_addSprite", "SPR_addSpriteEx", "SPR_addSpriteSafe", "SPR_addSpriteExSafe",
                "SPR_releaseSprite", "SPR_init", "SPR_initEx", "SPR_reset"}
WAIT_CALLS = {"waitMs", "waitTick", "waitSubTick", "JOY_waitPress", "JOY_waitPressBtn",
              "JOY_waitPressBtnTime", "JOY_waitPressTime"}
FADE_CALLS = {"PAL_fadeOut", "PAL_fadeIn", "PAL_fadeTo", "PAL_fadeOutAll", "PAL_fadeInAll", "PAL_fadeToAll",
              "PAL_fade", "PAL_fadePalette", "PAL_fadeInPalette", "PAL_fadeOutPalette", "PAL_fadeToPalette"}
FRAME_WAITS = {"SYS_doVBlankProcess", "SYS_doVBlankProcessEx", "VDP_waitVSync", "VDP_waitVBlank",
               "VDP_waitVInt", "SYS_waitVBlank"}
FIX_MACROS = {"FIX16": (6, "s16"), "F16": (6, "s16"), "FIX32": (10, "32"), "F32": (10, "32"),
              "FASTFIX16": (8, "s16"), "FF16": (8, "s16"), "FASTFIX32": (16, "32"), "FF32": (16, "32")}
CALLBACK_SETTERS = {"SYS_setVIntCallback", "SYS_setVBlankCallback", "SYS_setHIntCallback",
                    "SYS_setVIntPreCallback"}

TOKEN = re.compile(r"""
    (?P<ws>\s+)
  | (?P<num>0[xX][0-9a-fA-F]+[uUlL]*|\d+\.\d*(?:[eE][-+]?\d+)?[fFlL]?|\.\d+(?:[eE][-+]?\d+)?[fFlL]?|\d+[eE][-+]?\d+[fFlL]?|\d+[uUlLfF]*)
  | (?P<id>[A-Za-z_]\w*)
  | (?P<op>->|\+\+|--|<<=|>>=|<<|>>|<=|>=|==|!=|&&|\|\||[-+*/%&|^!=<>]=|[{}()\[\];,.?:~!<>=+\-*/%&|^])
  | (?P<other>.)
""", re.X)


class Tok:
    __slots__ = ("kind", "text", "line")

    def __init__(self, kind, text, line):
        self.kind, self.text, self.line = kind, text, line

    def __repr__(self):
        return f"{self.text}@{self.line}"


def strip_source(src):
    """remove comments, strings, char literals and preprocessor lines, keeping line numbers"""
    out, i, n = [], 0, len(src)
    while i < n:
        c = src[i]
        if src.startswith("//", i):
            j = src.find("\n", i)
            i = n if j < 0 else j
        elif src.startswith("/*", i):
            j = src.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append("\n" * src.count("\n", i, j))
            i = j
        elif c in "\"'":
            j = i + 1
            while j < n and src[j] != c:
                j += 2 if src[j] == "\\" else 1
            out.append('""' if c == '"' else "0")
            out.append("\n" * src.count("\n", i, j))
            i = j + 1
        else:
            out.append(c)
            i += 1
    text = "".join(out)
    lines, cont = [], False
    for line in text.split("\n"):
        is_pp = cont or line.lstrip().startswith("#")
        cont = is_pp and line.rstrip().endswith("\\")
        lines.append("" if is_pp else line)
    return "\n".join(lines)


def tokenize(src):
    toks, line = [], 1
    for m in TOKEN.finditer(src):
        kind = m.lastgroup
        text = m.group()
        if kind == "ws":
            line += text.count("\n")
            continue
        toks.append(Tok(kind, text, line))
    return toks


def match(toks, i):
    """index of the bracket closing toks[i]"""
    pairs = {"(": ")", "[": "]", "{": "}"}
    o, c, d = toks[i].text, pairs[toks[i].text], 0
    for j in range(i, len(toks)):
        if toks[j].text == o:
            d += 1
        elif toks[j].text == c:
            d -= 1
            if d == 0:
                return j
    return len(toks) - 1


# --- types ----------------------------------------------------------------

def collect_types(toks, types):
    """name -> set of width classes ('u16', 's16', '32', 'float', 'ptr') from declarations"""
    i, n = 0, len(toks)
    while i < n:
        t = toks[i]
        if t.kind == "id" and t.text in TYPES:
            words = [t.text]
            j = i + 1
            while j < n and toks[j].kind == "id" and (toks[j].text in TYPES or toks[j].text in QUALIFIERS):
                words.append(toks[j].text)
                j += 1
            cls = width(words)
            # declarators: [*] name [ [..] ] [= init] , ...
            while j < n:
                ptr = False
                while j < n and toks[j].text in ("*", "const", "volatile"):
                    ptr = ptr or toks[j].text == "*"
                    j += 1
                if j < n and toks[j].kind == "id" and toks[j].text not in KEYWORDS:
                    name = toks[j].text
                    j += 1
                    if j < n and toks[j].text == "(":  # function: its return type, skip
                        break
                    arr = False
                    while j < n and toks[j].text == "[":
                        arr = True
                        j = match(toks, j) + 1
                    types[name].add("ptr" if ptr else cls)
                    if arr and not ptr:
                        types[name + "[]"].add(cls)
                    if j < n and toks[j].text == "=":
                        d = 0
                        while j < n:
                            x = toks[j].text
                            if x in "([{":
                                d += 1
                            elif x in ")]}":
                                if d == 0:
                                    break
                                d -= 1
                            elif x in ",;" and d == 0:
                                break
                            j += 1
                    if j < n and toks[j].text == ",":
                        j += 1
                        continue
                break
            i = j
        else:
            i += 1


def width(words):
    w = [x for x in words if x not in QUALIFIERS]
    if any(x in TFLOAT for x in w):
        return "float"
    if "unsigned" in w and ("char" in w or "short" in w):
        return "u16"
    if "short" in w or "char" in w:
        return "s16"
    for x in w:
        if x in T16U:
            return "u16"
        if x in T16S:
            return "s16"
    return "32"


# --- operand extraction -----------------------------------------------------

def operand_left(toks, i):
    """tokens of the primary expression ending just before toks[i]"""
    j = i - 1
    while j >= 0:
        t = toks[j]
        if t.text in (")", "]"):
            d, k = 0, j
            while k >= 0:
                if toks[k].text in (")", "]"):
                    d += 1
                elif toks[k].text in ("(", "["):
                    d -= 1
                    if d == 0:
                        break
                k -= 1
            j = k - 1
            # call or index: keep the name before it
            if j >= 0 and (toks[j].kind == "id" and toks[j].text not in KEYWORDS):
                continue
            if j >= 0 and toks[j].text in ("]", ")"):
                continue
            break
        if t.kind in ("id", "num"):
            j -= 1
            if j >= 0 and toks[j].text in (".", "->"):
                j -= 1
                continue
            break
        break
    return toks[j + 1:i]


def operand_right(toks, i):
    """tokens of the primary expression starting at toks[i] (casts and unary minus included)"""
    j, n = i, len(toks)
    while j < n and toks[j].text in ("-", "+", "~", "!", "*", "&"):
        j += 1
    if j < n and toks[j].text == "(":
        k = match(toks, j)
        inner = toks[j + 1:k]
        if inner and all(t.kind == "id" and (t.text in TYPES or t.text in QUALIFIERS) or t.text == "*"
                         for t in inner):
            return toks[i:operand_right_end(toks, k + 1)]  # cast
        return toks[i:operand_postfix(toks, k + 1)]
    return toks[i:operand_postfix(toks, j)]


def operand_right_end(toks, j):
    sub = operand_right(toks, j)
    return j + len(sub)


def operand_postfix(toks, j):
    n = len(toks)
    if j < n and toks[j].kind in ("id", "num"):
        j += 1
    while j < n:
        if toks[j].text in ("(", "["):
            j = match(toks, j) + 1
        elif toks[j].text in (".", "->") and j + 1 < n:
            j += 2
        else:
            break
    return j


def classify(op, types):
    """width class of an operand token list: 'const:<value>', 'u16', 's16', '32', 'float', None (unknown)"""
    if not op:
        return None
    texts = [t.text for t in op]
    # a leading cast decides
    if texts[0] == "(":
        k = 1
        words = []
        while k < len(op) and op[k].kind == "id" and (op[k].text in TYPES or op[k].text in QUALIFIERS):
            words.append(op[k].text)
            k += 1
        if words and k < len(op) and op[k].text == ")":
            return width(words)
        if len(op) >= 2 and texts[-1] == ")" and match(op, 0) == len(op) - 1:
            inner = op[1:-1]
            depth, arith = 0, False
            for k, t in enumerate(inner):
                if t.text in "([":
                    depth += 1
                elif t.text in ")]":
                    depth -= 1
                elif depth == 0 and k > 0 and t.text in ("+", "-", "*", "<<", "|", "^", "&"):
                    arith = True
            if arith:
                # C promotes u16/s16 to int inside arithmetic, and the result may be
                # negative: (u16 - 240) / 30 compiles to __divsi3 (640 cycles)
                return "float" if classify_any_float(inner, types) else "32"
            return classify(inner, types)
    if len(op) == 1 and op[0].kind == "num" or (len(op) == 2 and texts[0] == "-" and op[1].kind == "num"):
        num = op[-1].text
        if re.search(r"[.eE]", num) and not num.lower().startswith("0x") or num[-1] in "fF" and "x" not in num:
            return "float"
        v = int(re.sub(r"[uUlL]+$", "", num), 0)
        return f"const:{-v if texts[0] == '-' else v}"
    if texts[0] in FIX_MACROS:
        # FIX16(1.5) is a constant folded by the compiler (SGDK 2.x frac bits)
        frac, cls = FIX_MACROS[texts[0]]
        if len(op) == 4 and op[2].kind == "num":
            try:
                return f"const:{int(float(op[2].text.rstrip('fFlLuU')) * (1 << frac))}"
            except ValueError:
                pass
        return cls
    if texts[0] == "sizeof":
        return "32"
    # name of the last member / variable / array before indexing
    names = [t.text for t in op if t.kind == "id"]
    if not names:
        return None
    last = names[-1]
    if texts[-1] == ")":  # a call: return type unknown, except SGDK maths helpers
        fn = None
        for k, t in enumerate(op):
            if t.text == "(":
                fn = op[k - 1].text if k else None
                break
        if fn in ("divu", "modu", "mulu"):
            return "32" if fn == "mulu" else "u16"
        if fn in ("divs", "mods", "F16_mul", "F16_div", "abs"):
            return "s16"
        return None
    if texts[-1] == "]":
        # arr[i] or s->arr[i]: the name before the first [
        k = texts.index("[")
        last = op[k - 1].text if k else last
        cls = types.get(last + "[]") or types.get(last)
    else:
        cls = types.get(last)
    if not cls:
        return None
    cls = set(cls) - {"ptr"} if texts[-1] == "]" else set(cls)
    if len(cls) == 1:
        return next(iter(cls))
    return None


def classify_any_float(toks, types):
    return any((t.kind == "num" and classify([t], types) == "float") or
               (t.kind == "id" and types.get(t.text) == {"float"}) for t in toks)


def pow2(v):
    return v > 0 and v & (v - 1) == 0


# --- statement walk -----------------------------------------------------------

class Ctx:
    __slots__ = ("loops", "guards", "inf")

    def __init__(self, loops=0, guards=0, inf=False):
        self.loops, self.guards, self.inf = loops, guards, inf

    def loop(self):
        return Ctx(self.loops + 1, self.guards, self.inf)

    def guard(self):
        return Ctx(self.loops, self.guards + 1, self.inf)

    def infinite(self):
        # the game loop body: counted as frame level, not as an inner loop
        return Ctx(0, 0, True)


class Site:
    """something found in a function: an operation or a call"""
    __slots__ = ("kind", "name", "line", "loops", "guards", "inf", "detail", "loop_frame_waits")

    def __init__(self, kind, name, line, ctx, detail=None):
        self.kind, self.name, self.line, self.detail = kind, name, line, detail
        self.loops, self.guards, self.inf = ctx.loops, ctx.guards, ctx.inf
        self.loop_frame_waits = 0


class Func:
    def __init__(self, name, path, line):
        self.name, self.path, self.line = name, path, line
        self.sites = []
        self.game_loops = []  # (first line, frame-wait call count) per infinite loop


def is_infinite(cond):
    t = [x.text for x in cond]
    return t in (["TRUE"], ["1"], ["true"], ["(", "TRUE", ")"], ["(", "1", ")"]) or t == [";", ";"]


class Walker:
    def __init__(self, toks, func, types):
        self.t, self.f, self.types = toks, func, types

    def block(self, i, end, ctx):
        while i < end:
            i = self.stmt(i, end, ctx)

    def stmt(self, i, end, ctx):
        t = self.t
        if i >= end:
            return end
        x = t[i].text
        if x == "{":
            k = match(t, i)
            self.block(i + 1, k, ctx)
            return k + 1
        if x in ("for", "while") and i + 1 < end and t[i + 1].text == "(":
            k = match(t, i + 1)
            cond = t[i + 2:k]
            self.expr(cond, ctx)
            inf = is_infinite(cond) if x == "while" else self.for_infinite(cond)
            if inf:
                start = len(self.f.sites)
                nxt = self.stmt(k + 1, end, ctx.infinite())
                waits = sum(1 for s in self.f.sites[start:]
                            if s.kind == "call" and s.name in FRAME_WAITS and s.inf and s.loops == 0
                            and s.guards == 0)
                self.f.game_loops.append((t[i].line, waits))
                for s in self.f.sites[start:]:
                    s.loop_frame_waits = waits
                return nxt
            return self.stmt(k + 1, end, ctx.loop())
        if x == "do":
            body_ctx = ctx.loop()
            j = self.stmt(i + 1, end, body_ctx)
            if j < end and t[j].text == "while" and j + 1 < end and t[j + 1].text == "(":
                k = match(t, j + 1)
                self.expr(t[j + 2:k], ctx)
                j = k + 1
            return j + 1 if j < end and t[j].text == ";" else j
        if x in ("if", "switch") and i + 1 < end and t[i + 1].text == "(":
            k = match(t, i + 1)
            self.expr(t[i + 2:k], ctx)
            j = self.stmt(k + 1, end, ctx.guard())
            if x == "if" and j < end and t[j].text == "else":
                j = self.stmt(j + 1, end, ctx.guard())
            return j
        if x in ("case", "default"):
            j = i
            while j < end and t[j].text != ":":
                j += 1
            return j + 1
        if x in ("else",):
            return self.stmt(i + 1, end, ctx)
        # expression or declaration up to ';' at depth 0
        j, d = i, 0
        while j < end:
            y = t[j].text
            if y in "([{":
                d += 1
            elif y in ")]}":
                d -= 1
                if d < 0:
                    break
            elif y == ";" and d == 0:
                break
            j += 1
        self.expr(t[i:j], ctx)
        return j + 1

    @staticmethod
    def for_infinite(cond):
        semis = [k for k, x in enumerate(cond) if x.text == ";"]
        return len(semis) == 2 and semis[1] - semis[0] == 1

    def expr(self, toks, ctx):
        f = self.f
        # float literals inside FIX16(..) and friends are folded at compile time
        folded = set()
        for k, tk in enumerate(toks):
            if tk.text in FIX_MACROS and k + 1 < len(toks) and toks[k + 1].text == "(":
                folded.update(range(k + 1, match(toks, k + 1) + 1))
        for k, tk in enumerate(toks):
            if k in folded and tk.kind == "num":
                continue
            if tk.kind == "id" and k + 1 < len(toks) and toks[k + 1].text == "(" and tk.text not in KEYWORDS \
                    and tk.text not in TYPES:
                close = match(toks, k + 1)
                args = split_args(toks[k + 2:close])
                f.sites.append(Site("call", tk.text, tk.line, ctx, args))
            elif tk.kind == "num" and classify([tk], self.types) == "float":
                f.sites.append(Site("floatlit", tk.text, tk.line, ctx))
            elif tk.kind == "id" and tk.text in TFLOAT:
                f.sites.append(Site("floattype", tk.text, tk.line, ctx))
            elif tk.text in ("/", "%", "/=", "%=", "*", "*="):
                if k == 0:
                    continue
                prev = toks[k - 1]
                binary = prev.kind in ("id", "num") or prev.text in (")", "]")
                if not binary or (prev.kind == "id" and prev.text in TYPES):
                    continue  # unary * (dereference) or pointer declarator
                left = classify(operand_left(toks, k), self.types)
                right = classify(operand_right(toks, k + 1), self.types)
                f.sites.append(Site("op", tk.text.rstrip("="), tk.line, ctx, (left, right)))


def split_args(toks):
    args, cur, d = [], [], 0
    for t in toks:
        if t.text in "([{":
            d += 1
        elif t.text in ")]}":
            d -= 1
        if t.text == "," and d == 0:
            args.append(cur)
            cur = []
        else:
            cur.append(t)
    if cur:
        args.append(cur)
    return args


def find_functions(toks, path, types):
    funcs = []
    i, n = 0, len(toks)
    while i < n:
        t = toks[i]
        if t.text in ("struct", "union", "enum") and i + 1 < n:
            j = i + 1
            while j < n and toks[j].text not in ("{", ";", "(", ")"):
                j += 1
            if j < n and toks[j].text == "{":
                i = match(toks, j) + 1
                continue
            i += 1
            continue
        if t.text == "=":
            # initializer at file scope: skip to ';'
            d = 0
            while i < n:
                if toks[i].text in "([{":
                    d += 1
                elif toks[i].text in ")]}":
                    d -= 1
                elif toks[i].text == ";" and d == 0:
                    break
                i += 1
            i += 1
            continue
        if t.kind == "id" and t.text not in KEYWORDS and i + 1 < n and toks[i + 1].text == "(":
            k = match(toks, i + 1)
            j = k + 1
            # K&R-style attributes after the parameter list are rare in SGDK code; allow one
            while j < n and toks[j].kind == "id" and toks[j].text not in KEYWORDS:
                j += 1
                if j < n and toks[j].text == "(":
                    j = match(toks, j) + 1
            if j < n and toks[j].text == "{":
                end = match(toks, j)
                f = Func(t.text, path, t.line)
                Walker(toks, f, types).block(j + 1, end, Ctx())
                funcs.append(f)
                i = end + 1
                continue
            i = k + 1
            continue
        if t.text == "{":
            i = match(toks, i) + 1
            continue
        i += 1
    return funcs


# --- per-frame propagation and findings ------------------------------------------

def frame_context(funcs):
    """function name -> (inner loop depth on the frame path, guarded?) for functions that run every frame"""
    by_name = defaultdict(list)
    for f in funcs:
        by_name[f.name].append(f)
    ctx = {}
    work = []

    def offer(name, loops, guarded):
        if name not in by_name:
            return
        old = ctx.get(name)
        new = (min(loops, 3), guarded)
        if old is None or new[0] > old[0] or (not new[1] and old[1]):
            merged = (max(new[0], old[0]) if old else new[0], (old[1] if old else True) and new[1])
            if merged != old:
                ctx[name] = merged
                work.append(name)

    # roots: calls in game loops, and callback setters anywhere
    for f in funcs:
        for s in f.sites:
            if s.kind == "call" and s.inf:
                offer(s.name, s.loops, s.guards > 0)
            if s.kind == "call" and s.name in CALLBACK_SETTERS and s.detail:
                arg = [t.text for t in s.detail[-1] if t.kind == "id"]
                if arg:
                    offer(arg[-1], 0, False)
    while work:
        name = work.pop()
        loops, guarded = ctx[name]
        for f in by_name[name]:
            for s in f.sites:
                if s.kind == "call":
                    offer(s.name, loops + s.loops, guarded or s.guards > 0)
    return ctx


def site_level(f, s, fctx):
    """(per_frame, loops, guarded) for a site"""
    if s.inf:
        return True, s.loops, s.guards > 0
    if f.name in fctx:
        loops, guarded = fctx[f.name]
        return True, loops + s.loops, guarded or s.guards > 0
    return False, s.loops, s.guards > 0


def judge_op(s):
    """(rule, message) or None for a / % * site"""
    left, right = s.detail
    op = s.name

    def const(c):
        return int(c.split(":")[1]) if c and c.startswith("const:") else None

    lc, rc = const(left), const(right)
    if op in ("/", "%"):
        if rc is not None and pow2(abs(rc)):
            return None  # shift or mask
        if lc is not None and rc is not None:
            return None
        if "float" in (left, right):
            return None  # reported by the float rule
        small_u = lambda c: c == "u16" or (const(c) is not None and 0 <= const(c) <= 0xFFFF)
        if small_u(left) and small_u(right):
            return None  # divu (117 cycles) or a multiply by reciprocal (77)
        if left is None or right is None:
            known = left if right is None else right
            if known in ("u16",) or (const(known) is not None):
                cost = "453 to 1,041 cycles if the other operand is 32-bit or signed, 117 if both are u16"
            else:
                cost = "453 to 1,041 cycles unless both operands are u16"
            return "div", f"'{op}' with an operand of unknown type: {cost}"
        what = {"32": "32-bit", "s16": "signed 16-bit", "u16": "u16"}
        lw = what.get(left, "constant") if const(left) is None else "constant"
        rw = what.get(right, "constant") if const(right) is None else "constant"
        if op == "%":
            cost = f"{COST['mod_s32']:,} cycles (s32 % s32); u16 % u16 is 126"
        elif rc is not None:
            cost = f"about {COST['div_s32_const']:,} cycles (measured: s32 / 10 is 623, (u16 - 240) / 30 is 640; u16 / 10 is 77, / 8 is 23)"
        elif "s16" in (left, right) and "32" not in (left, right):
            cost = f"{COST['div_s16']:,} cycles: GCC calls __divsi3 for s16 / s16; SGDK divs() is 139"
        else:
            cost = f"{COST['div_s32']:,} cycles (s32 / s32); SGDK divu()/divs() is 133-139, a shift is 23"
        return "div", f"'{op}' on {lw} / {rw}: {cost}"
    # multiply
    if lc is not None or rc is not None:
        return None
    if "float" in (left, right):
        return None
    if left == "32" and right == "32":
        return "mul32", (f"32 x 32-bit multiply: {COST['mul_s32']} cycles (__mulsi3); "
                         f"muls()/mulu() on 16-bit values is 35, F16_mul 59")
    return None


def findings_for(funcs, show_all):
    fctx = frame_context(funcs)
    out = []
    for f in funcs:
        float_lines = set()
        for s in f.sites:
            per_frame, loops, guarded = site_level(f, s, fctx)
            if not per_frame and not show_all:
                continue
            rule = msg = None
            if s.kind == "op":
                r = judge_op(s)
                if r:
                    rule, msg = r
            elif s.kind in ("floatlit", "floattype"):
                if s.line in float_lines:
                    continue
                float_lines.add(s.line)
                rule = "float"
                msg = (f"float/double: software floating point, {COST['float_add']:,} (add) to "
                       f"{COST['float_mul']:,} (multiply) cycles per operation plus {COST['float_to_int']:,} per "
                       f"conversion to int; use fix16/fix32 (F16_mul 59 cycles)")
            elif s.kind == "call":
                n = s.name
                if n in ("VDP_setTileMapXY", "VDP_setTileMapXYEx") and (loops > 0):
                    rule = "tile-loop"
                    msg = (f"{n} in a loop: {COST['tile_xy']} cycles per tile; VDP_fillTileMapRect is "
                           f"{COST['tile_fill_per_tile']} per tile, VDP_setTileMapDataRect "
                           f"{COST['tile_datarect_per_tile']} (one 40-tile row: 12,966 vs 732 vs 1,470)")
                elif n in REDRAW_CALLS:
                    rule = "redraw"
                    msg = (f"{n} on the frame path: a 128x64 VDP_drawImageEx costs {COST['image_none']:,} cycles "
                           f"uncompressed (18% of a frame), {COST['image_lz4w']:,} with FAST/LZ4W (1.1 frames), "
                           f"{COST['image_aplib']:,} with APLIB (4.3 frames); 64 tiles by CPU {COST['load_tiles_64_cpu']:,}")
                elif n in TEXT_CALLS:
                    rule = "text"
                    if n == "sprintf":
                        msg = f"sprintf on the frame path: {COST['sprintf']:,} cycles (10% of a frame) for \"SCORE %ld\""
                    elif n in ("intToStr", "uintToStr"):
                        msg = f"{n} on the frame path: {COST['inttostr']:,} cycles for a 7-digit number"
                    else:
                        msg = f"{n} on the frame path: {COST['text_19']:,} cycles for 19 characters"
                    msg += "; redraw only when the value changes"
                elif n in SPRITE_ALLOC:
                    rule = "sprite-alloc"
                    msg = (f"{n} on the frame path: add + release costs {COST['spr_add_release']:,} cycles, and an "
                           f"add without a release leaks a sprite slot every frame; allocate once, then "
                           f"SPR_setVisibility / SPR_setPosition")
                elif n in WAIT_CALLS:
                    rule = "blocking"
                    msg = f"{n} inside the game loop stops the game for the whole wait"
                elif n in FADE_CALLS:
                    async_arg = s.detail[-1] if s.detail else []
                    if [t.text for t in async_arg] in (["FALSE"], ["0"], ["false"]):
                        rule = "blocking"
                        msg = f"{n}(..., FALSE) blocks until the fade ends; pass TRUE (async) inside the game loop"
                elif n in FRAME_WAITS and s.inf and s.loop_frame_waits > 1 and loops == 0 and not guarded:
                    rule = "blocking"
                    msg = (f"{s.loop_frame_waits} frame waits in one pass of the game loop: the game runs at "
                           f"60/{s.loop_frame_waits} fps")
            if not rule:
                continue
            if not per_frame:
                level = "init"
            elif guarded:
                level = "guarded"
            elif loops > 0:
                level = "hot"
            else:
                level = "frame"
            out.append(dict(file=str(f.path), line=s.line, function=f.name, rule=rule, level=level,
                            loops=loops, message=msg))
    # one finding per (file, line, rule)
    seen, uniq = set(), []
    for x in sorted(out, key=lambda d: (d["file"], d["line"], d["rule"])):
        key = (x["file"], x["line"], x["rule"])
        if key not in seen:
            seen.add(key)
            uniq.append(x)
    return uniq, fctx


def lint(paths, show_all=False):
    files = []
    for p in paths:
        p = Path(p)
        if p.is_dir():
            files += sorted(q for q in p.rglob("*") if q.suffix in (".c", ".h") and "boot" not in q.parts)
        else:
            files.append(p)
    types = defaultdict(set)
    toks_by_file = {}
    for p in files:
        toks = tokenize(strip_source(p.read_text(errors="replace")))
        toks_by_file[p] = toks
        collect_types(toks, types)
    funcs = []
    for p, toks in toks_by_file.items():
        funcs += find_functions(toks, p, types)
    findings, fctx = findings_for(funcs, show_all)
    game_loops = [(str(f.path), line, f.name) for f in funcs for line, _ in f.game_loops]
    return dict(files=len(files), functions=len(funcs), frame_functions=sorted(fctx),
                game_loops=game_loops, findings=findings)


LEVEL_ORDER = {"hot": 0, "frame": 1, "guarded": 2, "init": 3}


def main(argv):
    show_all = "--all" in argv
    argv = [a for a in argv if a != "--all"]
    json_out = None
    if "--json" in argv:
        i = argv.index("--json")
        json_out = argv[i + 1]
        del argv[i:i + 2]
    if not argv:
        print(__doc__)
        return 2
    rep = lint(argv, show_all)
    for x in sorted(rep["findings"], key=lambda d: (LEVEL_ORDER[d["level"]], d["file"], d["line"])):
        where = f" in {x['loops']} loop{'s' if x['loops'] > 1 else ''}" if x["loops"] else ""
        print(f"{x['file']}:{x['line']}: {x['level']}{where} [{x['rule']}] {x['message']}")
    n = defaultdict(int)
    for x in rep["findings"]:
        n[x["level"]] += 1
    loops = ", ".join(f"{Path(p).name}:{l} in {fn}()" for p, l, fn in rep["game_loops"]) or "none found"
    print(f"\n{rep['files']} files, {rep['functions']} functions, {len(rep['frame_functions'])} on the frame path; "
          f"game loops: {loops}")
    print("findings: " + ", ".join(f"{n[k]} {k}" for k in LEVEL_ORDER if n[k]) if rep["findings"] else "findings: none")
    if json_out:
        Path(json_out).write_text(json.dumps(rep, indent=1))
    return 1 if any(x["level"] in ("hot", "frame") for x in rep["findings"]) else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
