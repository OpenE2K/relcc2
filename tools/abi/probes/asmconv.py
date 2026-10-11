#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Ask lcc how it handles inline assembly (spec/abi/source-compat.md,
"Inline assembly"): which constraint letters, operand modifiers and
clobber names it accepts, how it prints operands, what `%#` becomes and
which window a function with an asm gets.  Compiles small functions with
`lcc -S` and prints one line per case.

  python3 tools/abi/probes/asmconv.py [--lcc PATH] [--work DIR]

The default compiler is lcc 1.29.16 for e2k-v3; pass the 1.31.05 wrapper
(toolchains/lcc-1.31.05-e2k-v3/lcc) or another target's lcc to compare.
"""

import argparse
import os
import re
import subprocess

DEFAULT_LCC = "/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc"


def compile_c(lcc, work, src, opt="-O2"):
    """Compile SRC; return (ok, assembly or first diagnostic)."""
    c = os.path.join(work, "t.c")
    s = os.path.join(work, "t.s")
    with open(c, "w") as f:
        f.write(src + "\n")
    if os.path.exists(s):
        os.remove(s)
    # Run in WORK so that diagnostics (which lcc wraps by line length)
    # name the same file whatever WORK is.
    r = subprocess.run([os.path.abspath(lcc) if os.sep in lcc else lcc]
                       + opt.split() + ["-S", "t.c", "-o", "t.s"],
                       capture_output=True, text=True, cwd=work)
    if r.returncode != 0:
        msg = [l for l in (r.stdout + r.stderr).splitlines()
               if re.search("error|ошибка", l, re.I)]
        msg = msg[0] if msg else (r.stdout + r.stderr).strip()[:100]
        # lcc wraps diagnostics at a width that depends on the file name:
        # keep the error number and the first three words.
        m = re.search(r"(error(?: #\d+)?): *(.*)", msg)
        if m:
            msg = m.group(1) + ": " + " ".join(m.group(2).split()[:3])
        return False, msg
    with open(s) as f:
        return True, f.read()


def marked(asm, mark="! OUT"):
    """The text lcc printed for the asm statement marked MARK."""
    for line in asm.splitlines():
        if mark in line:
            return line.split(mark, 1)[1].strip()
    return "(not found)"


def window(asm, fn):
    """setwd / setbn of function FN."""
    body = asm.split(fn + ":", 1)[1]
    body = body.split(".size", 1)[0]
    wd = re.search(r"setwd\s+([^\n]*)", body)
    bn = re.search(r"setbn\s+([^\n]*)", body)
    return (wd.group(1).strip() if wd else "-",
            bn.group(1).strip() if bn else "-")


CONSTRAINTS = [
    # letter, value passed
    ("r", "x"), ("x", "x"), ("m", "g"), ("o", "g"), ("g", "g"),
    ("rm", "g"), ("mr", "g"), ("ri", "5"), ("im", "5"),
    ("i", "5"), ("n", "5"), ("I", "5"), ("J", "5"),
    ("V", "g"), ("<", "g"), (">", "g"), ("s", "&g"), ("X", "g"),
    ("p", "&g"), ("E", "1.5"), ("F", "1.5"), ("f", "x"),
] + [(c, "x") for c in "KLMNOPGHabcdehjklqtuvwyzABCDQRSTUWYZ"]

CONSTANTS = [
    ("i", "5"), ("i", "-1"), ("i", "-1L"), ("i", "(short) -1"),
    ("i", "(signed char) -1"), ("i", "(unsigned) -1"), ("i", "1.5"),
    ("i", "&g"), ("I", "31"), ("I", "32"), ("J", "255"), ("J", "256"),
    ("n", "-16L"),
]

MEMORY = [
    ("p->b", "struct S *p", "m"), ("p->c[50]", "struct S *p", "m"),
    ("gs.b", "void", "m"), ("ga[i]", "long i", "m"),
    ("p[1]", "long *p", "o"), ("p[1]", "long *p", "g"),
]

MODIFIERS = ["c", "n", "H", "a", "l", "P", "p", "w", "d", "x", "X", "h",
             "b", "q", "s", "z", "k", "V", "K"]

CLOBBERS = [
    "memory", "cc", "call", "ctpr0", "ctpr1", "ctpr3", "ctpr4", "%ctpr3",
    "b[0]", "b[7]", "b[8]", "%b[0]", "db[0]", "b0",
    "pred0", "pred31", "pred32", "%pred0", "p0",
    "g0", "g16", "g31", "g32", "%g16", "dg16", "xg16",
    "r0", "r63", "r64", "%r0", "dr0", "xr0",
    "lsr", "ilcr", "pfpfr", "fpcr", "fpsr", "upsr", "usd", "wd", "br",
    "psr", "cr0", "aasr", "aad0", "aasti0", "empty", "MLOCK", "clkr",
    "rndpr", "foo",
]

# %# and the window: (name, source)
WBS = [
    ("leaf, asm volatile with %#", """
