#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Generate ABI compatibility tests for the e2k64 calling convention.

Each unit is a header, a caller file and a callee file. The two files are
built by different compilers and linked together; the callee checks every
argument it receives and the caller checks every result. The rules under
test are in spec/abi/calling-convention.md.

Units:
  scalars     every scalar type alone, after 0..8 longs, and returned
  structs     aggregates of 1..80 bytes (and larger) as arguments and results
  floats      aggregates of float, double and long double members
  layout      packed, over-aligned, bit-field and union arguments
  large_ret   results over 64 bytes, with arguments around them
  many        long argument lists that spill past slot 7
  variadic    variadic callees reading fixed and variable arguments
  unproto     calls without a prototype, to fixed and variadic callees
  ext         narrow integer extension by callers and callees
  valist      a va_list started by one compiler and read by the other
  longjmp     longjmp across frames built by the other compiler
  eh          C++ exceptions thrown and caught across compilers (.cc files)
  random      random signatures over random aggregates (seeded)

Usage: gen.py OUTDIR [--seed N] [--random-count N]
"""

import argparse
import random
from pathlib import Path


def hexfloat(x, suffix=""):
    return "(" + float.hex(x) + suffix + ")"


class Type:
    """A C type that can be passed, returned and compared."""
    tag = None          # identifier part, used in eq_<tag>
    name = None         # C spelling (typedef name for aggregates)
    promotable = False  # changed by the default argument promotions
    category = "scalar"

    def value(self, rng):
        raise NotImplementedError

    def eq_body(self):
        return "return a == b;"

    def defs(self):
        return ""


class Int(Type):
    def __init__(self, tag, name, bits, signed, promotable=False):
        self.tag, self.name, self.bits, self.signed = tag, name, bits, signed
        self.promotable = promotable

    def value(self, rng):
        # Prefer values with the sign bit set: they show missing extension.
        if self.name == "_Bool":
            return "1" if rng.random() < 0.7 else "0"
        n = rng.getrandbits(self.bits)
        if rng.random() < 0.7:
            n |= 1 << (self.bits - 1)
        if self.signed and n >> (self.bits - 1):
            n -= 1 << self.bits
        if self.bits == 128:
            u = n & ((1 << 128) - 1)
            return (f"(({self.name})(((unsigned __int128)0x{u >> 64:x}UL << 64)"
                    f" | 0x{u & (1 << 64) - 1:x}UL))")
        if n < 0:
            return f"(({self.name})-0x{-n:x}L)" if n != -(1 << 63) else \
                f"(({self.name})(-0x7fffffffffffffffL - 1))"
        return f"(({self.name})0x{n:x}UL)"

    def eq_body(self):
        return "return a == b;"


class Float(Type):
    def __init__(self, tag, name, mant, suffix, promotable=False):
        self.tag, self.name, self.mant, self.suffix = tag, name, mant, suffix
        self.promotable = promotable

    def value(self, rng):
        m = rng.getrandbits(self.mant - 1) | 1 << (self.mant - 1)
        x = m * 2.0 ** (rng.randint(-20, 20) - self.mant)
        return hexfloat(-x if rng.random() < 0.5 else x, self.suffix)


class Wide(Type):
    """long double and __float128: a double plus a small exact tail."""

    def __init__(self, tag, name, tail_shift):
        self.tag, self.name, self.tail_shift = tag, name, tail_shift

    def value(self, rng):
        e = rng.randint(-20, 20)
        hi = (rng.getrandbits(52) | 1 << 52) * 2.0 ** (e - 52)
        lo = (rng.getrandbits(8) | 1) * 2.0 ** (e - self.tail_shift)
        s = "-" if rng.random() < 0.5 else ""
        return (f"({s}(({self.name}){float.hex(hi)} + "
                f"({self.name}){float.hex(lo)}))")


class Complex(Type):
    def __init__(self, tag, base):
        self.tag, self.base = tag, base
        self.name = f"_Complex {base.name}"

    def value(self, rng):
        return (f"__builtin_complex({self.base.value(rng)}, "
                f"{self.base.value(rng)})")

    def eq_body(self):
        return "return __real__ a == __real__ b && __imag__ a == __imag__ b;"


class Pointer(Type):
    tag, name = "ptr", "void *"

    def value(self, rng):
        return f"((void *)0x{rng.getrandbits(47) & ~7:x}UL)"


class Vector(Type):
    category = "vector"

    def __init__(self, tag, elem, count):
        self.tag, self.elem, self.count = tag, elem, count
        self.name = tag

    def defs(self):
        size = self.count * (4 if self.elem.name in ("int", "float") else 8)
        return (f"typedef {self.elem.name} {self.name} "
                f"__attribute__((vector_size({size})));\n")

    def value(self, rng):
        return ("{" + ", ".join(self.elem.value(rng) for _ in range(self.count))
                + "}")

    def eq_body(self):
        return "return memcmp(&a, &b, sizeof a) == 0;"


I8 = Int("sc", "signed char", 8, True, True)
U8 = Int("uc", "unsigned char", 8, False, True)
CH = Int("ch", "char", 8, True, True)  # char is signed on E2K (data-layout.md)
I16 = Int("ss", "short", 16, True, True)
U16 = Int("us", "unsigned short", 16, False, True)
I32 = Int("si", "int", 32, True)
U32 = Int("ui", "unsigned int", 32, False)
I64 = Int("sl", "long", 64, True)
U64 = Int("ul", "unsigned long", 64, False)
I64LL = Int("sll", "long long", 64, True)
BOOL = Int("b", "_Bool", 1, False, True)
I128 = Int("si128", "__int128", 128, True)
U128 = Int("ui128", "unsigned __int128", 128, False)
FLT = Float("f", "float", 24, "f", True)
DBL = Float("d", "double", 53, "")
LDBL = Wide("ld", "long double", 62)
F128 = Wide("q", "__float128", 100)
CF = Complex("cf", FLT)
CD = Complex("cd", DBL)
CLD = Complex("cld", LDBL)
PTR = Pointer()
V2SI = Vector("v2si", I32, 2)
V4SI = Vector("v4si", I32, 4)
V2DF = Vector("v2df", DBL, 2)
V4DF = Vector("v4df", DBL, 4)

SCALARS = [BOOL, CH, I8, U8, I16, U16, I32, U32, I64, U64, I64LL, I128, U128,
           FLT, DBL, LDBL, F128, CF, CD, CLD, PTR, V2SI, V4SI, V2DF, V4DF]
NARROW = [BOOL, CH, I8, U8, I16, U16, I32, U32]


class Field:
    def __init__(self, type_, count=None, bits=None):
        self.type, self.count, self.bits = type_, count, bits


class Aggregate(Type):
    category = "aggregate"

    def __init__(self, tag, fields, union=False, attrs=""):
        self.tag = self.name = tag
        self.fields, self.union, self.attrs = fields, union, attrs

    def defs(self):
        out = [f"typedef {'union' if self.union else 'struct'} {self.attrs}{{"]
        for i, f in enumerate(self.fields):
            decl = f"\t{f.type.name} f{i}"
            if f.count:
                decl += f"[{f.count}]"
            if f.bits:
                decl += f" : {f.bits}"
            out.append(decl + ";")
        out.append(f"}} {self.name};\n")
        return "\n".join(out)

    def _field_value(self, f, rng):
        if f.bits:
            n = rng.getrandbits(f.bits)
            if f.type.signed and n >> (f.bits - 1):
                n -= 1 << f.bits
            return str(n) + ("L" if f.bits > 31 else "")
        if f.count:
            return "{" + ", ".join(f.type.value(rng) for _ in range(f.count)) + "}"
        return f.type.value(rng)

    def value(self, rng):
        fields = self.fields[:1] if self.union else self.fields
        return "{" + ", ".join(f".f{i} = {self._field_value(f, rng)}"
                               for i, f in enumerate(fields)) + "}"

    def eq_body(self):
        terms = []
        for i, f in enumerate(self.fields[:1] if self.union else self.fields):
            if f.bits:
                terms.append(f"a.f{i} == b.f{i}")
            elif f.count:
                terms.append(f"eq_arr_{f.type.tag}(a.f{i}, b.f{i}, {f.count})")
            else:
                terms.append(f"eq_{f.type.tag}(a.f{i}, b.f{i})")
        return "return " + (" && ".join(terms) or "1") + ";"


class Test:
    """One callee and its caller.

    kind: plain, variadic (args[nfixed:] are variadic), unproto (caller has
    no prototype), unproto_va (no prototype, variadic callee), ext_arg,
    ext_ret, reext (see Unit.ext_callee), valist, valist_ptr (see
    Unit.valist_callee), longjmp, longjmp_cb (see Unit.jmp_callee).
    depth: recursion depth of the callee before it longjmps.
    """

    def __init__(self, name, ret, args, kind="plain", nfixed=None, depth=0):
        self.name, self.ret, self.args, self.kind = name, ret, args, kind
        self.nfixed = len(args) if nfixed is None else nfixed
        self.depth = depth


class Unit:
    def __init__(self, name, seed):
        self.name = name
        self.rng = random.Random(f"{seed}/{name}")
        self.tests = []
        self.types = []

    def add(self, *args, **kw):
        t = Test(*args, **kw)
        self.tests.append(t)
        for ty in [t.ret] + t.args:
            self.use(ty)

    def use(self, ty):
        if ty is None or ty.tag in (t.tag for t in self.types):
            return
        if isinstance(ty, Aggregate):
            for f in ty.fields:
                self.use(f.type)
        self.types.append(ty)

    # Output

    def header(self):
        rng = self.rng
        out = [f"/* Generated by tools/abi/compat/gen.py: unit {self.name}. */",
               "#include \"compat.h\"", ""]
        for ty in self.types:
            out.append(ty.defs())
        for ty in self.types:
            out.append(f"static inline int eq_{ty.tag}({ty.name} a, {ty.name} b)"
                       f" {{ {ty.eq_body()} }}")
            out.append(f"static inline int eq_arr_{ty.tag}({ty.name} const *a, "
                       f"{ty.name} const *b, int n) {{ for (int i = 0; i < n; "
                       f"i++) if (!eq_{ty.tag}(a[i], b[i])) return 0; return 1; }}")
        out.append("")
        for n, t in enumerate(self.tests):
            for i, ty in enumerate(t.args):
                out.append(f"static {ty.name} const v{n}_{i} UNUSED = "
                           f"{ty.value(rng)};")
            if t.ret:
                out.append(f"static {t.ret.name} const v{n}_r UNUSED = "
                           f"{t.ret.value(rng)};")
        return "\n".join(out) + "\n"

    @staticmethod
    def params(t, names=True, upto=None):
        args = t.args[:upto] if upto is not None else t.args
        ps = [f"{ty.name} a{i}" if names else ty.name for i, ty in enumerate(args)]
        return ps

    def proto(self, n, t, fixed_only=False, names=True):
        ret = t.ret.name if t.ret else "void"
        if t.kind in ("variadic", "unproto_va") or fixed_only:
            ps = self.params(t, names, t.nfixed) + ["..."]
        else:
            ps = self.params(t, names) or ["void"]
        return f"{ret} callee_{n}({', '.join(ps)})"

    def callee(self):
        out = [f"#include \"{self.name}.h\"", ""]
        for n, t in enumerate(self.tests):
            if t.kind.startswith("ext") or t.kind == "reext":
                out.append(self.ext_callee(n, t))
                continue
            if t.kind.startswith("valist"):
                out.append(self.valist_callee(n, t))
                continue
            if t.kind.startswith("longjmp"):
                out.append(self.jmp_callee(n, t))
                continue
            out.append(self.proto(n, t) + "\n{")
            for i in range(t.nfixed):
                out.append(f"\tCHECK({n}, {i}, {t.args[i].tag}, a{i}, v{n}_{i});")
            if t.kind in ("variadic", "unproto_va"):
                out.append("\tva_list ap;")
                out.append(f"\tva_start(ap, a{t.nfixed - 1});")
                for i in range(t.nfixed, len(t.args)):
                    ty = t.args[i]
                    out.append(f"\t{{ {ty.name} x = va_arg(ap, {ty.name}); "
                               f"CHECK({n}, {i}, {ty.tag}, x, v{n}_{i}); }}")
                out.append("\tva_end(ap);")
            if t.ret:
                out.append(f"\treturn v{n}_r;")
            out.append("}\n")
        return "\n".join(out)

    def caller(self):
        out = [f"#include \"{self.name}.h\"", ""]
        for n, t in enumerate(self.tests):
            if t.kind.startswith("ext") or t.kind == "reext":
                out.append(self.ext_caller(n, t))
                continue
            if t.kind.startswith("valist"):
                out.append(self.valist_caller(n, t))
                continue
            if t.kind.startswith("longjmp"):
                out.append(self.jmp_caller(n, t))
                continue
            if t.kind.startswith("unproto"):
                ret = t.ret.name if t.ret else "void"
                out.append(f"{ret} callee_{n}();")
            else:
                out.append(self.proto(n, t, names=False) + ";")
            vals = ", ".join(f"v{n}_{i}" for i in range(len(t.args)))
            ret = t.ret.name if t.ret else "void"
            if t.kind.startswith("unproto"):
                fptype = f"{ret} (*volatile fp)()"
            else:
                fptype = self.proto(n, t, names=False).replace(
                    f"callee_{n}", "(*volatile fp)")
            out.append(f"static void test_{n}(void)\n{{")
            out.append(f"\t{fptype} = callee_{n};")
            if t.ret:
                out.append(f"\t{ret} r = callee_{n}({vals});")
                out.append(f"\tCHECK({n}, -1, {t.ret.tag}, r, v{n}_r);")
                out.append(f"\tr = fp({vals});")
                out.append(f"\tCHECK({n}, -2, {t.ret.tag}, r, v{n}_r);")
            else:
                out.append(f"\tcallee_{n}({vals});")
                out.append(f"\tfp({vals});")
            out.append("}\n")
        out.append("const char *const compat_names[] = {")
        out += [f"\t\"{t.name}\"," for t in self.tests]
        out.append("};")
        out.append("void (*const compat_tests[])(void) = {")
        out += [f"\ttest_{n}," for n in range(len(self.tests))]
        out.append("};")
        out.append(f"const int compat_count = {len(self.tests)};")
        return "\n".join(out) + "\n"

    # Extension tests use different declarations on each side, so that one
    # side sees the whole 64-bit slot:
    #   ext_arg  caller passes a narrow type; callee reads unsigned long and
    #            checks the caller extended it (guide 9.5.1).
    #   ext_ret  callee returns a narrow type; caller reads unsigned long and
    #            checks the callee extended it (guide 9.5.2).
    #   reext    caller passes an unsigned long with wrong upper bits; callee
    #            reads the narrow type and must not depend on them (the
    #            robustness rule; lcc callees re-extend).

    def ext_callee(self, n, t):
        ty = t.args[0] if t.args else t.ret
        if t.kind == "ext_arg":
            return (f"long callee_{n}(long pad, unsigned long x)\n{{\n"
                    f"\tif (x != (unsigned long)(long)v{n}_0) compat_fail({n}, 0);\n"
                    f"\treturn pad;\n}}\n")
        if t.kind == "ext_ret":
            return f"{ty.name} callee_{n}(void)\n{{\n\treturn v{n}_r;\n}}\n"
        return (f"long callee_{n}(long pad, {ty.name} x)\n{{\n"
                f"\tCHECK({n}, 0, {ty.tag}, x, v{n}_0);\n"
                f"\treturn (long)x;\n}}\n")

    def ext_caller(self, n, t):
        ty = t.args[0] if t.args else t.ret
        if t.kind == "ext_arg":
            return (f"long callee_{n}(long, {ty.name});\n"
                    f"static void test_{n}(void)\n{{\n"
                    f"\tif (callee_{n}(7, v{n}_0) != 7) compat_fail({n}, -1);\n}}\n")
        if t.kind == "ext_ret":
            return (f"unsigned long callee_{n}(void);\n"
                    f"static void test_{n}(void)\n{{\n"
                    f"\tif (callee_{n}() != (unsigned long)(long)v{n}_r)"
                    f" compat_fail({n}, -1);\n}}\n")
        # Flip all upper bits relative to the correct extension.
        bits = 8 if ty is BOOL else ty.bits
        return (f"long callee_{n}(long, unsigned long);\n"
                f"static void test_{n}(void)\n{{\n"
                f"\tunsigned long good = (unsigned long)(long)v{n}_0;\n"
                f"\tunsigned long bad = good ^ ~((1UL << {bits}) - 1);\n"
                f"\tif (callee_{n}(7, bad) != (long)v{n}_0) compat_fail({n}, -1);\n}}\n")

    # va_list tests: the caller side has a variadic function fwd_N taking
    # the fixed longs args[:nfixed] and the values args[nfixed:]; it starts
    # a va_list and hands it to callee_N, built by the other compiler.
    #   valist      callee_N(va_list) reads every value.
    #   valist_ptr  callee_N(va_list *) reads the first half of the values,
    #               then fwd_N reads the rest from the same va_list.
    # With the suffix _static, fwd_N is static. lcc then lays out its
    # argument area privately (not 16-byte aligned), and the va_list that
    # escapes shows whether va_arg aligns by address or by slot index.

    def va_split(self, t):
        nva = len(t.args) - t.nfixed
        if t.kind.startswith("valist_ptr"):
            return t.nfixed + (nva + 1) // 2
        return len(t.args)

    def va_reads(self, n, t, lo, hi, ap):
        return [f"\t{{ {t.args[i].name} x = va_arg({ap}, {t.args[i].name}); "
                f"CHECK({n}, {i}, {t.args[i].tag}, x, v{n}_{i}); }}"
                for i in range(lo, hi)]

    def valist_callee(self, n, t):
        ptr = t.kind.startswith("valist_ptr")
        ap = "*ap" if ptr else "ap"
        out = [f"long callee_{n}(va_list {'*' if ptr else ''}ap)\n{{"]
        out += self.va_reads(n, t, t.nfixed, self.va_split(t), ap)
        out.append("\treturn 7;\n}\n")
        return "\n".join(out)

    def valist_caller(self, n, t):
        ptr = t.kind.startswith("valist_ptr")
        static = "static " if t.kind.endswith("_static") else ""
        fixed = [f"long a{i}" for i in range(t.nfixed)]
        out = [f"long callee_{n}(va_list {'*' if ptr else ''});",
               f"{static}long fwd_{n}({', '.join(fixed)}, ...)\n{{",
               "\tva_list ap;",
               f"\tva_start(ap, a{t.nfixed - 1});"]
        out += [f"\tCHECK({n}, {i}, sl, a{i}, v{n}_{i});" for i in range(t.nfixed)]
        out.append(f"\tlong r = callee_{n}({'&' if ptr else ''}ap);")
        if ptr:
            out += self.va_reads(n, t, self.va_split(t), len(t.args), "ap")
        out += ["\tva_end(ap);", "\treturn r;", "}"]
        vals = ", ".join(f"v{n}_{i}" for i in range(len(t.args)))
        out.append(f"static void test_{n}(void)\n{{\n"
                   f"\tif (fwd_{n}({vals}) != 7) compat_fail({n}, -1);\n}}\n")
        return "\n".join(out)

    # setjmp tests: test_N calls setjmp, then callee_N, built by the other
    # compiler, which recurses t.depth times (deep enough to spill register
    # windows to the procedure stack) and checks its arguments.
    #   longjmp     callee_N calls longjmp itself.
    #   longjmp_cb  callee_N calls back jump_N on the caller side, which
    #               calls longjmp across callee_N's frames.
    # Locals of test_N must survive, and later tests must still run.

    def jmp_callee(self, n, t):
        ps = ", ".join(self.params(t))
        names = ", ".join(f"a{i}" for i in range(len(t.args)))
        how = ("jmp_buf *env" if t.kind == "longjmp" else "void (*jump)(int)")
        out = [f"void callee_{n}({how}, int depth{', ' + ps if ps else ''})\n{{",
               "\tvolatile long frame[4] = { depth, depth + 1, depth + 2, depth + 3 };",
               "\tif (depth > 0) {",
               f"\t\tcallee_{n}({'env' if t.kind == 'longjmp' else 'jump'}, "
               f"depth - 1{', ' + names if names else ''});",
               f"\t\tcompat_fail({n}, -3);",
               "\t}"]
        out += [f"\tCHECK({n}, {i}, {ty.tag}, a{i}, v{n}_{i});"
                for i, ty in enumerate(t.args)]
        out.append(f"\tif (frame[3] != depth + 3) compat_fail({n}, -4);")
        if t.kind == "longjmp":
            out.append(f"\tlongjmp(*env, {n + 1});")
        else:
            out.append(f"\tjump({n + 1});")
        out.append("}\n")
        return "\n".join(out)

    def jmp_caller(self, n, t):
        ps = ", ".join(self.params(t, names=False))
        vals = ", ".join(f"v{n}_{i}" for i in range(len(t.args)))
        how = "jmp_buf *" if t.kind == "longjmp" else "void (*)(int)"
        out = [f"void callee_{n}({how}, int{', ' + ps if ps else ''});",
               f"static jmp_buf env_{n};"]
        if t.kind == "longjmp_cb":
            out.append(f"static void jump_{n}(int v)\n{{\n\tlongjmp(env_{n}, v);\n}}")
        arg0 = f"&env_{n}" if t.kind == "longjmp" else f"jump_{n}"
        out.append(
            f"static void test_{n}(void)\n{{\n"
            f"\tvolatile long keep = 0x5a5a5a5a5a5aL;\n"
            f"\tint r = setjmp(env_{n});\n"
            f"\tif (r == 0) {{\n"
            f"\t\tcallee_{n}({arg0}, {t.depth}{', ' + vals if vals else ''});\n"
            f"\t\tcompat_fail({n}, -1);\n"
            f"\t\treturn;\n"
            f"\t}}\n"
            f"\tif (r != {n + 1} || keep != 0x5a5a5a5a5a5aL) compat_fail({n}, -2);\n"
            f"}}\n")
        return "\n".join(out)


# Unit builders

def longs(k):
    return [I64] * k


def u_scalars(u):
    for ty in SCALARS:
        u.add(f"{ty.tag}/ret", ty, [])
        for k in range(9):
            u.add(f"{ty.tag}/after_{k}_long", ty, longs(k) + [ty])
        u.add(f"{ty.tag}/after_7_long_then_long", ty, longs(7) + [ty, I64])
        u.add(f"{ty.tag}/twice_after_1_long", None, [I64, ty, ty])
        u.add(f"{ty.tag}/x10", ty, [ty] * 10)


def arr_struct(elem, n):
    return Aggregate(f"A_{elem.tag}_{n}", [Field(elem, n)])


def u_structs(u):
    sizes = list(range(1, 73)) + [80, 96, 128, 200]
    for size in sizes:
        s = arr_struct(CH, size)
        u.add(f"char{size}/arg_ret", s, [s])
        u.add(f"char{size}/after_1_long", s, [I64, s, I64])
        u.add(f"char{size}/after_7_long", None, longs(7) + [s, I64])
    for elem, counts in ((I16, (1, 2, 3, 4, 5, 8, 9, 32)),
                         (I32, (1, 2, 3, 4, 5, 16, 17)),
                         (I64, (1, 2, 3, 8, 9)),
                         (PTR, (1, 2, 3))):
        for c in counts:
            s = arr_struct(elem, c)
            u.add(f"{s.tag}/arg_ret", s, [s])
            u.add(f"{s.tag}/after_3_long", s, longs(3) + [s, s])
    nested = Aggregate("N_ch_sl", [Field(CH), Field(Aggregate(
        "N_in", [Field(I16), Field(I64)]))])
    u.add("nested/arg_ret", nested, [CH, nested, nested])


def u_floats(u):
    shapes = {
        "ff": [Field(FLT), Field(FLT)],
        "fff": [Field(FLT)] * 3,
        "ffff": [Field(FLT)] * 4,
        "fd": [Field(FLT), Field(DBL)],
        "df": [Field(DBL), Field(FLT)],
        "dd": [Field(DBL), Field(DBL)],
        "ddd": [Field(DBL)] * 3,
        "if": [Field(I32), Field(FLT)],
        "fi": [Field(FLT), Field(I32)],
        "ld": [Field(LDBL)],
        "cld": [Field(CH), Field(LDBL)],
        "ldld": [Field(LDBL), Field(LDBL)],
        "q": [Field(F128)],
        "cd": [Field(CD)],
        "f8": [Field(FLT, 8)],
        "d8": [Field(DBL, 8)],
        "d9": [Field(DBL, 9)],
    }
    for tag, fields in shapes.items():
        s = Aggregate(f"F_{tag}", fields)
        u.add(f"{tag}/arg_ret", s, [s])
        u.add(f"{tag}/mixed", s, [FLT, s, DBL, s, FLT])
        u.add(f"{tag}/after_7_long", s, longs(7) + [s])


def u_layout(u):
    types = [
        Aggregate("P_c_l", [Field(CH), Field(I64)], attrs="__attribute__((packed)) "),
        Aggregate("P_s_i_c", [Field(I16), Field(I32), Field(CH)],
                  attrs="__attribute__((packed)) "),
        Aggregate("AL16_c", [Field(CH)], attrs="__attribute__((aligned(16))) "),
        Aggregate("AL32_c", [Field(CH)], attrs="__attribute__((aligned(32))) "),
        Aggregate("AL32_l3", [Field(I64, 3)], attrs="__attribute__((aligned(32))) "),
        Aggregate("BF_small", [Field(I32, bits=3), Field(U32, bits=5),
                               Field(I32, bits=7)]),
        Aggregate("BF_long", [Field(I64, bits=40), Field(U64, bits=24),
                              Field(I64, bits=33)]),
        Aggregate("BF_mixed", [Field(CH), Field(I32, bits=9), Field(I16)]),
        Aggregate("U_f_i", [Field(FLT), Field(I32)], union=True),
        Aggregate("U_d_c3", [Field(DBL), Field(CH, 3)], union=True),
        Aggregate("U_c20_l", [Field(CH, 20), Field(I64)], union=True),
        Aggregate("U_ld_l", [Field(LDBL), Field(I64)], union=True),
    ]
    for s in types:
        u.add(f"{s.tag}/arg_ret", s, [s])
        for k in (1, 5, 6, 7):
            u.add(f"{s.tag}/after_{k}_long", s, longs(k) + [s, I32])


def u_large_ret(u):
    for size in (64, 65, 72, 100, 128, 256, 1024):
        s = arr_struct(I8, size)
        u.add(f"char{size}/void", s, [])
        u.add(f"char{size}/1_long", s, [I64])
        u.add(f"char{size}/8_long", s, longs(8))
        u.add(f"char{size}/12_long", s, longs(12))
        u.add(f"char{size}/self", s, [s, I64])
    u.add("d9/ret_after_doubles", arr_struct(DBL, 9), [DBL] * 9)


def u_many(u):
    for n in range(8, 25):
        u.add(f"long_x{n}", I64, longs(n))
        u.add(f"double_x{n}", DBL, [DBL] * n)
        u.add(f"int_x{n}", I32, [I32] * n)
        u.add(f"float_x{n}", FLT, [FLT] * n)
    mix = [CH, DBL, I128, FLT, LDBL, I16, CD, PTR, I32, F128, U8, CF]
    for n in range(1, len(mix) + 1):
        u.add(f"mix_{n}", I64, mix[:n] + mix[:n])


VA_TYPES = [I32, U32, I64, U64, I128, DBL, LDBL, F128, CF, CD, CLD, PTR, V4SI]


def u_variadic(u):
    for ty in VA_TYPES:
        for k in (1, 2, 6, 7, 8, 9):
            u.add(f"{ty.tag}/fixed_{k}", I64, longs(k) + [ty, ty], "variadic", k)
    for size in (1, 3, 8, 12, 16, 24, 64, 72):
        s = arr_struct(CH, size)
        for k in (1, 7, 8):
            u.add(f"char{size}/fixed_{k}", s, longs(k) + [s, I32, s], "variadic", k)
    for tag, fields in (("ff", [Field(FLT)] * 2), ("ld", [Field(LDBL)]),
                        ("dd", [Field(DBL)] * 2)):
        s = Aggregate(f"VF_{tag}", fields)
        u.add(f"{tag}/fixed_1", s, [I32, s, DBL, s], "variadic", 1)
    u.add("double_fixed", DBL, [DBL, I32, DBL, LDBL], "variadic", 1)
    u.add("ldbl_fixed", LDBL, [LDBL, LDBL, I32], "variadic", 1)
    u.add("struct_fixed", I64, [arr_struct(CH, 24), I64, I64], "variadic", 1)
    mix = [I32, DBL, I128, LDBL, PTR, CD, I64, F128, U32, CF]
    for n in range(1, len(mix) + 1):
        u.add(f"mix_{n}", I64, [I32] + mix[:n], "variadic", 1)


UNPROTO_TYPES = [I32, U32, I64, U64, I128, DBL, LDBL, F128, CD, PTR]


def u_unproto(u):
    for kind in ("unproto", "unproto_va"):
        for ty in UNPROTO_TYPES:
            for k in (1, 7, 8, 9):
                u.add(f"{kind}/{ty.tag}/{k}", I64, [I64] + [ty] * k, kind, 1)
        for size in (3, 16, 24, 72):
            s = arr_struct(CH, size)
            u.add(f"{kind}/char{size}", s, [I64, s, I64, s], kind, 1)


def u_ext(u):
    for ty in NARROW:
        for k in range(4):
            u.add(f"arg/{ty.tag}/{k}", I64, [ty], "ext_arg")
            u.add(f"ret/{ty.tag}/{k}", ty, [], "ext_ret")
            u.add(f"callee/{ty.tag}/{k}", I64, [ty], "reext")


def u_valist(u):
    for kind in ("valist", "valist_ptr", "valist_static", "valist_ptr_static"):
        for ty in VA_TYPES:
            for k in (1, 2, 7, 8):
                u.add(f"{kind}/{ty.tag}/fixed_{k}", I64, longs(k) + [ty, I32, ty, ty],
                      kind, k)
        for size in (3, 16, 24, 72):
            s = arr_struct(CH, size)
            u.add(f"{kind}/char{size}", I64, [I64, s, I32, s], kind, 1)
        mix = [I32, DBL, I128, LDBL, PTR, CD, I64, F128, U32, CF]
        u.add(f"{kind}/mix", I64, [I64] + mix + mix, kind, 1)


def u_longjmp(u):
    for kind in ("longjmp", "longjmp_cb"):
        for depth in (0, 1, 2, 10, 50, 200):
            u.add(f"{kind}/depth_{depth}", None, [], kind, depth=depth)
            u.add(f"{kind}/args_{depth}", None, [I64, DBL, arr_struct(CH, 24), I128],
                  kind, depth=depth)
        u.add(f"{kind}/many_args", None, longs(12) + [LDBL, CD], kind, depth=3)


def random_aggregate(rng, n, depth=0):
    fields = []
    for _ in range(rng.randint(1, 5)):
        r = rng.random()
        if r < 0.15 and depth < 2:
            fields.append(Field(random_aggregate(rng, f"{n}_{len(fields)}", depth + 1)))
        elif r < 0.3:
            fields.append(Field(rng.choice(SCALARS[:21]), rng.randint(1, 5)))
        else:
            fields.append(Field(rng.choice(SCALARS[:21])))
    union = rng.random() < 0.1
    return Aggregate(f"R{n}", fields, union=union)


def u_random(u, count):
    rng = u.rng
    pool = [random_aggregate(rng, i) for i in range(count // 4 + 1)]
    for n in range(count):
        def pick():
            return rng.choice(pool) if rng.random() < 0.3 else \
                rng.choice(SCALARS)
        args = [pick() for _ in range(rng.randint(0, 14))]
        ret = pick() if rng.random() < 0.85 else None
        u.add(f"r{n}", ret, args)


# C++ exceptions. A thrown type is (tag, setup, throw expression, catch
# clause, check): setup returns the value-dependent parts from the rng;
# the catch clause binds x, and check is true if x is the thrown value.
# The types are declared in EH_TYPES (eh.h); EhKeyed's key function is
# defined in the callee file only, so its vtable and type_info come from
# the callee's compiler.

EH_TYPES = """\
extern int compat_guards;
struct Guard {
	long pad[3];
	Guard() : pad{1, 2, 3} {}
	~Guard() { compat_guards++; }
};

