#!/usr/bin/env python3
"""Minimal AVM2 (ABC) disassembler for the original Kingdom SWF: prints one method of one class.

  python tools/abc_dump.py <swf> Citizen pickNewGoal

Used to compare the SHIPPED bytecode with the source snapshot (game logic that the
snapshot may have lost, e.g. Citizen.pickNewGoal). Only what is needed: constant pool,
instance/class traits, method bodies, opcode operands.
"""
import struct
import sys
import zlib


class R:
    def __init__(self, b, p=0):
        self.b, self.p = b, p

    def u8(self):
        v = self.b[self.p]
        self.p += 1
        return v

    def u30(self):
        v, sh = 0, 0
        while True:
            c = self.u8()
            v |= (c & 0x7F) << sh
            sh += 7
            if not c & 0x80:
                return v

    def s24(self):
        v = self.b[self.p] | self.b[self.p + 1] << 8 | self.b[self.p + 2] << 16
        self.p += 3
        return v - (1 << 24) if v & 0x800000 else v

    def bytes(self, n):
        v = self.b[self.p:self.p + n]
        self.p += n
        return v


def load_abc(swf):
    d = open(swf, "rb").read()
    sig = d[:3]
    body = d[8:]
    if sig == b"CWS":
        body = zlib.decompress(body)
    elif sig != b"FWS":
        raise SystemExit("unsupported SWF " + repr(sig))
    r = R(body)
    nbits = body[0] >> 3
    r.p = (5 + nbits * 4 + 7) // 8 + 4  # RECT, frame rate, frame count
    abcs = []
    while r.p < len(body):
        h = struct.unpack_from("<H", body, r.p)[0]
        r.p += 2
        tag, ln = h >> 6, h & 0x3F
        if ln == 0x3F:
            ln = struct.unpack_from("<I", body, r.p)[0]
            r.p += 4
        if tag == 82:  # DoABC
            q = r.p + 4
            while body[q]:
                q += 1
            abcs.append(body[q + 1:r.p + ln])
        elif tag == 72:
            abcs.append(body[r.p:r.p + ln])
        r.p += ln
    return abcs


class Abc:
    def __init__(self, data):
        r = R(data)
        r.p = 4
        self.ints = [0] + [self.s32(r) for _ in range(max(r.u30() - 1, 0))]
        n = r.u30()
        self.uints = [0] + [r.u30() for _ in range(max(n - 1, 0))]
        n = r.u30()
        self.doubles = [0.0] + [struct.unpack("<d", r.bytes(8))[0] for _ in range(max(n - 1, 0))]
        n = r.u30()
        self.strings = [""] + [r.bytes(r.u30()).decode("utf-8", "replace") for _ in range(max(n - 1, 0))]
        n = r.u30()
        self.ns = [(0, 0)] + [(r.u8(), r.u30()) for _ in range(max(n - 1, 0))]
        n = r.u30()
        self.nssets = [[]] + [[r.u30() for _ in range(r.u30())] for _ in range(max(n - 1, 0))]
        n = r.u30()
        self.mn = [None]
        for _ in range(max(n - 1, 0)):
            k = r.u8()
            if k in (0x07, 0x0D):
                self.mn.append((k, r.u30(), r.u30()))
            elif k in (0x0F, 0x10):
                self.mn.append((k, r.u30()))
            elif k in (0x11, 0x12):
                self.mn.append((k,))
            elif k in (0x09, 0x0E):
                self.mn.append((k, r.u30(), r.u30()))
            elif k in (0x1B, 0x1C):
                self.mn.append((k, r.u30()))
            elif k == 0x1D:
                base = r.u30()
                self.mn.append((k, base, [r.u30() for _ in range(r.u30())]))
            else:
                raise SystemExit("multiname kind %x" % k)
        self.methods = []
        for _ in range(r.u30()):
            pc = r.u30()
            r.u30()
            for _ in range(pc):
                r.u30()
            r.u30()
            fl = r.u8()
            if fl & 0x08:
                for _ in range(r.u30()):
                    r.u30()
                    r.u8()
            if fl & 0x80:
                for _ in range(pc):
                    r.u30()
            self.methods.append(None)
        for _ in range(r.u30()):  # metadata
            r.u30()
            n2 = r.u30()
            for _ in range(2 * n2):
                r.u30()
        cn = r.u30()
        self.instances = []
        for _ in range(cn):
            name, sup, fl = r.u30(), r.u30(), r.u8()
            if fl & 0x08:
                r.u30()
            for _ in range(r.u30()):
                r.u30()
            iinit = r.u30()
            self.instances.append((name, sup, iinit, self.traits(r)))
        self.classes = []
        for _ in range(cn):
            self.classes.append((r.u30(), self.traits(r)))
        for _ in range(r.u30()):  # scripts
            r.u30()
            self.traits(r)
        self.bodies = {}
        for _ in range(r.u30()):
            m = r.u30()
            r.u30(), r.u30(), r.u30(), r.u30()
            code = r.bytes(r.u30())
            for _ in range(r.u30()):
                for _ in range(5):
                    r.u30()
            self.traits(r)
            self.bodies[m] = code

    @staticmethod
    def s32(r):
        return R.u30(r)

    def traits(self, r):
        out = []
        for _ in range(r.u30()):
            name, kind = r.u30(), r.u8()
            t = kind & 0x0F
            if t in (0, 6):
                r.u30(), r.u30()
                vindex = r.u30()
                vkind = r.u8() if vindex else None
                out.append((name, t, (vkind, vindex)))
            elif t == 4 or t == 5:
                r.u30(), r.u30()
                out.append((name, t, None))
            else:
                r.u30()
                out.append((name, t, r.u30()))
            if kind & 0x40:
                for _ in range(r.u30()):
                    r.u30()
        return out

    def mname(self, i):
        if i == 0 or i >= len(self.mn):
            return "*"
        m = self.mn[i]
        if m[0] in (0x07, 0x0D, 0x09, 0x0E):
            return self.strings[m[2]] if m[0] in (0x07, 0x0D) else self.strings[m[1]]
        return "<mn%x>" % m[0]


