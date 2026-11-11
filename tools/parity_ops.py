#!/usr/bin/env python3
"""Per-function parity of the C translation against the ActionScript source (and against the SWF).

  python tools/parity_ops.py <source dir> [--swf <swf>] [--all]

Unlike tools/side_by_side.py (statement text), this compares what decides behaviour:
  * comparison operators (<, <=, >, >=) per function: C count vs source count
  * numeric constants: constants in the source that never appear in the C function
    (a dropped threshold/timer) and the reverse
Candidates only: C needs casts/helpers, `for` loops and `switch` map differently. Read each with
side_by_side / tools/abc_dump.py. Exit code 0 even when candidates exist (it is a report).
"""
import re
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import swf_vs_source as S  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
CMPS = ("<", "<=", ">", ">=")


def clean_c(text):
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    text = re.sub(r"//[^\n]*", " ", text)
    text = re.sub(r'"(?:\\.|[^"\\])*"', '""', text)
    text = re.sub(r"'(?:\\.|[^'\\])*'", "''", text)
    text = re.sub(r"^\s*#[^\n]*", " ", text, flags=re.M)
    return text.replace("->", ".")


def c_functions(path, cls):
    text = clean_c(path.read_text(encoding="utf-8", errors="replace"))
    out = {}
    for m in re.finditer(r"^[A-Za-z_][\w\s\*]*?\b%s_(\w+)\s*\([^;{]*\)\s*\{" % re.escape(cls), text, flags=re.M):
        name = m.group(1)
        i = m.end() - 1
        depth, j = 0, i
        while j < len(text):
            depth += text[j] == "{"
            depth -= text[j] == "}"
            if depth == 0:
                break
            j += 1
        name = re.sub(r"^(get|set)_", "", name)
        out[name] = out.get(name, "") + text[i:j + 1]
    return out


def c_stats(body):
    ops = Counter()
    for tok in re.findall(r">>>=?|<<=?|>>=?|<=|>=|<|>", body):
        if tok in CMPS:
            ops[tok] += 1
    nums = set()
    for n in re.findall(r"(?<![\w.])(?:0x[0-9a-fA-F]+|\d+\.?\d*(?:[eE][+-]?\d+)?)[fFuUlL]*", body):
        if not n.lower().startswith("0x"):
            n = re.sub(r"[fFuUlL]+$", "", n)
        else:
            n = re.sub(r"[uUlL]+$", "", n)
        try:
            nums.add(float(int(n, 16)) if n.lower().startswith("0x") else float(n))
        except ValueError:
            pass
    return ops, nums


def main():
    args = sys.argv[1:]
    srcdir = Path(args[0])
    show_all = "--all" in args
    total = ops_diff = const_diff = 0
    lines = []
    for sub in ("game", "flixel"):
        for cf in sorted((ROOT / sub).glob("*.c")):
            cls = cf.stem
            asf = srcdir / (cls + ".as")
            if not asf.exists():
                asf = next(srcdir.rglob(cls + ".as"), None)
            if asf is None:
                continue
            sm = S.src_methods(asf)
            cm = c_functions(cf, cls)
            for name, body in sorted(cm.items()):
                if name not in sm:
                    continue
                total += 1
                aops, anums = S.src_stats(sm[name])
                cops, cnums = c_stats(body)
                issues = []
                if aops != cops:
                    ops_diff += 1
                    issues.append("comparisons source %s vs C %s" % (dict(aops) or "{}", dict(cops) or "{}"))
                miss = sorted(x for x in anums - cnums if x not in (0.0, 1.0, -1.0))
                if miss:
                    const_diff += 1
                    issues.append("source constants missing in C %s" % miss[:8])
                if issues or show_all:
                    lines.append("%s.%s: %s" % (cls, name, "; ".join(issues) or "ok"))
    print("\n".join(lines))
    print("\n%d functions compared; %d with different comparison counts; %d with source constants missing in C" % (total, ops_diff, const_diff))


if __name__ == "__main__":
    main()
