#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""What MCST's `__no_semispec` makes lcc emit (spec/abi/kernel-build.md,
"Options the kernel passes" and "Speculative loads").  The kernel puts it
on every `.uaccess_functions` function (asm/uaccess.h:397-411): a
`noinline` wrapper whose work is done by an `always_inline` body without
the attribute.  Such a function must read only the memory it was asked
to, so it must not use semi-speculative loads.  The kernel spells it
(asm/compiler.h:212-222) `optimize("-fno-semi-spec-ld")` plus
`optimize("-fno-loop-apb")` for lcc 1.31.05 and later, `optimize("O1")`
for older lcc, `optnone` for clang, and not at all for GCC.

Compiles four loop and load shapes (a conditional load through
pointers, a loop with conditional loads, pointer chasing, a counted sum),
each once as a function carrying the attribute (d_*) and once as the
kernel's shape, an attributed noinline wrapper around an always_inline
body (w_*), with each spelling and without, under the kernel's lcc
options (kernelbuild.py's "kernel" set: CONFIG_SEMI_SPECULATIVE_KERNEL=y,
-fsemi-spec-ld; also the =n set and the kernel set without
-fno-loop-apb).  Prints per function "semi/full" counts: semi-speculative
loads (`ld*,sm`, MAS mod 0) and fully speculative ones (`ld*,sm ...
mas=0x3`, with `rbranch ? %dt_alN` recovery), and "apb" when the function
has `ldisp`/`mova`.  Compile only.

  python3 tools/abi/probes/foldp103_nosemispec.py [--lcc PATH ...]
      [--gcc 'CMD' --gcc-attr 'ATTRIBUTE' ...] [--work DIR]

--lcc defaults to lcc 1.29.16 and 1.31.05 (toolchains/lcc-1.31.05-*) for
e2k-v3, e2k-v5.8c2 and e2k-v6.2c3.  --gcc compiles the same source with
another compiler at -O2 with the options kernelbuild.py gives a GCC, once
without an attribute and once with each --gcc-attr (for example a port's
`target ("...")` spelling), and reports the same counts.

Results (2026-10-24, both versions, all three targets, kernel options):
- no attribute: semi-speculative loads in every shape (2-46 a function);
- optimize("O1"): no speculative load of either kind and no APB,
  directly or in the inlined body;
- optimize("-fno-semi-spec-ld") optimize("-fno-loop-apb"): 1.31.05 turns
  every semi-speculative load into a fully speculative one with recovery
  (or none) and drops APB where the options allowed it (v5, v6); 1.29.16
  ignores both, silently (the same code as without), which is why the
  kernel uses "O1" before 1.31.05;
- w_* equal d_* in every case: lcc compiles the inlined always_inline
  body with the wrapper's per-function options.