U30_OPS = {0x2C: "pushstring", 0x2D: "pushint", 0x2E: "pushuint", 0x2F: "pushdouble", 0x31: "pushnamespace", 0x40: "newfunction",
           0x41: "call", 0x42: "construct", 0x49: "constructsuper", 0x55: "newobject", 0x56: "newarray", 0x58: "newclass",
           0x59: "getdescendants", 0x5A: "newcatch", 0x5D: "findpropstrict", 0x5E: "findproperty", 0x5F: "finddef", 0x60: "getlex",
           0x61: "setproperty", 0x62: "getlocal", 0x63: "setlocal", 0x66: "getproperty", 0x68: "initproperty", 0x6A: "deleteproperty",
           0x6C: "getslot", 0x6D: "setslot", 0x80: "coerce", 0x86: "astype", 0xB2: "istype", 0xF0: "debugline", 0xF1: "debugfile",
           0x08: "kill", 0x25: "pushshort", 0x65: "getscopeobject", 0xC2: "inclocal", 0xC3: "declocal", 0x14: "x"}
U30U30 = {0x43: "callmethod", 0x44: "callstatic", 0x45: "callsuper", 0x46: "callproperty", 0x4A: "constructprop", 0x4C: "callproplex",
          0x4E: "callsupervoid", 0x4F: "callpropvoid", 0x32: "hasnext2"}
BRANCH = {0x0C: "ifnlt", 0x0D: "ifnle", 0x0E: "ifngt", 0x0F: "ifnge", 0x10: "jump", 0x11: "iftrue", 0x12: "iffalse", 0x13: "ifeq", 0x14: "ifne",
          0x15: "iflt", 0x16: "ifle", 0x17: "ifgt", 0x18: "ifge", 0x19: "ifstricteq", 0x1A: "ifstrictne"}
