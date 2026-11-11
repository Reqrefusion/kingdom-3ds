#!/usr/bin/env python3
"""Statement-level static comparison of the ActionScript source and the C translation.

For every AS3 function it finds the C function of the same name
(Class_method / Class_get_x / Class_set_x / Class_new|Class_ctor for
constructors), normalises both bodies to a common token form and aligns the
statements. The report lists, per function, how many source statements have
an identical counterpart, and shows every statement that differs side by side.

Usage:
  python tools/side_by_side.py --source <kingdom source root> [--out STATIC_COMPARISON.md]
"""
import argparse
import difflib
import re
from pathlib import Path

HERE = Path(__file__).resolve().parents[1]

AS3_FILES = {
    "Arrow", "Attention", "Bunny", "Campfire", "CameraTarget", "Castle", "Citizen", "Coin", "CoinFloat",
    "Coinsack", "Dust", "Farmland", "Firefly", "FlxBackdrop", "Fog", "GameOverState", "Haze", "Light",
    "MenuState", "Player", "PlayState", "Reed", "Scaffold", "Shop", "Sky", "Sparkle", "Splash", "SunMoon",
    "Torch", "Treeline", "Troll", "Utils", "Wall", "Water", "Weather",
    "org/flixel/FlxBasic", "org/flixel/FlxObject", "org/flixel/FlxSprite", "org/flixel/FlxGroup",
    "org/flixel/FlxParticle", "org/flixel/FlxCamera", "org/flixel/FlxSound", "org/flixel/FlxText",
    "org/flixel/FlxU", "org/flixel/FlxPoint", "org/flixel/FlxRect", "org/flixel/FlxState",
    "org/flixel/FlxTilemap", "org/flixel/system/FlxQuadTree", "org/flixel/system/FlxTilemapBuffer",
    "org/flixel/system/FlxAnim", "org/flixel/FlxG", "com/quasimondo/geom/ColorMatrix",
}

# C functions that are static (no receiver) even though they take arguments.
STATIC_PREFIXES = ("FlxG_", "FlxU_", "Utils_", "Math_", "FlxObject_separate", "FlxPoint_new", "FlxRect_new",
                   "Matrix_new", "Point_new", "Rectangle_new", "ColorTransform_new", "TextFormat_new")


def strip_comments(s: str) -> str:
    out, i, n = [], 0, len(s)
    while i < n:
        c = s[i]
        if c in "\"'":
            j = i + 1
            while j < n and s[j] != c:
                j += 2 if s[j] == "\\" else 1
            out.append(s[i:j + 1])
            i = j + 1
        elif s.startswith("//", i):
            while i < n and s[i] != "\n":
                i += 1
        elif s.startswith("/*", i):
            j = s.find("*/", i + 2)
            i = n if j < 0 else j + 2
        else:
            out.append(c)
            i += 1
    return "".join(out)


def body_at(s: str, open_brace: int) -> tuple[str, int]:
    depth, i = 0, open_brace
    while i < len(s):
        c = s[i]
        if c in "\"'":
            q = c
            i += 1
            while i < len(s) and s[i] != q:
                i += 2 if s[i] == "\\" else 1
        elif c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                return s[open_brace + 1:i], i
        i += 1
    return s[open_brace + 1:], len(s)


def as3_functions(path: Path):
    s = strip_comments(path.read_text(encoding="utf-8", errors="replace"))
    cls = path.stem
    funcs = {}
    for m in re.finditer(r"function\s+(get\s+|set\s+)?(\w+)\s*\(([^)]*)\)[^{;]*\{", s):
        kind = (m.group(1) or "").strip()
        name = m.group(2)
        body, _ = body_at(s, m.end() - 1)
        key = f"{kind}_{name}" if kind else name
        funcs[key] = body
    return cls, funcs