"""

import argparse
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import kernelbuild as kb  # noqa: E402

LCCS = [
    "/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc",
    "/opt/mcst/lcc-1.29.16.e2k-v5.8c2.linux-6.1/bin/lcc",
    "/opt/mcst/lcc-1.29.16.e2k-v6.2c3.linux-6.1/bin/lcc",
    "toolchains/lcc-1.31.05-e2k-v3/lcc",
    "toolchains/lcc-1.31.05-e2k-v5.8c2/lcc",
    "toolchains/lcc-1.31.05-e2k-v6.2c3/lcc",
]

SRC = r"""
struct node { struct node *next; long v; };
#define BODY_COND  { long r = 0; if (c) r = *p + *q; return r; }
#define BODY_LOOP  { long s = 0; for (long i = 0; i < n; i++) if (b[i]) s += a[i]; return s; }
#define BODY_CHASE { long s = 0; while (p) { s += p->v; p = p->next; } return s; }
#define BODY_SUM   { long s = 0; for (long i = 0; i < n; i++) s += a[i] * b[i]; return s; }
#define NI __attribute__((noinline))
#define AI __attribute__((always_inline)) inline
NI ATTR long d_cond (long *p, long *q, int c) BODY_COND
NI ATTR long d_loop (long *a, long *b, long n) BODY_LOOP
NI ATTR long d_chase (struct node *p) BODY_CHASE
NI ATTR long d_sum (long *a, long *b, long n) BODY_SUM
static AI long b_cond (long *p, long *q, int c) BODY_COND
static AI long b_loop (long *a, long *b, long n) BODY_LOOP
static AI long b_chase (struct node *p) BODY_CHASE
static AI long b_sum (long *a, long *b, long n) BODY_SUM
NI ATTR long w_cond (long *p, long *q, int c) { return b_cond (p, q, c); }
NI ATTR long w_loop (long *a, long *b, long n) { return b_loop (a, b, n); }
NI ATTR long w_chase (struct node *p) { return b_chase (p); }
NI ATTR long w_sum (long *a, long *b, long n) { return b_sum (a, b, n); }
"""

FUNCS = ["d_cond", "d_loop", "d_chase", "d_sum",
         "w_cond", "w_loop", "w_chase", "w_sum"]

LCC_ATTRS = [
    ("none", ""),
    ('optimize("O1")', '__attribute__((optimize("O1")))'),
    ('optimize("-fno-semi-spec-ld") optimize("-fno-loop-apb")',
     '__attribute__((optimize("-fno-semi-spec-ld"))) '
     '__attribute__((optimize("-fno-loop-apb")))'),
]


def counts(asm):
    res = []
    for fn in FUNCS:
        t = kb.body(asm, fn)
        if not t:
            res.append("%s (not found)" % fn)
            continue
        o = kb.ops(t)
        sm = [x for x in o if re.match(r"ld[bhwdq]\S*,sm", x)]
        full = [x for x in sm if "mas=0x3" in x]
        apb = re.search(r"\bldisp\b|\bmova", t)
        res.append("%s %d/%d%s" % (fn, len(sm) - len(full), len(full),
                                   " apb" if apb else ""))
    return "  ".join(res)


def run(cc, sets, attrs):
    print("[%s]" % cc.label)
    for sname, opts in sets.items():
        for aname, attr in attrs:
            src = "#define ATTR %s\n%s" % (attr, SRC)
            ok, asm, diag = cc.compile(src, opts)
            if not ok:
                print("  %s, %s: ERROR %s" % (sname, aname,
                                              kb.short_diag(diag)))
                continue
            warn = kb.short_diag(diag) if diag else ""
            print("  %s, %s: %s%s" % (sname, aname, counts(asm),
                                      ("  [" + warn + "]") if warn else ""))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--lcc", action="append")
    ap.add_argument("--gcc")
    ap.add_argument("--gcc-attr", action="append", default=[])
    ap.add_argument("--work", default="toolchains/foldp103")
    a = ap.parse_args()
    os.makedirs(a.work, exist_ok=True)
    work = os.path.abspath(a.work)
    for path in a.lcc or LCCS:
        argv = [os.path.abspath(path) if os.sep in path else path]
        cc = kb.Compiler(argv, "lcc", work)
        ks = kb.kernel_sets(cc)
        sets = {
            "kernel": ks["kernel"],
            "kernel, SEMI_SPECULATIVE_KERNEL=n":
                ks["kernel, CONFIG_SEMI_SPECULATIVE_KERNEL=n"],
            "kernel without -fno-loop-apb":
                ks["kernel"].replace(" -fno-loop-apb", ""),
        }
        run(cc, sets, LCC_ATTRS)
    if a.gcc:
        cc = kb.Compiler(a.gcc.split(), "gcc", work)
        attrs = [("none", "")] + [(x, "__attribute__((%s))" % x)
                                  for x in a.gcc_attr]
        run(cc, {"-O2": "-O2", "kernel as GCC gets it": kb.KERNEL_GCC},
            attrs)


if __name__ == "__main__":
    main()
