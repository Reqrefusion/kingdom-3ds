#!/usr/bin/env python3
"""Compares the SHIPPED SWF's bytecode with the pinned ActionScript source, method by method.

  python tools/swf_vs_source.py <swf> <source dir> [Class ...]

For every method it compares (a) the comparison operators (<, <=, >, >=) and (b) the numeric
constants. The pinned source is a repository snapshot; the SWF is what players ran. A difference
is a CANDIDATE only (compilers fold constants, reorder expressions); read it with
tools/abc_dump.py <swf> <Class> <method>. Found with this: Wall.needsWork (`t < HURT_COOLDOWN`
in the snapshot, `t > HURT_COOLDOWN` in the SWF).
"""
import re
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import abc_dump as A  # noqa: E402

LT, LE, GT, GE = "<", "<=", ">", ">="
CMP = {0xAD: LT, 0x0C: LT, 0x15: LT, 0xAE: LE, 0x0D: LE, 0x16: LE, 0xAF: GT, 0x0E: GT, 0x17: GT, 0xB0: GE, 0x0F: GE, 0x18: GE}


NAME_OPS = ("getproperty", "setproperty", "initproperty", "callproperty", "callpropvoid", "getlex", "findpropstrict", "findproperty",
            "constructprop", "callsuper", "callsupervoid")


def swf_method(a, code):
    ops, nums, names = Counter(), set(), set()
    for at, text in A.disasm(a, code):
        b = code[at]
        if b in CMP:
            ops[CMP[b]] += 1
        parts = text.split()
        if parts[0] in ("pushbyte", "pushshort", "pushint", "pushdouble", "pushuint"):
            try:
                nums.add(float(parts[1]))
            except ValueError:
                pass
        elif parts[0] in NAME_OPS and len(parts) > 1 and not parts[1].startswith("<mn"):
            names.add(parts[1])
    return ops, nums, names


def clean(src):
    src = re.sub(r"/\*.*?\*/", " ", src, flags=re.S)
    src = re.sub(r"//[^\n]*", " ", src)
    src = re.sub(r'"(?:\\.|[^"\\])*"', '""', src)
    return re.sub(r"'(?:\\.|[^'\\])*'", "''", src)


def src_methods(path):
    text = clean(path.read_text(encoding="utf-8", errors="replace"))
    out = {}
    for m in re.finditer(r"function\s+(?:(get|set)\s+)?(\w+)\s*\(", text):
        i = text.find("{", m.end())
        if i < 0:
            continue
        depth, j = 0, i
        while j < len(text):
            depth += text[j] == "{"
            depth -= text[j] == "}"
            if depth == 0:
                break
            j += 1
        out[m.group(2)] = out.get(m.group(2), "") + text[i:j + 1]
    return out


def src_stats(body):
    body = re.sub(r"\.<[^>]*>", "", body)  # Vector.<T>
    ops = Counter()
    for tok in re.findall(r">>>=?|<<=?|>>=?|<=|>=|<|>", body):
        if tok in (LT, LE, GT, GE):
            ops[tok] += 1
    nums = {float(int(n, 16)) if n.lower().startswith("0x") else float(n)
            for n in re.findall(r"(?<![\w.])(?:0x[0-9a-fA-F]+|\d+\.?\d*(?:[eE][+-]?\d+)?)", body)}
    return ops, nums


def main():
    swf, srcdir = sys.argv[1], Path(sys.argv[2])
    only = set(sys.argv[3:])
    abcs = [A.Abc(d) for d in A.load_abc(swf)]
    for a in abcs:
        for (name, sup, iinit, itraits), (cinit, ctraits) in zip(a.instances, a.classes):
            cls = a.mname(name)
            if only and cls not in only:
                continue
            f = srcdir / (cls + ".as")
            if not f.exists():
                continue
            sm = src_methods(f)
            swf_methods = {}
            for tname, t, m in itraits + ctraits:
                if t in (1, 2, 3) and m in a.bodies:
                    swf_methods.setdefault(a.mname(tname), []).append(m)
            swf_methods.setdefault(cls, []).append(iinit)
            for mname, ms in sorted(swf_methods.items()):
                if mname not in sm:
                    continue
                sops, snums, snames = Counter(), set(), set()
                for m in ms:
                    if m in a.bodies:
                        o, n, nm = swf_method(a, a.bodies[m])
                        sops += o
                        snums |= n
                        snames |= nm
                aops, anums = src_stats(sm[mname])
                tokens = set(re.findall(r"\w+", sm[mname]))
                extra = sorted(n for n in snames if n not in tokens and n not in ("Math", "Number", "int", "uint", "Array", "Object", "String"))
                issues = []
                if sops != aops:
                    issues.append("comparisons swf %s vs source %s" % (dict(sops) or "{}", dict(aops) or "{}"))
                only_swf = sorted(x for x in snums - anums if x not in (0.0, 1.0, -1.0))
                only_src = sorted(x for x in anums - snums if x not in (0.0, 1.0, -1.0))
                if only_swf or only_src:
                    issues.append("constants only in swf %s, only in source %s" % (only_swf[:8], only_src[:8]))
                if extra:
                    issues.append("names used by the swf but absent from the source method %s" % extra[:8])
                if issues:
                    print("%s.%s: %s" % (cls, mname, "; ".join(issues)))


if __name__ == "__main__":
    main()