def c_functions(root: Path):
    funcs = {}
    for p in list(root.glob("flixel/*.c")) + list(root.glob("game/*.c")):
        s = strip_comments(p.read_text(encoding="utf-8"))
        for m in re.finditer(r"^(?=\S)(?:static[ \t]+)?(?:const[ \t]+)?[\w \t\*]*?\b(\w+)[ \t]*\(([^;{}]*)\)\s*\{", s, re.M):
            name = m.group(1)
            if name in ("if", "while", "for", "switch"):
                continue
            body, _ = body_at(s, m.end() - 1)
            funcs.setdefault(name, (body, p.relative_to(root)))
    return funcs


def split_args(s: str):
    parts, depth, cur = [], 0, ""
    for c in s:
        if c in "([{":
            depth += 1
        elif c in ")]}":
            depth -= 1
        if c == "," and depth == 0:
            parts.append(cur.strip())
            cur = ""
        else:
            cur += c
    if cur.strip():
        parts.append(cur.strip())
    return parts


ALT_DEFAULTS = {}  # name -> every overload's defaults found in the source (play(), draw(), ...)
DEFAULTS = {  # method/constructor name -> [default or None per parameter]
    # flash.* API (not in the source tree)
    "ColorTransform": ["1", "1", "1", "1", "0", "0", "0", "0"],
    "draw": [None, "null", "null", "null", "null", "false"],
    "copyPixels": [None, None, None, "false"],
    "BitmapData": [None, None, "true", "0xFFFFFFFF"],
}


def collect_defaults(source_root: Path):
    for path in source_root.rglob("*.as"):
        text = strip_comments(path.read_text(encoding="utf-8", errors="replace"))
        for m in re.finditer(r"function\s+(?:get\s+|set\s+)?(\w+)\s*\(([^)]*)\)", text):
            params = []
            for p in split_args(m.group(2)):
                d = p.split("=", 1)[1].strip() if "=" in p else None
                params.append(d)
            if any(d is not None for d in params):
                ALT_DEFAULTS.setdefault(m.group(1), []).append(params)


def _pop_defaults(params, args):
    args = list(args)
    while args and len(args) <= len(params):
        d = params[len(args) - 1]
        if d is None or tokens(common(args[-1].replace("NULL", "null"))) != tokens(common(d)):
            break
        args.pop()
    return args


def drop_defaults(name: str, args: list) -> list:
    params = DEFAULTS.get(name)
    if params:
        return _pop_defaults(params, args)
    # several classes define a method of this name (play(), draw(), ...): of the
    # overloads that can take this many arguments use the one that drops most
    best = list(args)
    for alt in ALT_DEFAULTS.get(name, []):
        if len(alt) >= len(args):
            cand = _pop_defaults(alt, args)
            if len(cand) < len(best):
                best = cand
    return best


def find_call(s: str, start: int):
    """index of the ')' matching the '(' at start"""
    depth = 0
    for i in range(start, len(s)):
        if s[i] == "(":
            depth += 1
        elif s[i] == ")":
            depth -= 1
            if depth == 0:
                return i
    return len(s) - 1


