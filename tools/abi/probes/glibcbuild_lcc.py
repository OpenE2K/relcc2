#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Ask lcc what compiling OpenE2K/glibc's E2K code needs from a compiler
(spec/abi/source-compat.md; spec/abi/kernel-interface.md, "Process
entry"): which register-variable names lcc binds and to what, which
pointer-size options it takes, whether it defines __EXCEPTIONS for C and
emits landing pads for __attribute__ ((cleanup)), and what it does with
its own pragmas under -Wall -Werror (and whether `noprefetch` keeps array
prefetch out of a strlen-like loop).  Also counts the lcc pragmas in
OpenE2K/glibc and OpenE2K/linux.  Compiles small files with -S and prints
one line per case; nothing runs.

  python3 tools/abi/probes/glibcbuild_lcc.py [--lcc PATH] [--lcc-apb PATH]
      [--gcc PATH] [--clang PATH] [--src OpenE2K] [--work DIR]

--lcc defaults to lcc 1.29.16 for e2k-v3; --lcc-apb (for the array
prefetch case) to the e2k-v5.8c2 lcc, since lcc's v3 code doesn't
prefetch that loop.  --gcc names another compiler to put beside lcc in
the register-name, option and pragma tables (the proof of concept's
e2k-linux-gnu-gcc, for example), --clang Unipro's clang
(toolchains/unipro/bin/clang).
"""

import argparse
import collections
import os
import re
import subprocess

LCC = "/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc"
LCC_APB = "/opt/mcst/lcc-1.29.16.e2k-v5.8c2.linux-6.1/bin/lcc"


def run(cc, args, work, src, name="t.c"):
    """Compile SRC as NAME in WORK with the command CC (a list); return
    (rc, output text, asm or '')."""
    path = os.path.join(work, name)
    with open(path, "w") as f:
        f.write(src)
    out = os.path.join(work, "t.s")
    if os.path.exists(out):
        os.remove(out)
    r = subprocess.run(cc + args + ["-S", name, "-o", "t.s"], cwd=work,
                       capture_output=True, text=True)
    asm = ""
    if r.returncode == 0 and os.path.exists(out):
        with open(out) as f:
            asm = f.read()
    return r.returncode, r.stdout + r.stderr, asm


def first_diag(text):
    """The first error or warning: its number, first words and option."""
    text = " ".join(text.split())
    m = re.search(r"(error|warning)( #\d+)?: ", text)
    if not m:
        return text[:80]
    rest = text[m.end():]
    opt = re.search(r"\[-W[\w=,-]+\]", rest)
    words = " ".join(rest[:opt.start() if opt else len(rest)].split()[:6])
    return (m.group(1) + (m.group(2) or "") + ": " + words
            + (" " + opt.group(0) if opt else ""))


REG = re.compile(r"%(?:d?r\d+|d?g\d+|b\[\d+\]|db\[\d+\])")


def body(asm, fn):
    """The operations of function FN that compute its result (%r0),
    found by following register operands back from the last write of
    %r0, plus every getsp."""
    if fn + ":" not in asm:
        return "(not found)"
    text = asm.split(fn + ":", 1)[1].split(".size", 1)[0]
    ops = []
    for line in text.splitlines():
        line = " ".join(line.split())
        if (not line or line in ("{", "}") or line.startswith(".")
                or line.endswith(":")):
            continue
        ops.append(line)
    want, keep = {"%r0"}, []
    for op in reversed(ops):
        regs = REG.findall(op)
        dst = regs[-1].replace("%d", "%") if regs else None
        if op.startswith("getsp") or (dst in want and not op.startswith(
                ("call", "disp", "st"))):
            keep.append(op)
            if dst in want:
                want.discard(dst)
                want.update(r.replace("%d", "%") for r in regs[:-1])
    return "; ".join(reversed(keep)) or "(nothing: %r0 is the argument)"


# Register variables.  glibc binds "FP" (start.c, dl-machine.h _start2);
# sysdeps/e2k/memusage.h, which nothing includes since glibc 2.35
# (upstream commit 83b8d5027d, "malloc: Remove memusage.h"), binds "%SP"
# under __LCC__ and "%r9" otherwise.
REG_NAMES = ["FP", "%FP", "SP", "%SP", "fp", "sp", "USD", "%usd", "r9", "%r9",
             "r0", "dr0", "b[0]", "g16", "%g16", "dg16", "pred0", "ctpr1",
             "ip", "lsr", "pfpfr"]
REG_FUNCS = """\
extern void g (volatile char *);
long leaf (void) { register long v asm ("NAME"); return v; }
long frame (void) { volatile char buf[200]; g (buf);
  register long v asm ("NAME"); return v; }
long after_alloca (int n) { char *p = __builtin_alloca (n); g (p);
  register long v asm ("NAME"); return v; }
long minus_frame_address (void) { register long v asm ("NAME");
  return v - (long) __builtin_frame_address (0); }
"""


def regvars(cc, work, label):
    print(f"== register variables, {label} -O2: leaf / 200-byte frame /"
          " after alloca / minus __builtin_frame_address (0)")
    for name in REG_NAMES:
        rc, out, asm = run(cc, ["-O2"], work, REG_FUNCS.replace("NAME", name))
        if rc:
            print(f"{name:7s} rejected: {first_diag(out)}")
            continue
        print(f"{name:7s} " + " | ".join(
            body(asm, fn) for fn in ("leaf", "frame", "after_alloca",
                                     "minus_frame_address")))


def options(cc, work, label):
    print(f"== pointer-size options, {label}: accepted or not, and the mode"
          " macros they define")
    for opt in ["-mptr64", "-m64", "-mptr32", "-m32", "-mptr128", "-m128"]:
        r = subprocess.run(cc + [opt, "-dM", "-E", "-x", "c", os.devnull],
                           capture_output=True, text=True)
        if r.returncode:
            print(f"{opt:9s} rejected: {first_diag(r.stdout + r.stderr)}")
            continue
        macros = [m for m in ("__ptr32__", "__ptr64__", "__ptr128__",
                              "_LP64") if re.search(rf"#define {m}\b",
                                                     r.stdout)]
        print(f"{opt:9s} accepted: {' '.join(macros)}")


def exceptions(cc, work, label):
    print(f"== __EXCEPTIONS and C cleanups, {label}")
    for lang in ("c", "c++"):
        for opt in ([], ["-fexceptions"], ["-fno-exceptions"]):
            r = subprocess.run(cc + opt + ["-dM", "-E", "-x", lang,
                                             os.devnull],
                               capture_output=True, text=True)
            have = re.search(r"#define __EXCEPTIONS\b", r.stdout) is not None
            print(f"{lang:4s} {' '.join(opt) or '(default)':16s}"
                  f" __EXCEPTIONS {'defined' if have else 'not defined'}")
    src = """\
#include <unistd.h>
extern int done;
static void undo (int *p) { done = *p; }
int f (int fd, char *buf)
{
  int x __attribute__ ((cleanup (undo))) = 1;
  return read (fd, buf, 1);
}
"""
    for opt in ("-O0", "-O2"):
        rc, out, asm = run(cc, [opt, "-fexceptions"], work, src)
        eh = [k for k in ("personality", "lsda", "gcc_except_table",
                          "_Unwind_Resume") if k in asm]
        print(f"cleanup attribute around read(), {opt} -fexceptions: "
              f"{'EH data: ' + ', '.join(eh) if eh else 'no personality, no LSDA, no landing pad'}")


PRAGMAS = ["loop count (8)", "unroll (1)", "unroll (4)", "unroll", "noprefetch",
           "prefetch", "ivdep", "novector", "vector aligned", "vector always",
           "no_dam", "swp", "noswp", "comb_oper", "cache_opt",
           "reduce recurrence", "asm_inline", "no_asm_inline",
           "asm_length (0)", "no_such_pragma", "GCC unroll 4", "GCC ivdep",
           "GCC novector"]
LOOP = """\
unsigned long sum (const unsigned char *s, unsigned long n)
{
  unsigned long r = 0, i;
#pragma PRAGMA
  for (i = 0; i < n; i++)
    r += s[i] * 3u + (r >> 7);
  return r;
}
"""
ASM = """\
unsigned get (void)
{
  unsigned cw;
#pragma PRAGMA
  __asm__ __volatile__ ("rrs %%fpcr, %0" : "=r" (cw));
  return cw;
}
"""


def pragmas(compilers, work):
    print("== pragmas before a loop (asm_* before an asm), -O2 -Wall -Werror"
          " (lcc -O3)")
    for p in PRAGMAS:
        src = (ASM if p.startswith(("asm", "no_asm")) else LOOP)
        src = src.replace("PRAGMA", p)
        cols = []
        for label, cc in compilers:
            opt = "-O3" if label == "lcc" else "-O2"
            rc, out, asm = run(cc, [opt, "-Wall", "-Werror"], work, src)
            cols.append(f"{label}: " + ("ok" if rc == 0 else first_diag(out)))
        print(f"#pragma {p:18s} " + " | ".join(cols))
    # The installed headers' _Pragma ("asm_inline") macros, from the
    # sysroot (a system header) and from a copy in a plain -I directory
    # (as glibc's own build includes its sysdeps copy).
    src = ("#include <fpu_control.h>\n"
           "unsigned get (void) { fpu_control_t cw; _FPU_GETCW (cw);"
           " return cw; }\n")
    inc = os.path.join(work, "inc")
    os.makedirs(inc, exist_ok=True)
    hdr = os.path.join(os.path.dirname(compilers[0][1][0]),
                       "../fs/usr/include/fpu_control.h")
    with open(hdr) as f, open(os.path.join(inc, "fpu_control.h"), "w") as g:
        g.write(f.read())
    for where, extra in (("system header", []), ("via -I", ["-Iinc"])):
        cols = []
        for label, cc in compilers:
            rc, out, asm = run(cc, ["-O2", "-Wall", "-Werror"] + extra, work,
                               src)
            cols.append(f"{label}: " + ("ok" if rc == 0 else first_diag(out)))
        print(f"_FPU_GETCW, {where:13s} " + " | ".join(cols))


STRLEN = """\
typedef unsigned long u64;
long scan (const u64 *p, const u64 *start)
{
  u64 a, b, m;
PRAGMAS
  for (;;) {
    a = p[0];
    b = p[1];
    m = __builtin_e2k_pcmpeqb (__builtin_e2k_pminub (a, b), 0);
    p += 2;
    if (m != 0) break;
  }
  return (const char *) p - (const char *) start + __builtin_ctzll (m);
}
"""


def apb(cc, work, label):
    print(f"== array prefetch in glibc's strlen loop shape, {label} -O3:"
          " bap / mova count")
    for prag in ([], ["noprefetch"], ["loop count (8)"],
                 ["noprefetch", "loop count (8)"], ["unroll (1)"]):
        src = STRLEN.replace("PRAGMAS", "\n".join("#pragma " + p
                                                  for p in prag))
        rc, out, asm = run(cc, ["-O3"], work, src)
        if rc:
            print(f"{' + '.join(prag) or '(none)':30s} {first_diag(out)}")
            continue
        print(f"{' + '.join(prag) or '(none)':30s} bap {len(re.findall(r'\bbap\b', asm))}"
              f" mova {len(re.findall(r'\bmova', asm))}")


PRAG_RE = re.compile(
    r'(?:#\s*pragma\s+|_Pragma\s*\(\s*"\s*)'
    r'(loop\s+count|noprefetch|prefetch|unroll|asm_inline|no_asm_inline|'
    r'asm_length|ivdep|novector|no_dam|vector\s+\w+|swp|noswp|comb_oper|'
    r'no_comb_oper|cache_opt|no_cache_opt|nounroll|reduce\s+recurrence|'
    r'spmv|nonesting|apb_cache|apb_aligned|no_rtmd|diag_\w+)\b')
COND_RE = re.compile(r"^\s*#\s*(if|ifdef|ifndef|elif|else|endif)\b(.*)")
# What a GCC build defines, for the conditions that name __LCC__.
GCC_MACROS = {"__LCC__": None, "__GNUC__": "16", "__e2k__": "1"}


def t_not(v):
    return None if v is None else not v


def t_and(a, b):
    if a is False or b is False:
        return False
    return None if a is None or b is None else True


def t_or(a, b):
    if a is True or b is True:
        return True
    return None if a is None or b is None else False


def gcc_value(cond):
    """Whether a GCC build takes `#if COND`: True, False, or None when it
    depends on macros other than __LCC__, __GNUC__ and __e2k__ (every
    value of those tried: None unless all agree)."""
    c = re.sub(r"/\*.*?\*/|//.*", " ", cond)

    def defined(m):
        name = m.group(1) or m.group(2)
        if name in GCC_MACROS:
            return "0" if GCC_MACROS[name] is None else "1"
        return "defined_" + name
    c = re.sub(r"\bdefined\s*(?:\(\s*(\w+)\s*\)|(\w+))", defined, c)
    c = re.sub(r"\b__GNUC_PREREQ\s*\([^)]*\)", "1", c)
    for name, val in GCC_MACROS.items():
        c = re.sub(rf"\b{name}\b", val or "0", c)
    c = re.sub(r"\b(\d+)[uUlL]+\b", r"\1", c)
    c = c.replace("&&", " and ").replace("||", " or ")
    c = re.sub(r"!(?!=)", " not ", c)
    unknown = sorted(set(re.findall(r"\b[A-Za-z_]\w*\b", c))
                     - {"and", "or", "not"})
    if len(unknown) > 6:
        return None
    seen = set()
    for bits in range(1 << len(unknown)):
        env = {n: (bits >> i) & 1 for i, n in enumerate(unknown)}
        try:
            seen.add(bool(eval(c, {"__builtins__": {}}, env)))
        except Exception:
            return None
    return seen.pop() if len(seen) == 1 else None


def scan_file(path):
    """(pragma, lcc only) for each use in PATH: lcc only when an
    enclosing #if, #ifdef or #elif that names __LCC__ (or its #else) is
    false for a GCC build."""
    found, stack = [], []
    with open(path, errors="replace") as f:
        for line in f:
            m = COND_RE.match(line)
            if m:
                kind, rest = m.groups()
                if kind in ("if", "ifdef", "ifndef"):
                    v = gcc_value(rest if kind == "if"
                                  else "defined " + rest.strip())
                    if kind == "ifndef":
                        v = t_not(v)
                    stack.append({"lcc": "__LCC__" in rest, "prev": v,
                                  "cur": v})
                elif kind == "elif" and stack:
                    v, fr = gcc_value(rest), stack[-1]
                    fr["cur"] = t_and(t_not(fr["prev"]), v)
                    fr["prev"] = t_or(fr["prev"], v)
                    fr["lcc"] = fr["lcc"] or "__LCC__" in rest
                elif kind == "else" and stack:
                    stack[-1]["cur"] = t_not(stack[-1]["prev"])
                elif kind == "endif" and stack:
                    stack.pop()
                continue
            for p in PRAG_RE.finditer(line):
                lcc_only = any(fr["lcc"] and fr["cur"] is False
                               for fr in stack)
                found.append((" ".join(p.group(1).split()), lcc_only))
    return found


# Headers that only lcc includes, whatever their own #ifs say.
LCC_ONLY_HEADERS = {
    # math/math.h:1344-1346: only under __USE_EXTERN_INLINES && __LCC__.
    "glibc/sysdeps/e2k/fpu/bits/mathinline.h",
}


def survey(src):
    """Count the lcc pragmas: per pragma, the uses a GCC build compiles
    ("seen by GCC") and those an #if or an include only lcc takes keeps
    from it ("lcc only"), then the total GCC sees."""
    areas = [
        ("glibc E2K sysdeps", ["glibc/sysdeps/e2k",
                               "glibc/sysdeps/unix/sysv/linux/e2k"]),
        ("glibc, other directories", ["glibc"]),
        ("linux arch/e2k", ["linux/arch/e2k"]),
        ("linux include/linux", ["linux/include/linux"]),
    ]
    seen = set()
    for label, dirs in areas:
        uses, files = collections.Counter(), collections.defaultdict(set)
        gcc_uses, gcc_files = 0, set()
        for d in dirs:
            for dp, dn, fn in os.walk(os.path.join(src, d)):
                dn.sort()
                for name in sorted(fn):
                    path = os.path.join(dp, name)
                    if not name.endswith((".c", ".h")) or path in seen:
                        continue
                    seen.add(path)
                    header_lcc = os.path.relpath(path, src) in LCC_ONLY_HEADERS
                    for p, g in scan_file(path):
                        kind = ("lcc only (include)" if header_lcc
                                else "lcc only (#if)" if g else "seen by GCC")
                        uses[(p, kind)] += 1
                        files[(p, kind)].add(path)
                        if kind == "seen by GCC":
                            gcc_uses += 1
                            gcc_files.add(path)
        print(f"== lcc pragmas in {label}")
        for (p, g), n in sorted(uses.items(), key=lambda x: (-x[1], x[0])):
            print(f"{p:18s} {g:18s} {n:4d} uses in {len(files[(p, g)])} files")
        print(f"total seen by GCC: {gcc_uses} uses in {len(gcc_files)} files")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--lcc", default=LCC)
    ap.add_argument("--lcc-apb", default=LCC_APB)
    ap.add_argument("--gcc")
    ap.add_argument("--clang", help="Unipro's clang: run with --target="
                    "e2k-linux-gnu -march=elbrus-v3, lcc's sysroot and lcc's"
                    " bin directory on PATH (its driver calls lcc)")
    ap.add_argument("--src", default=os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "../../../OpenE2K"))
    ap.add_argument("--work", default="toolchains/glibcbuild_lcc")
    a = ap.parse_args()
    os.makedirs(a.work, exist_ok=True)
    lcc = os.path.abspath(a.lcc) if os.sep in a.lcc else a.lcc
    compilers = [("lcc", [lcc])]
    if a.gcc:
        compilers.append(("gcc", [os.path.abspath(a.gcc)
                                  if os.sep in a.gcc else a.gcc]))
    if a.clang:
        lccdir = os.path.dirname(lcc)
        os.environ["PATH"] = lccdir + os.pathsep + os.environ["PATH"]
        compilers.append(("clang", [
            os.path.abspath(a.clang), "--target=e2k-linux-gnu",
            "-march=elbrus-v3", "--sysroot=" + os.path.join(lccdir, "../fs")]))
    for label, cc in compilers:
        regvars(cc, a.work, label)
        options(cc, a.work, label)
        exceptions(cc, a.work, label)
    pragmas(compilers, a.work)
    apb([os.path.abspath(a.lcc_apb)], a.work, a.lcc_apb)
    if os.path.isdir(a.src):
        survey(a.src)


if __name__ == "__main__":
    main()