SIMPLE = {0x20: "pushnull", 0x21: "pushundefined", 0x26: "pushtrue", 0x27: "pushfalse", 0x28: "pushnan", 0x29: "pop", 0x2A: "dup", 0x2B: "swap",
          0x30: "pushscope", 0x47: "returnvoid", 0x48: "returnvalue", 0x64: "getglobalscope", 0x70: "convert_s", 0x73: "convert_i", 0x74: "convert_u",
          0x75: "convert_d", 0x76: "convert_b", 0x82: "coerce_a", 0x85: "coerce_s", 0x90: "negate", 0x91: "increment", 0x93: "decrement",
          0x95: "typeof", 0x96: "not", 0xA0: "add", 0xA1: "subtract", 0xA2: "multiply", 0xA3: "divide", 0xA4: "modulo", 0xAB: "equals",
          0xAD: "lessthan", 0xAE: "lessequals", 0xAF: "greaterthan", 0xB0: "greaterequals", 0xAC: "strictequals", 0xA8: "bitand", 0xA9: "bitor",
          0xC0: "increment_i", 0xC1: "decrement_i", 0xD0: "getlocal0", 0xD1: "getlocal1", 0xD2: "getlocal2", 0xD3: "getlocal3",
          0xD4: "setlocal0", 0xD5: "setlocal1", 0xD6: "setlocal2", 0xD7: "setlocal3", 0x57: "newactivation", 0x1D: "popscope", 0x09: "label",
          0x35: "li8", 0x1C: "pushwith", 0x1E: "nextname", 0x23: "nextvalue", 0x24: "pushbyte", 0xB3: "istypelate", 0xB4: "in", 0xB1: "instanceof"}


def disasm(a, code):
    r = R(code)
    while r.p < len(code):
        at = r.p
        op = r.u8()
        if op == 0x24:
            v = r.u8()
            yield at, "pushbyte %d" % (v - 256 if v > 127 else v)
        elif op == 0xEF:
            r.u8(), r.u30(), r.u8(), r.u30()
            yield at, "debug"
        elif op in U30U30:
            m, n = r.u30(), r.u30()
            yield at, "%s %s argc=%d" % (U30U30[op], a.mname(m), n)
        elif op == 0x65:
            yield at, "getscopeobject %d" % r.u8()
        elif op in U30_OPS:
            v = r.u30()
            if op in (0x5D, 0x5E, 0x5F, 0x60, 0x61, 0x66, 0x68, 0x6A, 0x80, 0x86, 0xB2, 0x59, 0x42 - 0x42 + 0x59):
                yield at, "%s %s" % (U30_OPS[op], a.mname(v))
            elif op == 0x2C:
                yield at, "pushstring %r" % a.strings[v]
            elif op == 0x2F:
                yield at, "pushdouble %r" % a.doubles[v]
            elif op == 0x2D:
                yield at, "pushint %d" % a.ints[v]
            elif op in (0xF0, 0xF1):
                yield at, "%s %s" % (U30_OPS[op], v if op == 0xF0 else a.strings[v])
            else:
                yield at, "%s %d" % (U30_OPS[op], v)
        elif op in BRANCH:
            off = r.s24()
            yield at, "%s -> %d" % (BRANCH[op], r.p + off)
        elif op == 0x1B:
            d = r.s24()
            n = r.u30()
            cases = [r.s24() for _ in range(n + 1)]
            yield at, "lookupswitch default %d cases %s" % (at + d, cases)
        elif op in SIMPLE:
            yield at, SIMPLE[op]
        else:
            yield at, "op_%02x" % op


def main():
    swf, cls, meth = sys.argv[1:4]
    if meth == "--consts":  # print the constant (static and instance) values of a class
        for data in load_abc(swf):
            a = Abc(data)
            for (name, sup, iinit, itraits), (cinit, ctraits) in zip(a.instances, a.classes):
                if a.mname(name) != cls:
                    continue
                for tname, t, v in itraits + ctraits:
                    if t == 6 and v and v[0] is not None:
                        k, i = v
                        val = {0x03: a.ints, 0x04: a.uints, 0x06: a.doubles, 0x01: a.strings}.get(k, [None] * (i + 1))[i]
                        if k == 0x0A: val = False
                        if k == 0x0B: val = True
                        print("%s = %r" % (a.mname(tname), val))
        return
    for data in load_abc(swf):
        a = Abc(data)
        for (name, sup, iinit, itraits), (cinit, ctraits) in zip(a.instances, a.classes):
            if a.mname(name) != cls:
                continue
            for tname, t, m in itraits + ctraits:  # instance and static methods
                if t in (1, 2, 3) and a.mname(tname) == meth:
                    print("%s.%s method #%d" % (cls, meth, m))
                    for at, text in disasm(a, a.bodies[m]):
                        if not text.startswith("debug"):
                            print("%5d  %s" % (at, text))
                    return
    print("not found")


if __name__ == "__main__":
    main()