long f (long n)
{
  long r;
  __asm__ __volatile__ ("! OUT %#\\n\\taddd 0x0, %1, %0" : "=r" (r) : "r" (n));
  return r;
}"""),
    ("asm volatile (\"\")", """
long f (long n)
{
  __asm__ __volatile__ ("");
  return n + 1;
}"""),
    ("asm with %# and a 12-argument call", """
extern long ext (long, long, long, long, long, long, long, long, long, long,
                 long, long);
long f (long n)
{
  long r;
  __asm__ __volatile__ ("! OUT %#\\n\\taddd 0x0, %1, %0" : "=r" (r) : "r" (n)
                        : "call");
  return r + ext (r, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11);
}"""),
    ("8 arguments live across the asm", """
long f (long a, long b, long c, long d, long e, long g, long h, long i)
{
  long r;
  __asm__ __volatile__ ("! OUT %#\\n\\taddd 0x0, %1, %0" : "=r" (r) : "r" (a)
                        : "call");
  return r * a + b * c + d * e + g * h + i;
}"""),
    ("non-volatile asm, no clobbers", """
long f (long n)
{
  long r;
  __asm__ ("! OUT %#\\n\\taddd 0x0, %1, %0" : "=r" (r) : "r" (n));
  return r;
}"""),
    ("non-volatile asm, \"call\"", """
long f (long n)
{
  long r;
  __asm__ ("! OUT %#\\n\\taddd 0x0, %1, %0" : "=r" (r) : "r" (n) : "call");
  return r;
}"""),
    ("non-volatile asm, \"b[0]\"", """
long f (long n)
{
  long r;
  __asm__ ("! OUT %#\\n\\taddd 0x0, %1, %0" : "=r" (r) : "r" (n) : "b[0]");
  return r;
}"""),
    ("no asm", """