def rewrite_calls(s: str) -> str:
    """Class_method(recv, args) -> recv.method(args); Class_new(a) -> new Class(a); macros."""
    out, i = [], 0
    pat = re.compile(r"\b(SUPER|CALL0|CALL|As3Array_get|As3Array_set|ARR|AS3_INT|AS3_UINT|AS3_SAR|AS3_SHR|as3_mod|as3_streq|as3_is|AS3_CAT|AS3_STR|AS3_BOX|AS3_REGEX|AS3_ARRAY|AS3_LIST|([A-Z]\w*?)_(\w+))\s*\(")
    while True:
        m = pat.search(s, i)
        if not m:
            out.append(s[i:])
            break
        out.append(s[i:m.start()])
        open_i = m.end() - 1
        close_i = find_call(s, open_i)
        args = split_args(rewrite_calls(s[open_i + 1:close_i]))
        word = m.group(1)
        if word == "SUPER":
            inner = args[0] if args else ""
            if re.match(r"^[A-Z]\w*\(", inner):
                inner = re.sub(r"^[A-Z]\w*\(", "super(", inner)
            inner = re.sub(r"^(\w+)\.(\w+)\(", r"super.\2(", inner)
            inner = re.sub(r"^(\w+)\(", r"super.\1(", inner) if not inner.startswith("super") else inner
            rep = inner.replace("super.ctor(", "super(")
        elif word in ("CALL0", "CALL"):
            recv, meth = args[0], args[1]
            rep = (f"{recv}." if recv != "self" else "") + f"{meth}({', '.join(drop_defaults(meth, args[2:]))})"
        elif word == "As3Array_get":
            rep = f"{args[0]}[{args[1]}]"
        elif word == "As3Array_set":
            rep = f"{args[0]}.push({args[2]})" if args[1] == f"{args[0]}.length" else f"{args[0]}[{args[1]}] = {args[2]}"
        elif word == "ARR":
            rep = "[" + ", ".join(args) + "]"
        elif word == "AS3_INT":
            rep = f"int({args[0]})"
        elif word == "AS3_UINT":
            rep = f"uint({args[0]})"
        elif word == "AS3_SAR":
            rep = f"{args[0]} >> {args[1]}"
        elif word == "AS3_SHR":
            rep = f"{args[0]} >>> {args[1]}"
        elif word == "as3_mod":
            rep = f"{args[0]} % {args[1]}"
        elif word == "as3_streq":
            rep = f"{args[0]} == {args[1]}"
        elif word == "as3_is":
            rep = f"{args[0]} is {args[1]}"
        elif word == "AS3_CAT":  # String concatenation (as3.h)
            rep = " + ".join(args)
        elif word in ("AS3_STR", "AS3_BOX"):  # implicit conversions
            rep = args[0]
        elif word == "AS3_REGEX":  # C "D(\\d+)" -> /D(\d+)/
            rep = "/" + args[0][1:-1].replace("\\\\", "\\") + "/"
        elif word in ("AS3_ARRAY", "AS3_LIST"):
            rep = "[" + ", ".join(args) + "]"
        else:
            cls, meth = m.group(2), m.group(3)
            full = f"{cls}_{meth}"
            if meth == "new":
                rep = f"new {cls}({', '.join(drop_defaults(cls, args))})"
            elif meth == "ctor":
                rep = f"{cls}({', '.join(drop_defaults(cls, args[1:]))})"
            elif full.startswith(STATIC_PREFIXES):
                rep = f"{cls}.{meth}({', '.join(drop_defaults(meth, args))})"
            elif meth.startswith("get_") and len(args) == 1:
                rep = (f"{args[0]}." if args[0] != "self" else "") + meth[4:]
            elif meth.startswith("set_") and len(args) == 2:
                rep = (f"{args[0]}." if args[0] != "self" else "") + f"{meth[4:]} = {args[1]}"
            elif args:
                recv = args[0]
                rep = (f"{recv}." if recv != "self" else "") + f"{meth}({', '.join(drop_defaults(meth, args[1:]))})"
            else:
                rep = f"{cls}.{meth}()"
        out.append(rep)
        i = close_i + 1
    return "".join(out)


CASTS = re.compile(r"\((?:const\s+)?(?:struct\s+)?[A-Z]\w*\s*\*+\s*\)|\((?:As3Callback\w*|As3AnimCallback|uint32_t|int32_t|Number|void \*|const uint32_t\[\]|const Number\[20\])\)")