struct EhSmall { int a; char b; };
struct EhBig { long a[16]; };
inline EhBig eh_big(long k) { EhBig b; for (int i = 0; i < 16; i++) b.a[i] = k * 31 + i; return b; }
inline bool eh_big_ok(const EhBig &b, long k) { for (int i = 0; i < 16; i++) if (b.a[i] != k * 31 + i) return false; return true; }
struct alignas(32) EhAligned { long a; };

struct EhBase { long v; EhBase(long v) : v(v) {} virtual ~EhBase() {} virtual long id() const { return 1; } };
struct EhDerived : EhBase { EhDerived(long v) : EhBase(v) {} long id() const override { return 2; } };
struct EhA { long a; virtual ~EhA() {} };
struct EhB { long b; virtual ~EhB() {} };
struct EhMI : EhA, EhB { EhMI(long a, long b) { this->a = a; this->b = b; } };
struct EhKeyed { long v; EhKeyed(long v) : v(v) {} virtual ~EhKeyed(); };
enum class EhEnum : short { a = 1, b = 0x1234 };

struct EhError : std::runtime_error { long v; EhError(long v) : std::runtime_error("eh error"), v(v) {} };
"""


def eh_types(rng):
    a, b = rng.getrandbits(31), rng.getrandbits(63)
    hi, lo = rng.getrandbits(64), rng.getrandbits(64)
    d = rng.uniform(-1e6, 1e6)
    ld = rng.getrandbits(63)
    word = "".join(rng.choice("abcdefgh") for _ in range(12))
    return [
        ("int", f"{a}", "int x", f"x == {a}"),
        ("long", f"{b}L", "long x", f"x == {b}L"),
        ("double", repr(d), "double x", f"x == {d!r}"),
        ("ldouble", f"(long double){ld}L / 1024", "long double x",
         f"x == (long double){ld}L / 1024"),
        ("int128", f"(((__int128){hi}UL << 64) | {lo}UL)", "__int128 x",
         f"x == (((__int128){hi}UL << 64) | {lo}UL)"),
        ("cstr", f"\"{word}\"", "const char *x", f"strcmp(x, \"{word}\") == 0"),
        ("small", f"EhSmall{{{a}, 0x5a}}", "EhSmall x", f"x.a == {a} && x.b == 0x5a"),
        ("big", f"eh_big({a})", "const EhBig &x", f"eh_big_ok(x, {a})"),
        ("aligned", f"EhAligned{{{b}L}}", "EhAligned &x", f"x.a == {b}L"),
        # The runtime aligns exception objects to 16 bytes only (the
        # __aligned__ of _Unwind_Exception), not to alignas(32).
        ("aligned_addr", f"EhAligned{{{b}L}}", "EhAligned &x",
         f"x.a == {b}L && (unsigned long)&x % 16 == 0"),
        ("derived", f"EhDerived({b}L)", "const EhBase &x", f"x.id() == 2 && x.v == {b}L"),
        ("mi_second", f"EhMI({a}, {b}L)", "EhB &x",
         f"x.b == {b}L && dynamic_cast<EhMI *>(&x) && dynamic_cast<EhMI *>(&x)->a == {a}"),
        ("keyed", f"EhKeyed({b}L)", "EhKeyed &x", f"x.v == {b}L"),
        ("pointer", f"new EhDerived({b}L)", "EhBase *x",
         f"x->id() == 2 && x->v == {b}L && (delete x, true)"),
        ("enum", "EhEnum::b", "EhEnum x", "x == EhEnum::b"),
        ("std_error", f"EhError({b}L)", "const std::exception &x",
         f"strcmp(x.what(), \"eh error\") == 0 && "
         f"dynamic_cast<const EhError *>(&x)->v == {b}L"),
        ("std_string", f"std::string(\"{word}\")", "const std::string &x",
         f"x == \"{word}\""),
    ]


def eh_unit(seed):
    """Return (header, caller, callee, test names) for the eh unit.

    Kinds, each over every thrown type:
      throw    callee_N recurses DEPTH times, with a Guard in every frame,
               then throws; test_N catches.
      cb       callee_N recurses, then calls cb_N on the caller side, which
               throws through callee_N's frames.
      rethrow  callee_N catches what cb_N throws and rethrows it with
               "throw;"; test_N catches.
      catch    callee_N catches what cb_N throws, checks it and returns.
    Every test checks that each Guard was destroyed once.
    """
    rng = random.Random(f"{seed}/eh")
    types = eh_types(rng)
    head = ["/* Generated by tools/abi/compat/gen.py: unit eh. */",
            "#include \"compat.h\"", "#include <stdexcept>", "#include <string>",
            "", EH_TYPES]
    caller = ["#include \"eh.h\"", "", "int compat_guards;", ""]
    callee = ["#include \"eh.h\"", "", "EhKeyed::~EhKeyed() {}", ""]
    names = []
    for kind, depths in (("throw", (0, 1, 30)), ("cb", (0, 1, 30)),
                         ("rethrow", (0, 5)), ("catch", (0, 5))):
        for tag, value, clause, check in types:
            for depth in depths:
                n = len(names)
                names.append(f"{kind}/{tag}/depth_{depth}")
                guards = depth + 1
                if kind == "throw":
                    callee += [
                        f"void callee_{n}(int depth)\n{{",
                        "\tGuard g;",
                        f"\tif (depth > 0) {{ callee_{n}(depth - 1); compat_fail({n}, -3); }}",
                        f"\tthrow {value};",
                        "}\n"]
                    caller.append(f"void callee_{n}(int);")
                    call = f"callee_{n}({depth});"
                else:
                    caller += [f"void callee_{n}(void (*)(), int);",
                               f"static void cb_{n}() {{ throw {value}; }}"]
                    call = f"callee_{n}(cb_{n}, {depth});"
                    body = {"cb": ["\tcb();"],
                            "rethrow": ["\ttry { cb(); } catch (...) { throw; }"],
                            "catch": ["\ttry {", "\t\tcb();",
                                      f"\t}} catch ({clause}) {{",
                                      f"\t\tif (!({check})) compat_fail({n}, 1);",
                                      "\t\treturn;",
                                      f"\t}} catch (...) {{ compat_fail({n}, 2); return; }}"]}[kind]
                    callee += [
                        f"void callee_{n}(void (*cb)(), int depth)\n{{",
                        "\tGuard g;",
                        f"\tif (depth > 0) {{ callee_{n}(cb, depth - 1); return; }}",
                        *body,
                        f"\tcompat_fail({n}, -3);" if kind != "catch" else "",
                        "}\n"]
                if kind == "catch":
                    caller.append(
                        f"static void test_{n}()\n{{\n"
                        f"\tcompat_guards = 0;\n"
                        f"\ttry {{ {call} }} catch (...) {{ compat_fail({n}, -2); }}\n"
                        f"\tif (compat_guards != {guards}) compat_fail({n}, -4);\n}}\n")
                else:
                    caller.append(
                        f"static void test_{n}()\n{{\n"
                        f"\tcompat_guards = 0;\n"
                        f"\ttry {{\n\t\t{call}\n\t\tcompat_fail({n}, -1);\n"
                        f"\t}} catch ({clause}) {{\n"
                        f"\t\tif (!({check})) compat_fail({n}, -1);\n"
                        f"\t}} catch (...) {{\n\t\tcompat_fail({n}, -2);\n\t}}\n"
                        f"\tif (compat_guards != {guards}) compat_fail({n}, -4);\n}}\n")
    caller.append('extern "C" const char *const compat_names[] = {')
    caller += [f'\t"{x}",' for x in names]
    caller += ["};", 'extern "C" void (*const compat_tests[])(void) = {']
    caller += [f"\ttest_{n}," for n in range(len(names))]
    caller += ["};", f'extern "C" const int compat_count = {len(names)};']
    return ("\n".join(head) + "\n", "\n".join(caller) + "\n",
            "\n".join(callee) + "\n", names)


COMPAT_H = """\
/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Generated by tools/abi/compat/gen.py. Shared by every unit. */
#ifndef COMPAT_H
#define COMPAT_H
#include <setjmp.h>
#include <stdarg.h>
#include <string.h>