long f (long n)
{
  return n + 1;
}"""),
]


EFFECT_CLOBBERS = ["", "memory", "ctpr3", "call", "b[0]", "g16", "pred0",
                   "r5"]


def effects(lcc, w, volatile, clob):
    """What an asm with clobber CLOB makes lcc do around it (-O2)."""
    vol = "__volatile__ " if volatile else ""
    cl = ' : "%s"' % clob if clob else ""
    # A value loaded before the asm and used after it: reloaded?
    ok, out = compile_c(lcc, w, 'long f (long *p) { long a = *p; long r; '
                        '__asm__ %s("! OUT %%0" : "=r" (r) : : %s); '
                        'return a + r + *p; }'
                        % (vol, ('"%s"' % clob) if clob else ""))
    if not ok:
        return "ERR " + out
    body = out.split("f:", 1)[1]
    pre, post = body.split("! OUT", 1)
    loads = len(re.findall(r"\bldd", post))
    rets = len(re.findall(r"return\s+%ctpr3", post))
    # Operands of a function with values live across the asm.
    ok, out = compile_c(lcc, w, 'long f (long x, long y) { long r; __asm__ '
                        '%s("! OUT %%0 %%1" : "=r" (r) : "r" (x)%s); '
                        'return r + y * x; }' % (vol, cl))
    ops = marked(out) if ok else "ERR " + out
    wd = window(out, "f")[0] if ok else "-"
    return ("loads after it %d, return prepared after it %d; operands %s; "
            "%s" % (loads, rets, ops, wd))


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--lcc", default=DEFAULT_LCC)
    ap.add_argument("--work", default="toolchains/asmconv")
    args = ap.parse_args()
    os.makedirs(args.work, exist_ok=True)
    lcc, w = args.lcc, args.work

    print("Constraints (input operand; how it prints):")
    for c, v in CONSTRAINTS:
        ok, out = compile_c(lcc, w, 'long g; long f (long x) { '
                            '__asm__ __volatile__ ("! OUT %%0" : : "%s" (%s));'
                            ' return x; }' % (c, v))
        print("  %-3s %-5s %s" % (c, "ok" if ok else "ERR",
                                  marked(out) if ok else out))
    ok, out = compile_c(lcc, w, 'long f (long x) { long r; __asm__ '
                        '__volatile__ ("! OUT %0 %1" : "=r" (r) : "0" (x));'
                        ' return r; }')
    print("  0   %-5s %s" % ("ok" if ok else "ERR",
                             marked(out) if ok else out))

    print("Constants:")
    for c, v in CONSTANTS:
        ok, out = compile_c(lcc, w, 'long g; long f (long x) { '
                            '__asm__ __volatile__ ("! OUT %%0" : : "%s" (%s));'
                            ' return x; }' % (c, v))
        print("  %s (%s): %s" % (c, v, marked(out) if ok else "ERR " + out))
    for fpic in ("", "-fPIC"):
        ok, out = compile_c(lcc, w, 'long g; long f (long x) { '
                            '__asm__ __volatile__ ("! OUT %0" : : "i" (&g));'
                            ' return x; }', "-O2 " + fpic if fpic else "-O2")
        print("  i (&g) %s: %s" % (fpic or "non-PIC",
                                   marked(out) if ok else "ERR " + out))

    print("Memory operands:")
    for expr, param, c in MEMORY:
        ok, out = compile_c(lcc, w, 'struct S { long a, b, c[100]; } gs; '
                            'long ga[1000]; long f (%s) { __asm__ __volatile__'
                            ' ("! OUT %%0" : : "%s" (%s)); return 0; }'
                            % (param, c, expr))
        print("  %s (%s): %s" % (c, expr, marked(out) if ok else "ERR " + out))

    print("Operand modifiers (%mN on a register and on i(5)):")
    for m in MODIFIERS:
        ok, out = compile_c(lcc, w, 'long f (long x) { long r; __asm__ '
                            '__volatile__ ("! OUT %%%s1 %%%s2" : "=r" (r) : '
                            '"r" (x), "i" (5)); return r; }' % (m, m))
        ok2, out2 = compile_c(lcc, w, 'long f (long x) { __asm__ '
                              '__volatile__ ("! OUT %%%s0" : : "i" (5)); '
                              'return x; }' % m)
        print("  %%%s: register+constant %s; constant alone %s"
              % (m, marked(out) if ok else "ERR " + out,
                 marked(out2) if ok2 else "ERR " + out2))
    for desc, src in [
            ("%H on __int128 r", '__int128 v; long f (long x) { __asm__ '
             '__volatile__ ("! OUT %0 %H0" : : "r" (v)); return x; }'),
            ("%n on i(-5L)", 'long f (long x) { __asm__ __volatile__ '
             '("! OUT %n0" : : "i" (-5L)); return x; }'),
            ("%c on i(&g)", 'long g; long f (long x) { __asm__ __volatile__ '
             '("! OUT %c0" : : "i" (&g)); return x; }'),
            ("%= %% %[a]", 'long f (long x) { long r; __asm__ __volatile__ '
             '("! OUT %= %% %[a]" : "=r" (r) : [a] "r" (x)); return r; }'),
            ("basic asm %#", 'long f (long x) { __asm__ volatile '
             '("! OUT %#"); return x; }'),
            ("asm goto", 'int f (long x) { __asm__ goto ("! OUT %l1" : : '
             '"r" (x) : : out); return 0; out: return 1; }')]:
        ok, out = compile_c(lcc, w, src)
        print("  %s: %s" % (desc, marked(out) if ok else "ERR " + out))

    print("Clobbers:")
    for c in CLOBBERS:
        ok, out = compile_c(lcc, w, 'long f (long x) { long r; __asm__ '
                            '__volatile__ ("addd 0x0, %%1, %%0" : "=r" (r) : '
                            '"r" (x) : "%s"); return r; }' % c)
        print("  %-8s %s" % (c, "ok" if ok else "ERR " + out))

    print("Effects (-O2):")
    for vol in (True, False):
        for c in EFFECT_CLOBBERS:
            print("  %s, %s: %s" % ("volatile" if vol else "non-volatile",
                                    '"%s"' % c if c else "no clobbers",
                                    effects(lcc, w, vol, c)))

    print("%# and the window (-O0 / -O2):")
    for name, src in WBS:
        row = []
        for opt in ("-O0", "-O2"):
            ok, out = compile_c(lcc, w, src, opt)
            if not ok:
                row.append("ERR " + out)
                continue
            wd, bn = window(out, "f")
            calls = re.findall(r"call\s+%ctpr\d, wbs = (0x[0-9a-f]+)", out)
            row.append("%s; %s; %%# %s%s" % (
                wd, bn, marked(out) if "! OUT" in out else "-",
                "; call wbs " + ",".join(calls) if calls else ""))
        print("  %s:\n    -O0: %s\n    -O2: %s" % (name, row[0], row[1]))


if __name__ == "__main__":
    main()