def normalise_c(stmt: str) -> str:
    s = CASTS.sub("", stmt)
    s = s.replace("self->", "").replace("->", ".")
    s = re.sub(r"\bPLAYSTATE\b", "(FlxG.state)", s)
    s = re.sub(r"AS3_FN\(\s*self\s*,\s*(\w+)\)", r"\1", s)
    s = re.sub(r"AS3_FN\(\s*(\w+)\s*,\s*(\w+)\)", r"\1.\2", s)
    s = s.replace("AS3_NULL_FN", "null")
    s = re.sub(r"\(\s*(?:const\s+)?(?:uint32_t|Number)\[\d*\]\)\s*\{([^}]*)\}", r"[\1]", s)
    s = re.sub(r"&(\w+)_class\b", r"\1", s)
    s = re.sub(r"&(\w)", r"\1", s)
    s = rewrite_calls(s)
    s = re.sub(r"^\s*(?:const\s+)?(?:static\s+)?(?:unsigned\s+)?(?:Number|int32_t|uint32_t|bool|double|int|char|[A-Z]\w*)\b(?:\s*\*+\s*|\s+)(\w+)\s*(=|$)", r"\1 \2", s)
    s = re.sub(r"^\s*(?:const\s+)?(?:int32_t|uint32_t|Number|bool|char)\s+\*?(\w+)\[\d*\]\s*=", r"\1 =", s)  # value array
    s = re.sub(r"\bNULL\b", "null", s)
    s = re.sub(r"\b(\w+)_length\b", r"\1.length", s)
    s = re.sub(r"\bAs3Array_splice1\(([^,]+),\s*([^)]+)\)", r"\1.splice(\2, 1)", s)
    s = re.sub(r"\b([A-Z][a-z]\w*?)_([A-Z][A-Z0-9_]+)\b", r"\2", s)  # Class_CONST -> CONST
    s = re.sub(r"\b[A-Z]\w*__(\w+)", r"_\1", s)  # Class__static -> _static
    s = re.sub(r"(\w+(?:\.\w+)*)\.fn\(\1\.self(?:,\s*)?", r"\1(", s)  # closure call
    s = re.sub(r"\b(?:const\s+)?(?:int32_t|uint32_t|Number|bool)\s+(\w+)\s*=", r"\1 =", s)
    s = re.sub(r"\bcall_(\w+)\b", r'"\1"', s)
    s = re.sub(r"\b(?:[A-Z][a-z][A-Za-z]*|king)_(\w+)\b(?!\s*\()", r"\1", s)  # Class_staticMember
    s = re.sub(r"\bnew_(\w+)\(", r"new \1(", s)
    s = re.sub(r"\b(\w+)\.construct\(\)", r"new \1", s)
    s = re.sub(r"\bstrdup\(", "(", s)
    s = re.sub(r"\bchr\b", "char", s)
    s = re.sub(r"\bself\b", "this", s)
    s = re.sub(r"\bNAN\b", "NaN", s)
    return common(s)


def normalise_as3(stmt: str) -> str:
    s = re.sub(r"\bvar\s+(\w+)\s*:\s*[\w.<>*]+", r"\1", stmt)
    s = re.sub(r"\s+as\s+[A-Z]\w*", "", s)
    s = re.sub(r"\b[A-Z]\w*\((\w+)\)", lambda m: m.group(0) if m.group(0).startswith(("int(", "uint(")) else m.group(0), s)
    s = s.replace("this.", "").replace("'", '"')
    s = re.sub(r"\b([A-Z][A-Za-z]+|king)\.([A-Z][A-Z0-9_]+)\b", r"\2", s)  # Class.CONST -> CONST
    s = s.replace(".toString()", "")
    s = re.sub(r"(copyPixels\([^,]+,[^,]+,[^,]+),\s*null\s*,\s*null\s*,", r"\1,", s)
    s = s.replace("===", "==")
    return common(s)


def drop_defaults_in_calls(s: str) -> str:
    out, i = [], 0
    for m in re.finditer(r"\b(\w+)\s*\(", s):
        if m.start() < i:
            continue
        close = find_call(s, m.end() - 1)
        args = split_args(s[m.end():close])
        kept = drop_defaults(m.group(1), args)
        out.append(s[i:m.end()])
        out.append(drop_defaults_in_calls(", ".join(kept)))
        out.append(")")
        i = close + 1
    out.append(s[i:])
    return "".join(out)