#define UNUSED __attribute__((unused))

#ifdef __cplusplus
extern "C" {
#endif
/* arg >= 0: argument index, seen by the callee; -1: result of a direct
   call, -2: result of an indirect call, seen by the caller. */
void compat_fail(int test, int arg);
#ifdef __cplusplus
}
#endif

#define CHECK(t, i, tag, x, v) \\
	do { if (!eq_##tag(x, v)) compat_fail(t, i); } while (0)
#endif
"""

MAIN_C = """\
/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Generated by tools/abi/compat/gen.py. Linked into every test program,
   built by the reference compiler.

   Usage: prog [INDEX]. Prints "FAIL <test> <arg>" lines and, if it
   finishes, "DONE <count>". */
#include <stdio.h>
#include <stdlib.h>

extern const char *const compat_names[];
extern void (*const compat_tests[])(void);
extern const int compat_count;

void compat_fail(int test, int arg)
{
	printf("FAIL %s %d\\n", compat_names[test], arg);
	fflush(stdout);
}

int main(int argc, char **argv)
{
	int first = 0, last = compat_count;
	if (argc > 1)
		last = (first = atoi(argv[1])) + 1;
	for (int i = first; i < last; i++)
		compat_tests[i]();
	printf("DONE %d\\n", last - first);
	return 0;
}
"""

UNITS = {
    "scalars": u_scalars,
    "structs": u_structs,
    "floats": u_floats,
    "layout": u_layout,
    "large_ret": u_large_ret,
    "many": u_many,
    "variadic": u_variadic,
    "unproto": u_unproto,
    "ext": u_ext,
    "valist": u_valist,
    "longjmp": u_longjmp,
}


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("outdir", type=Path)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--random-count", type=int, default=300)
    ap.add_argument("--units", help="comma-separated subset")
    args = ap.parse_args()

    names = list(UNITS) + ["random", "eh"]
    if args.units:
        names = args.units.split(",")
    args.outdir.mkdir(parents=True, exist_ok=True)
    (args.outdir / "compat.h").write_text(COMPAT_H)
    (args.outdir / "main.c").write_text(MAIN_C)
    total = 0
    for name in names:
        if name == "eh":
            h, caller, callee, tests = eh_unit(args.seed)
            (args.outdir / "eh.h").write_text(h)
            (args.outdir / "eh_caller.cc").write_text(caller)
            (args.outdir / "eh_callee.cc").write_text(callee)
            (args.outdir / "eh.tests").write_text("".join(t + "\n" for t in tests))
            total += len(tests)
            print(f"eh: {len(tests)} tests")
            continue
        u = Unit(name, args.seed)
        if name == "random":
            u_random(u, args.random_count)
        else:
            UNITS[name](u)
        (args.outdir / f"{name}.h").write_text(u.header())
        (args.outdir / f"{name}_caller.c").write_text(u.caller())
        (args.outdir / f"{name}_callee.c").write_text(u.callee())
        (args.outdir / f"{name}.tests").write_text(
            "".join(t.name + "\n" for t in u.tests))
        total += len(u.tests)
        print(f"{name}: {len(u.tests)} tests")
    (args.outdir / "units").write_text("\n".join(names) + "\n")
    print(f"{total} tests -> {args.outdir}")


if __name__ == "__main__":
    main()
