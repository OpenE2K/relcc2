#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""lcc's loop pragmas in runs, and why a GCC port must not let the ones
it ignores reach its parser (spec/abi/source-compat.md, "Pragmas").

1. Counts, in OpenE2K/glibc and OpenE2K/linux, the runs of consecutive
   `#pragma` lines (blank and comment lines between allowed) in which a
   pragma GCC has no counterpart for (`loop count (N)`, `noprefetch`,
   `no_dam`, ...) follows one a port maps to GCC's own (`unroll (N)`,
   `ivdep`, `novector`), and lists them; also every bare `#pragma unroll`
   (lcc: unroll fully; GCC's unroll pragma needs a count).
2. Compiles the order with a GCC (default: the host's gcc and g++):
   `#pragma GCC unroll 2` followed by a deferred pragma (`#pragma message`,
   one the C and C++ parsers see as a token) and by a preprocessor pragma
   (`#pragma GCC warning`, which libcpp handles and leaves no token for),
   then the loop.
3. Compiles one such run, and a bare `unroll`, with lcc at -O3 -Wall
   -Werror.

  python3 tools/abi/probes/foldp101_pragma_runs.py [--src OpenE2K]
      [--gcc gcc] [--gxx g++] [--lcc PATH] [--work DIR]

Results (2026-10-24; OpenE2K/glibc 2.35-29.023 and linux 51c316e1d536;
host GCC 16.2.1, x86-64; lcc 1.29.16 e2k-v3 and 1.31.05 e2k-v3):
- 62 runs: 52 in glibc's sysdeps/e2k (bzero.c 3, memcpy.c 12, memmove.c
  31, memset.c 3, strcspn.c, strpbrk.c, strspn.c 1 each; `unroll (N)`
  then `loop count (N)`, some then `noprefetch`) and 10 in the kernel's
  arch/e2k (`ivdep` then `loop count`: crypto/chacha-e2kv3.c 3,
  chacha-e2kv6.c 2; `unroll(1)` then `loop count`: crypto/crc32c-e2kv6.c
  1; `no_dam`, `ivdep`, `loop count`, `unroll(1)`, `novector`:
  asm/xor.h 4).  None is under an #if that is false for GCC.  Bare
  `#pragma unroll` appears only in tools/testing/selftests/bpf, BPF
  programs clang builds, not in E2K code.
- GCC: the deferred pragma after `GCC unroll` is "for, while or do
  statement expected" (C) and "'#pragma' is not allowed here" (C++); the
  preprocessor pragma there compiles (its warning printed), and a
  deferred pragma before `GCC unroll` compiles.
- lcc accepts the run and the bare `unroll` with no diagnostic.
"""

import argparse
import os
import re
import subprocess

MAPPED = re.compile(r"^\s*#\s*pragma\s+(unroll|ivdep|novector)\b(.*)$")
PRAGMA = re.compile(r"^\s*#\s*pragma\s+(.*)$")
LCC = "/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc"


def kind(line):
    m = MAPPED.match(line)
    if m:
        return "mapped", (m.group(1) + m.group(2)).strip()
    m = PRAGMA.match(line)
    if m:
        return "other", m.group(1).strip()
    return None, None


def scan(path):
    """Runs in PATH with an unmapped pragma after a mapped one, and the
    line numbers of bare `unroll`s."""
    with open(path, errors="replace") as f:
        lines = f.read().split("\n")
    runs, bare, i = [], [], 0
    while i < len(lines):
        k, _ = kind(lines[i])
        if k is None:
            i += 1
            continue
        seq, j = [], i
        while j < len(lines):
            k2, t2 = kind(lines[j])
            s = lines[j].strip()
            if k2 is None:
                if s == "" or s.startswith("//"):
                    j += 1
                    continue
                break
            seq.append((j + 1, k2, t2))
            j += 1
        seen = bad = False
        for n, k2, t2 in seq:
            if k2 == "mapped":
                seen = True
                if re.match(r"unroll\s*(/[*/].*)?$", t2):
                    bare.append(n)
            elif seen:
                bad = True
        if bad:
            runs.append(seq)
        i = j
    return runs, bare


def survey(src):
    total, per = 0, {}
    bares = []
    for tree in ("glibc", "linux"):
        for dp, dn, fn in os.walk(os.path.join(src, tree)):
            dn[:] = sorted(d for d in dn if d != ".git")
            for name in sorted(fn):
                if not name.endswith((".c", ".h", ".cc")):
                    continue
                path = os.path.join(dp, name)
                runs, bare = scan(path)
                rel = os.path.relpath(path, src)
                for seq in runs:
                    print("%s:%d: %s" % (rel, seq[0][0], " | ".join(
                        re.sub(r"\s*/\*.*", "", t) for _, _, t in seq)))
                if runs:
                    per[rel] = len(runs)
                    total += len(runs)
                bares += ["%s:%d" % (rel, n) for n in bare]
    for tree in ("glibc", "linux"):
        print("%s: %d runs" % (tree, sum(v for k, v in per.items()
                                         if k.startswith(tree + "/"))))
    print("total: %d runs" % total)
    print("bare #pragma unroll: %s" % (", ".join(bares) or "none"))


GCC_SRC = r"""
void f (int *a, int n)
{
#pragma GCC unroll 2
#ifdef DEFERRED
#pragma message ("a deferred pragma")
#else
#pragma GCC warning "a preprocessor pragma"
#endif
  for (int i = 0; i < n; i++)
    a[i] = 0;
}
void g (int *a, int n)
{
#pragma message ("a deferred pragma before")
#pragma GCC unroll 2
  for (int i = 0; i < n; i++)
    a[i] = 0;
}
"""

LCC_SRC = r"""
void f (long *a, long n)
{
#pragma unroll (2)
#pragma loop count (1000)
#pragma noprefetch
  for (long i = 0; i < n; i++)
    a[i] = 0;
}
void h (long *a)
{
#pragma unroll
  for (long i = 0; i < 8; i++)
    a[i] = i;
}
"""


def compile_(cc, args, src, work, name):
    path = os.path.join(work, name)
    with open(path, "w") as f:
        f.write(src)
    r = subprocess.run(cc + args + ["-c", path, "-o", os.devnull],
                       capture_output=True, text=True)
    diags = [" ".join(x.split()) for x in (r.stdout + r.stderr).splitlines()
             if re.search(r"(error|warning|note):", x)]
    return r.returncode, diags


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--src", default=os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "../../../OpenE2K"))
    ap.add_argument("--gcc", default="gcc")
    ap.add_argument("--gxx", default="g++")
    ap.add_argument("--lcc", action="append")
    ap.add_argument("--work", default="toolchains/foldp101")
    a = ap.parse_args()
    os.makedirs(a.work, exist_ok=True)
    print("== runs with an unmapped pragma after unroll/ivdep/novector")
    survey(a.src)
    print("== GCC: a pragma between GCC unroll and the loop")
    for label, cc, ext in (("C", a.gcc, "c"), ("C++", a.gxx, "cc")):
        for case, d in (("deferred pragma after", ["-DDEFERRED"]),
                        ("preprocessor pragma after", [])):
            rc, diags = compile_([cc], ["-O2", "-Wall"] + d, GCC_SRC,
                                 a.work, "gccorder." + ext)
            errs = [x for x in diags if "error:" in x]
            print("%s, %s: %s" % (label, case,
                                  "ERROR " + errs[0].split("error:", 1)[1]
                                  if rc else "compiles"))
    print("== lcc: a run and a bare unroll, -O3 -Wall -Werror")
    for lcc in a.lcc or [LCC, "toolchains/lcc-1.31.05-e2k-v3/lcc"]:
        rc, diags = compile_([lcc], ["-O3", "-Wall", "-Werror"], LCC_SRC,
                             a.work, "lccrun.c")
        print("%s: %s" % (lcc, "compiles, %d diagnostics" % len(diags)
                          if rc == 0 else "ERROR " + "; ".join(diags[:2])))


if __name__ == "__main__":
    main()