def common(s: str) -> str:
    """Rewrites applied to both languages."""
    s = drop_defaults_in_calls(s)
    s = re.sub(r"\bundefined\b", "null", s)
    s = re.sub(r"\bFlxG\.", "", s)
    s = re.sub(r"\bu?int\(", "(", s)  # explicit and implicit coercions compare equal
    s = re.sub(r"\bMath\.floor\(", "floor(", s)
    s = re.sub(r"\bMath\.(sin|cos|pow|sqrt)\(", r"\1(", s)
    return s


def expand_compound(t):
    for k, x in enumerate(t):
        if x in ("+=", "-=", "*=", "/=") and k > 0:
            return t[:k] + ["="] + t[:k] + [x[0]] + t[k + 1:]
    return t


def tokens(stmt: str):
    t = re.findall(r"[A-Za-z_]\w*|\d+\.?\d*(?:[eE][-+]?\d+)?|0x[0-9A-Fa-f]+|==|!=|<=|>=|&&|\|\||>>>|>>|<<|\+=|-=|\*=|/=|\+\+|--|\"[^\"]*\"|[^\s()\[\]{};,]", stmt)
    out = []
    for x in t:
        if x.startswith("0x"):
            x = str(int(x, 16))
        elif re.fullmatch(r"\d+\.?\d*", x):
            x = repr(float(x)).rstrip("0").rstrip(".")
        out.append(x)
    return expand_compound(out)


def statements(body: str):
    """Split a body into statements: ';', '{', '}' and control headers."""
    out, cur, depth, i = [], "", 0, 0
    while i < len(body):
        c = body[i]
        if c == '"' or c == "'":
            j = i + 1
            while j < len(body) and body[j] != c:
                j += 2 if body[j] == "\\" else 1
            cur += body[i:j + 1]
            i = j + 1
            continue
        if c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
            if depth == 0 and re.match(r"\s*(if|while|for|switch|else\s+if)\b", cur):
                out.append(cur + ")")
                cur = ""
                i += 1
                continue
        if depth == 0 and c in ";{}":
            out.append(cur)
            cur = ""
        elif depth == 0 and c == "\n" and cur.strip() and not cur.rstrip().endswith((",", "(", "&&", "||", "+", "-", "*", "/", "?", ":", "=")):
            nxt = body[i + 1:].lstrip()[:1]
            if nxt and nxt not in ".+-*/?:=&|)," and not cur.strip().startswith(("if", "for", "while", "case", "default")):
                out.append(cur)
                cur = ""
        else:
            cur += c
        if depth == 0 and re.fullmatch(r"\s*else", cur) and (i + 1 >= len(body) or not body[i + 1].isalnum()):
            out.append(cur)
            cur = ""
        i += 1
    out.append(cur)
    return [s.strip() for s in out if s.strip() and s.strip() not in ("else",)]


PURE_DECL = re.compile(r"var\s+\w+\s*:\s*[\w.<>*]+(\s*,\s*\w+\s*:\s*[\w.<>*]+)*")
C_BOILERPLATE = re.compile(r"\(void\)\s*\w+|\w+ \*self = as3_alloc\(sizeof \*self\)|self->klass = &\w+|return self|"
                           r"\w+ \*self = object|(?:Number|int32_t|uint32_t|bool|[A-Z]\w*\s*\*)\s*\w+(\s*,\s*\*?\w+)*")


def compare(as3_body: str, c_body: str, cls: str = ""):
    a_stmts = [s for s in statements(as3_body) if tokens(normalise_as3(s)) and not PURE_DECL.fullmatch(s)]
    c_stmts = [s for s in statements(c_body) if tokens(normalise_c(s)) and not C_BOILERPLATE.fullmatch(s)]
    a_tok = [" ".join(tokens(normalise_as3(s))) for s in a_stmts]
    # a static call inside its own class is unqualified in AS3 (Utils.getColor32 inside Utils)
    c_tok = [" ".join(tokens(re.sub(r"\b%s\." % re.escape(cls), "", normalise_c(s)) if cls else normalise_c(s))) for s in c_stmts]
    sm = difflib.SequenceMatcher(None, a_tok, c_tok, autojunk=False)
    same = sum(b.size for b in sm.get_matching_blocks())
    diffs = []
    for op, i1, i2, j1, j2 in sm.get_opcodes():
        if op == "equal":
            continue
        diffs.append((op, a_stmts[i1:i2], c_stmts[j1:j2]))
    return len(a_stmts), same, diffs


def c_name_candidates(cls: str, key: str):
    if key == cls:
        return [f"{cls}_new", f"{cls}_ctor"]
    if key.startswith("get_") or key.startswith("set_"):
        return [f"{cls}_{key}"]
    return [f"{cls}_{key}", key]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--source", required=True, type=Path)
    ap.add_argument("--out", default=HERE / "STATIC_COMPARISON.md", type=Path)
    args = ap.parse_args()
    collect_defaults(args.source)
    cfuncs = c_functions(HERE)
    rows, details = [], []
    total_src = total_same = 0
    missing = []
    for rel in sorted(AS3_FILES):
        path = args.source / f"{rel}.as"
        if not path.exists():
            continue
        cls, funcs = as3_functions(path)
        for key, body in funcs.items():
            cands = c_name_candidates(cls, key)
            label = f"{cls}.{key.replace('_', ' ', 1) if key.startswith(('get_', 'set_')) else key}"
            options = [(c, cfuncs[c][0], cfuncs[c][1]) for c in cands if c in cfuncs]
            if key == cls and f"{cls}_new" in cfuncs and f"{cls}_ctor" in cfuncs:
                # AS3 constructor = field initialisers (_new) + body (_ctor)
                options.append((f"{cls}_new+{cls}_ctor", cfuncs[f"{cls}_new"][0] + "\n" + cfuncs[f"{cls}_ctor"][0],
                                cfuncs[f"{cls}_ctor"][1]))
            if not options:
                missing.append(label)
                continue
            best = None
            for hit, cbody, cfile in options:
                result = compare(body, cbody, cls)
                if best is None or result[1] > best[0][1]:
                    best = (result, hit, cbody, cfile)
            (n, same, diffs), hit, cbody, cfile = best
            total_src += n
            total_same += same
            pct = 100.0 * same / n if n else 100.0
            rows.append((label, hit, str(cfile), n, same, pct))
            if diffs:
                block = [f"### {label} → `{hit}` ({cfile})", "", "| ActionScript | C |", "| --- | --- |"]
                for op, a, c in diffs:
                    for k in range(max(len(a), len(c))):
                        left = a[k] if k < len(a) else ""
                        right = c[k] if k < len(c) else ""
                        block.append(f"| `{left.replace('|', '¦')}` | `{right.replace('|', '¦')}` |")
                details.append("\n".join(block) + "\n")
    lines = [
        "# Static comparison: ActionScript ↔ C",
        "",
        "Generated by `tools/side_by_side.py`. Each source statement is normalised (types, casts, `self->`,",
        "`Class_method(self, …)` → `method(…)` etc.) and aligned with the C statements of the same function.",
        "A statement counts as matched only when the normalised token sequence is identical.",
        "",
        f"**Matched statements: {total_same} / {total_src} ({100.0 * total_same / max(1, total_src):.1f}%)** across {len(rows)} translated functions.",
        "",
        "| Function | C | File | Source statements | Identical | % |",
        "| --- | --- | --- | ---: | ---: | ---: |",
    ]
    for label, hit, f, n, same, pct in sorted(rows, key=lambda r: r[5]):
        lines.append(f"| {label} | `{hit}` | {f} | {n} | {same} | {pct:.0f} |")
    lines += ["", f"## Source functions without a C function ({len(missing)})", "",
              "These are the desktop/debug-only or unused paths listed at the top of each C file.", ""]
    lines += [f"- {m}" for m in sorted(missing)]
    lines += ["", "## Differences", ""] + details
    args.out.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"{total_same}/{total_src} statements identical ({100.0 * total_same / max(1, total_src):.1f}%), "
          f"{len(rows)} functions, {len(missing)} without C counterpart -> {args.out}")


if __name__ == "__main__":
    main()
