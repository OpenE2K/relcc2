#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Ask lcc (and optionally a GCC) what building MCST's Linux kernel needs
from a compiler (spec/abi/kernel-build.md): which of the kernel's options
it accepts, which global registers it allocates under the kernel's
options, how it treats file-scope register variables and the kernel's
barriers, the no-data-stack attributes (`check_stack` and friends), the
kernel's non-volatile state-register reads, the `%L`/`%H` operand
modifiers, the speculation, CTPR, DAM, APB, jump-table, block-move and
inlining modes, the store/load spacing of `-ffix-lcc-bug123567`, which
helper functions 128-bit arithmetic calls, which relocations a module
object carries, the `-pg` call shape the kernel's recordmcount.pl and
ftrace patcher expect, and the `setwd` lines the kernel's build reads
back from objdump.  Everything is compile-only (`-S`, or `-c` and
objdump/readelf); nothing runs.

  python3 tools/abi/probes/kernelbuild.py [--lcc PATH ...] [--gcc 'CMD']
      [--section NAME ...] [--work DIR]
  python3 tools/abi/probes/kernelbuild.py --compare-wsz LCC.o OTHER.o ...

--lcc may be given several times (default: lcc 1.29.16 for e2k-v3); each
section prints one block per compiler.  --gcc takes a GCC command line
(for the proof-of-concept back end, e.g. 'toolchains/build-gcc-claude3/
gcc/xgcc -Btoolchains/build-gcc-claude3/gcc/'); GCC is given the
options a GCC-built kernel would get (the kernel's lcc-only options
dropped, as scripts/Makefile.compiler does).  --binutils is the
directory of e2k-linux-{as,objdump,readelf} (default toolchains/
binutils/bin).  Sections: options gregs gvars barrier attrs open
modifiers codegen bug123567 int128 relocs pg wsz gregapi attrs2 jumptab
blockmove pragmas asmops.  Output lines are "case: result";
spec/abi/kernel-build.md quotes them.  --compare-wsz LCC.o OTHER.o
prints, for two builds of one kernel object, how many functions get a
larger `setwd wsz` from the second compiler.  Kernel objects build on the
host without hardware, out of tree:

  make -C OpenE2K/linux O=$PWD/toolchains/kb ARCH=e2k \
      CROSS_COMPILE=e2k-linux- CC=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc \
      defconfig prepare arch/e2k/kernel/ttable.o kernel/sched/core.o

with lcc's binutils/bin in PATH (`V=1` prints each command line; a GCC
needs `scripts/config --disable FUNCTION_TRACER` while it has no -pg).
"""

import argparse
import hashlib
import os
import re
import subprocess
import sys

DEFAULT_LCC = "/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc"
DEFAULT_BINUTILS = "toolchains/binutils/bin"

FIXED_KERNEL = " ".join("-ffixed-g%d" % n for n in range(16, 26))
FIXED_ALL = " ".join("-ffixed-g%d" % n for n in range(16, 32))

# What arch/e2k/Makefile and the top Makefile give lcc for a generic
# kernel from arch/e2k/configs/defconfig (-O3 from
# CONFIG_CC_OPTIMIZE_FOR_PERFORMANCE_O3, -fsemi-spec-ld from
# CONFIG_SEMI_SPECULATIVE_KERNEL=y, -march from CONFIG_CPU_ISET_MIN).
# -pg (CONFIG_FUNCTION_TRACER) is left to the pg section.
KERNEL_COMMON = ("-O3 -std=gnu11 -fno-strict-aliasing -fno-common -fno-PIE "
                 "-fno-delete-null-pointer-checks")
KERNEL_LCC = (KERNEL_COMMON + " -fkernel -gline -mdmodel=large "
              "-fno-devirtualize -fms-extensions -masm-inline "
              "-fglobal-regs " + FIXED_KERNEL +
              " -Wassign-where-compare-meant -finline-functions-called-once "
              "-fno-loop-apb -fsemi-spec-ld -fno-ident")
# lcc 1.30+ only (1.29.16 rejects them, so the kernel's lcc-option drops
# them there).
KERNEL_LCC_131 = " -fno-spec-volatile-st -mfixed-gregs-base"
# What survives for GCC: every lcc-options group fails as a whole.
KERNEL_GCC = (KERNEL_COMMON.replace("-O3", "-O2") +
              " -fms-extensions -finline-functions-called-once "
              "-fno-strict-overflow -fno-stack-check")

# Option sets per section: name -> (lcc options, gcc options or None).
def kernel_sets(cc):
    kb = KERNEL_LCC + (KERNEL_LCC_131 if cc.v131 else "")
    entry = (kb.replace(" -fglobal-regs", "")
             .replace("-fsemi-spec-ld", "-fno-semi-spec-ld")
             + " -fexclude-ctpr2 -fno-dam")
    return {
        "user -O3": "-O3",
        "-O3 -fkernel": "-O3 -fkernel",
        "-O3 -fkernel -fglobal-regs": "-O3 -fkernel -fglobal-regs",
        "kernel": kb,
        "kernel, CONFIG_SEMI_SPECULATIVE_KERNEL=n":
            kb.replace("-fsemi-spec-ld", "-fno-semi-spec-ld"),
        "kernel entry (ttable.o)": entry,
        "kernel -fno-global-regs (iset_v3.o, boot_head.o)":
            kb + " -fno-global-regs",
        "kernel physical mode (p2v, kexec)":
            kb.replace("-fsemi-spec-ld", "-fno-semi-spec-ld") +
            " -fno-spec-ld -fno-jump-tables "
            "-ftrivial-auto-var-init=uninitialized",
    }


def gcc_sets():
    return {
        "user -O2": "-O2",
        "kernel as GCC gets it": KERNEL_GCC,
        "kernel + -ffixed-g16..g25": KERNEL_GCC + " " + FIXED_KERNEL,
        "kernel + -ffixed-g16..g31": KERNEL_GCC + " " + FIXED_ALL,
    }


class Compiler:
    def __init__(self, argv, kind, work):
        self.argv = argv
        self.kind = kind
        self.work = work
        self.name = " ".join(argv)
        if kind == "lcc":
            r = subprocess.run(argv + ["-dM", "-E", "-x", "c", "/dev/null"],
                               capture_output=True, text=True)
            m = re.search(r"#define __LCC__ (\d+)", r.stdout)
            self.v131 = bool(m and int(m.group(1)) >= 130)
            m2 = re.search(r"#define __iset__ (\d+)", r.stdout)
            self.iset = int(m2.group(1)) if m2 else 3
            m3 = re.search(r"#define __LCC_MINOR__ (\d+)", r.stdout)
            t = re.search(r"e2k-(v\d(?:\.[0-9a-z+]+)?)(?:\.linux|/|$)",
                          argv[0])
            v = int(m.group(1)) if m else 0
            self.label = "lcc 1.%d.%02d e2k-%s" % (
                v % 100, int(m3.group(1)) if m3 else 0,
                t.group(1) if t else "?")
        else:
            self.v131 = False
            r = subprocess.run(argv + ["-dM", "-E", "-x", "c", "/dev/null"],
                               capture_output=True, text=True)
            m2 = re.search(r"#define __iset__ (\d+)", r.stdout)
            self.iset = int(m2.group(1)) if m2 else 3
            self.label = "gcc (%s)" % os.path.basename(argv[0])

    def compile(self, src, opts, out="-S", ext="s"):
        """Compile SRC; return (ok, output text or object path, diags)."""
        c = os.path.join(self.work, "k.c")
        o = os.path.join(self.work, "k." + ext)
        with open(c, "w") as f:
            f.write(src + "\n")
        if os.path.exists(o):
            os.remove(o)
        r = subprocess.run(self.argv + opts.split() + [out, "k.c", "-o",
                                                        "k." + ext],
                           capture_output=True, text=True, cwd=self.work)
        diag = (r.stdout + r.stderr).strip()
        if r.returncode != 0 or not os.path.exists(o):
            return False, "", diag
        if ext == "s":
            with open(o) as f:
                return True, f.read(), diag
        return True, o, diag


def short_diag(diag, n=12):
    """First error or warning of DIAG in a few words."""
    for line in diag.splitlines():
        m = re.search(r"((?:error|warning|предупреждение|ошибка)"
                      r"(?: #\d+)?:.*)", line, re.I)
        if m:
            return " ".join(m.group(1).split()[:n])
    return " ".join(diag.split()[:n])


def body(asm, fn):
    """Assembly of function FN (from its label to its .size)."""
    m = re.search(r"^%s:\s*$" % re.escape(fn), asm, re.M)
    if not m:
        return ""
    rest = asm[m.end():]
    e = re.search(r"^\s*\.size\s+%s\b" % re.escape(fn), rest, re.M)
    return rest[:e.start()] if e else rest


def ops(text):
    """Operation lines of TEXT (no directives, braces or labels)."""
    res = []
    for line in text.splitlines():
        line = line.split("!", 1)[0].strip()
        if not line or line in "{}" or line.startswith(".") \
                or line.endswith(":"):
            continue
        res.append(line)
    return res


def instructions(text):
    """Wide instructions of TEXT: lists of operation lines.  A '{...}'
    group is one; an operation outside braces is one; a label starts a
    new block (None)."""
    res, cur, inside = [], None, False
    for raw in text.splitlines():
        line = raw.split("!", 1)[0].strip()
        if not line or (line.startswith(".") and not line.endswith(":")) \
                or line.startswith(".label"):
            continue
        if line.endswith(":"):
            res.append(None)
            continue
        if line == "{":
            inside, cur = True, []
            continue
        if line == "}":
            inside = False
            res.append(cur)
            continue
        if inside:
            cur.append(line)
        else:
            res.append([line])
    return res


def gregs(text):
    """Global registers named in TEXT, as a sorted list of numbers."""
    return sorted({int(n) for n in
                   re.findall(r"%[dxq]?g(\d+)\b", text)})


def wsz(text):
    m = re.findall(r"setwd\s+wsz\s*=\s*(0x[0-9a-f]+|\d+)", text)
    return ",".join(m) if m else "-"


def fmt_set(nums):
    if not nums:
        return "none"
    out, start, prev = [], nums[0], nums[0]
    for n in nums[1:] + [None]:
        if n is not None and n == prev + 1:
            prev = n
            continue
        out.append("%%g%d" % start if start == prev
                   else "%%g%d-%%g%d" % (start, prev))
        if n is not None:
            start = prev = n
    return " ".join(out)


# --------------------------------------------------------------------------
# options: which of the kernel's options a compiler accepts, tested the way
# scripts/Makefile.compiler's cc-option / lcc-option / lcc-options do
# (`$(CC) -Werror <options> -c -x c /dev/null`).

OPTION_TESTS = [
    # (Makefile, how it is tested, options)
    ("arch/e2k/Makefile:30", "lcc-options",
     "-fkernel -gline -mdmodel=large -fno-devirtualize"),
    ("arch/e2k/Makefile:31", "cc-option", "-fms-extensions"),
    ("arch/e2k/Makefile:31", "cc-option", "-masm-inline"),
    ("arch/e2k/Makefile:32-35", "lcc-options",
     "-fglobal-regs " + FIXED_KERNEL),
    ("arch/e2k/Makefile:36", "lcc-option", "-fno-spec-volatile-st"),
    ("arch/e2k/Makefile:37", "lcc-option", "-Wassign-where-compare-meant"),
    ("arch/e2k/Makefile:38", "lcc-option", "-finline-functions-called-once"),
    ("arch/e2k/Makefile:39", "lcc-option", "-mfixed-gregs-base"),
    ("arch/e2k/Makefile:43", "lcc-options", "-fno-loop-apb -fsemi-spec-ld"),
    ("arch/e2k/Makefile:45", "lcc-options",
     "-fno-loop-apb -fno-semi-spec-ld"),
    ("arch/e2k/Makefile:49", "plain", "-fprofile-generate-kernel"),
    ("arch/e2k/Makefile:80", "plain (not for GCC)", "-fno-ident"),
    ("arch/e2k/Makefile:108", "lcc-option", "-ffix-lcc-bug123567"),
    ("arch/e2k/kernel/Makefile:59", "lcc-option", "-fexclude-ctpr2"),
    ("arch/e2k/kernel/Makefile:59", "lcc-option", "-fno-semi-spec-ld"),
    ("arch/e2k/kernel/Makefile:61", "lcc-option", "-fno-dam"),
    ("arch/e2k/kernel/cpu/Makefile:15", "lcc-option", "-fno-global-regs"),
    ("arch/e2k/p2v/Makefile:11", "lcc-option", "-fno-spec-ld"),
    ("arch/e2k/p2v/Makefile:12", "plain", "-fno-jump-tables"),
    ("arch/e2k/p2v/Makefile:13", "plain",
     "-ftrivial-auto-var-init=uninitialized"),
    ("arch/e2k/boot/Makefile:10", "lcc-options",
     "-fno-semi-spec-ld -fno-spec-ld"),
    ("arch/e2k/Kconfig:356", "cc-option",
     "-fsanitize=kernel-address -fkernel"),
    ("Makefile:789 (CONFIG_FUNCTION_TRACER)", "plain", "-pg"),
    ("lcc manual: -fkernel implies", "plain", "-fforce-inline"),
    ("lcc manual: -fkernel implies", "plain", "-fno-use-memcpy"),
    ("lcc manual: -fkernel implies", "plain", "-fnon-std-greg-api"),
    ("lcc manual: -fkernel implies", "plain", "-mstrict-align"),
    ("lcc manual (not used by the kernel)", "plain", "-fforbid-fp"),
    ("in no lcc manual, not used by the kernel", "plain",
     "-fno-fix-lcc-bug123567"),
    ("lcc manual (not used by the kernel)", "plain", "-fno-use-memset"),
    ("scripts/recordmcount.pl:376 ($cc .= \" -mptr64\")", "plain", "-mptr64"),
    ("kernel/trace/Kconfig:731 (FTRACE_MCOUNT_USE_CC)", "cc-option",
     "-mrecord-mcount"),
    ("arch/e2k/kernel/Makefile:7 (CONFIG_VIRTUALIZATION)", "plain",
     "-Wframe-larger-than=6144"),
    ("GCC spelling of the fixed globals", "cc-option", FIXED_KERNEL),
]


def sec_options(cc, args):
    for where, how, opt in OPTION_TESTS:
        r = subprocess.run(cc.argv + ["-Werror"] + opt.split() +
                           ["-c", "-x", "c", "/dev/null", "-o",
                            os.path.join(cc.work, "o.o")],
                           capture_output=True, text=True, cwd=cc.work)
        res = "accepted" if r.returncode == 0 else \
            "rejected (%s)" % short_diag(r.stdout + r.stderr, 8)
        if r.returncode != 0 and how == "lcc-options":
            res += "; the whole group is dropped"
        elif r.returncode != 0 and how in ("lcc-option", "cc-option"):
            res += "; dropped"
        print("  %s, %s `%s`: %s" % (where, how, opt
                                     if len(opt) < 50 else opt[:47] + "...",
                                     res))


# --------------------------------------------------------------------------
# gregs: which global registers a compiler allocates as temporaries.

GREGS_SRC = r"""
extern long ext (long);
long leaf (long *p, long n)
{
  long s = 0, t = 1, u = 2, w = 3, a = 5, b = 7, c = 11, d = 13;
  for (long i = 0; i < n; i++)
    {
      s += p[i] * 3; t ^= p[i] >> 2; u += t * s; w -= u & p[i];
      a += s ^ w; b *= (t | 1); c += a * b; d ^= c + u;
    }
  return s + t + u + w + a + b + c + d;
}
long straight (long *p)
{
  long a = p[0] * p[1], b = p[2] * p[3], c = p[4] * p[5], d = p[6] * p[7];
  long e = p[8] * p[9], f = p[10] * p[11], g = p[12] * p[13];
  long h = p[14] * p[15], i = p[16] * p[17], j = p[18] * p[19];
  return (a ^ b) + (c ^ d) * (e ^ f) + (g ^ h) * (i ^ j) + a * j + b * i;
}
long nonleaf (long *p)
{
  long a = p[0] * 5, b = p[1] + 7, c = p[2] ^ 9, d = p[3] - 1;
  long r = ext (a);
  return r + a * b + c * d;
}
"""


def sec_gregs(cc, args):
    sets = kernel_sets(cc) if cc.kind == "lcc" else gcc_sets()
    for name, opts in sets.items():
        ok, asm, diag = cc.compile(GREGS_SRC, opts)
        if not ok:
            print("  %s: ERROR %s" % (name, short_diag(diag)))
            continue
        row = []
        for fn in ("leaf", "straight", "nonleaf"):
            b = body(asm, fn)
            row.append("%s %s (wsz %s)" % (fn, fmt_set(gregs(b)), wsz(b)))
        print("  %s: %s" % (name, "; ".join(row)))


# --------------------------------------------------------------------------
# gvars: file-scope register variables as the kernel declares them.

GVARS_SRC = r"""
/* asm/smp.h, asm/atomic_api.h and asm/preempt.h each declare this.  */
register unsigned long long __cpu_preempt_reg __asm__ ("%g19");
register unsigned long long __cpu_preempt_reg __asm__ ("%g19");
register unsigned long long __cpu_preempt_reg __asm__ ("%g19");
struct task { long a, b; };
register struct task *current __asm__ ("%g17");
register unsigned long my_cpu_offset __asm__ ("%g18");
register unsigned long cpuhas_greg0 __asm__ ("%g23");
register unsigned long cpuhas_greg1 __asm__ ("%g24");
extern void ext (void);
extern long ext1 (long);
long rd_call_rd (void) { long x = current->a; ext (); return x + current->a; }
unsigned long wr_call_rd (unsigned long v) { my_cpu_offset = v; ext (); return my_cpu_offset; }
void wr_call (unsigned long v) { my_cpu_offset = v; ext (); }
void wr_only (unsigned long v) { my_cpu_offset = v; }
unsigned long rd_st_rd (long *p) { unsigned long a = my_cpu_offset; p[0] = 1; return a + my_cpu_offset; }
long pass (void) { return ext1 (my_cpu_offset); }
unsigned long loop24 (long *p, long n)
{ unsigned long s = 0; for (long i = 0; i < n; i++) s += p[i] & cpuhas_greg1; return s; }
"""


def sec_gvars(cc, args):
    if cc.kind == "lcc":
        sets = {"kernel": kernel_sets(cc)["kernel"],
                "-O3 -fkernel -fglobal-regs (no -ffixed-gN)":
                    "-O3 -fkernel -fglobal-regs",
                "-O3 (user)": "-O3"}
    else:
        sets = {"kernel as GCC gets it": KERNEL_GCC,
                "kernel + -ffixed-g16..g25": KERNEL_GCC + " " + FIXED_KERNEL}
    for name, opts in sets.items():
        ok, asm, diag = cc.compile(GVARS_SRC, opts)
        d = sorted(set(re.findall(r"(?:warning|error)[^\n]*", diag)))
        print("  %s: %s; diagnostics: %s" % (
            name, "compiles" if ok else "ERROR",
            " | ".join(" ".join(x.split()[:10]) for x in d) or "none"))
        if not ok:
            continue
        for fn in ("rd_call_rd", "wr_call_rd", "wr_call", "wr_only",
                   "rd_st_rd", "pass", "loop24"):
            b = body(asm, fn)
            o = [re.sub(r",\d+(,sm)?\s", r"\1 ", x) for x in ops(b)
                 if re.search(r"%g(17|18|23|24)\b|call|disp", x)]
            print("    %s: %s" % (fn, " ; ".join(o)))


# --------------------------------------------------------------------------
# barrier: the kernel's barrier(), barrier_preemption() and barrier_data()
# (arch/e2k/include/asm/compiler.h:152-195): non-volatile asms with an
# unused "=r" output, "memory" and the per-CPU global clobbers.

BARRIER_SRC = r"""
register unsigned long my_cpu_offset __asm__ ("%%g18");
#define PREEMPTION_CLOBBERS "g19", "g18", "g20"
#define barrier() do { int unused; \
  __asm__ ("! BARRIER" : "=r" (unused) : : "memory", PREEMPTION_CLOBBERS); \
  } while (0)
#define barrier_preemption() do { int unused; \
  __asm__ ("! BARRIER" : "=r" (unused) : : PREEMPTION_CLOBBERS); } while (0)
#define barrier_data(p) do { \
  __asm__ ("! BARRIER" : : "r" (p) : "memory", PREEMPTION_CLOBBERS); \
  } while (0)
long mem (long *p) { long a = *p; %s; return a + *p; }
unsigned long greg (void) { unsigned long a = my_cpu_offset; %s; return a + my_cpu_offset; }
unsigned long loop (long *p, long n)
{ unsigned long s = 0; for (long i = 0; i < n; i++) { s += *(long *) (my_cpu_offset + (unsigned long) p); %s; } return s; }
"""


def sec_barrier(cc, args):
    opts = kernel_sets(cc)["kernel"] if cc.kind == "lcc" else KERNEL_GCC
    for b in ("barrier()", "barrier_preemption()", "barrier_data(p)",
              "__asm__ __volatile__ (\"\" : : : \"memory\")"):
        bb = b if "(p)" not in b else b
        src = BARRIER_SRC % (bb, bb.replace("(p)", "(&a)"),
                             bb.replace("(p)", "(p)"))
        ok, asm, diag = cc.compile(src, opts)
        if not ok:
            print("  %s: ERROR %s" % (b, short_diag(diag)))
            continue
        res = []
        for fn in ("mem", "greg", "loop"):
            t = body(asm, fn)
            o = ops(t)
            loads = len([x for x in o if re.match(r"ld[bhwdq]", x)])
            g18 = len(re.findall(r"%g18\b", "\n".join(o)))
            kept = t.count("BARRIER")
            res.append("%s: %d loads, %d reads of %%g18, asm text %s" % (
                fn, loads, g18, "kept" if kept else "gone"))
        print("  %s: %s" % (b, "; ".join(res)))
    # The kernel's "=r" (unused) idiom ("Prevent automatic volatile"):
    # __arch_this_cpu_write (asm/e2k_api.h:8841-8849), NATIVE_SET_AAUREG
    # (2226-2232), WRITE_UACCESS_REGS (2101-2122).
    ok, asm, diag = cc.compile(DUMMY_SRC, opts)
    if not ok:
        print("  dummy-output asms: ERROR %s" % short_diag(diag))
        return
    res = []
    for fn, mark in (("percpu_write", "std"), ("aau_write", "aaurw"),
                     ("uaccess_regs", "mmurw")):
        t = body(asm, fn)
        res.append("%s %s" % (fn, "kept" if re.search(r"\b%s" % mark, t)
                              else "deleted"))
    print("  asm with a dummy \"=r\" (unused) output: %s" % ", ".join(res))


DUMMY_SRC = r"""
void percpu_write (long *var, long val)
{ int unused; __asm__ ("std %%dg18, %[var], %[val]" : "=r" (unused)
                       : [var] "r" (var), [val] "r" (val) : "memory"); }
void aau_write (unsigned v)
{ int unused; __asm__ ("aaurw,2 %1, %%aasti1" : "=r" (unused) : "r" (v)); }
void uaccess_regs (unsigned long cont, unsigned long ptb)
{ unsigned long unused; __asm__ ("{mmurw %[p], %%root_ptb}\n{mmurw %[c], %%cont}"
  : "=r" (unused) : [p] "r" (ptb), [c] "r" (cont) : "memory", "g21", "g20"); }
"""


# --------------------------------------------------------------------------
# attrs: functions that must not use the data stack.

ATTR_CASES = [
    ("leaf arithmetic", "long f (long a, long b) { return a * b + 3; }"),
    ("call, 0 arguments", "long f (long a) { return g0 () + a; }"),
    ("call, 8 arguments",
     "long f (long a) { return g8 (a, 1, 2, 3, 4, 5, 6, 7) + 1; }"),
    ("call, 9 arguments",
     "long f (long a) { return g9 (a, 1, 2, 3, 4, 5, 6, 7, 8) + 1; }"),
    ("call, 12 arguments",
     "long f (long a) { return g12 (a, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11); }"),
    ("variadic call, 3 arguments",
     'int f (int x) { return vp ("%d %d", x, x + 1); }'),
    ("address of a local passed out",
     "int f (void) { int x = 1; g1 (&x); return x; }"),
    ("local array, variable index",
     "int f (int i) { int a[16]; for (int k = 0; k < 16; k++) a[k] = k * i;"
     " return a[i & 15]; }"),
    ("alloca", "int f (int n) { int *p = __builtin_alloca (n * 4); g1 (p);"
     " return p[0]; }"),
    ("VLA", "int f (int n) { int p[n]; g1 (p); return p[0]; }"),
    ("80-byte struct by value",
     "void f (long x) { struct big b = { { x, x, x, x, x, x, x, x, x, x } };"
     " gbig (b); }"),
    ("80-byte struct result of a call",
     "long f (long a) { struct big b = rbig (a); return b.a[3]; }"),
    ("10 incoming arguments",
     "long f (long a, long b, long c, long d, long e, long f_, long g, long h,"
     " long i, long j) { return a + b + c + d + e + f_ + g + h + i + j; }"),
    ("70 values live across a call", "long f (long *p) { " +
     " ".join("long v%d = p[%d] * p[%d];" % (i, i, i + 70) for i in range(70))
     + " g0 (); return " + " + ".join("v%d * v%d" % (i, (i + 1) % 70)
                                     for i in range(70)) + "; }"),
    ("300 values live across a call", "long f (long *p) { " +
     " ".join("long v%d = p[%d] * p[%d];" % (i, i, i + 300)
              for i in range(300))
     + " g0 (); return " + " + ".join("v%d * v%d" % (i, (i + 1) % 300)
                                     for i in range(300)) + "; }"),
    ("inlined helper with a local array",
     "static inline int helper (int i) { int a[16]; for (int k = 0; k < 16;"
     " k++) a[k] = k * i; return a[i & 15]; } int f (int i) "
     "{ return helper (i); }"),
    ("indirect call, 8 arguments", "long f (long (*fp) (long, long, long, "
     "long, long, long, long, long), long a) { return fp (a, 1, 2, 3, 4, 5, "
     "6, 7); }"),
    ("32-byte struct result of a call",
     "long f (long a) { struct mid m = rmid (a); return m.a[3]; }"),
    ("asm reads %usd.lo", "unsigned long f (void) { unsigned long v; "
     '__asm__ __volatile__ ("rrd %%usd.lo, %0" : "=r" (v)); return v; }'),
    ("noreturn call", "void f (long a) { gnr (a); }"),
]

ATTR_HEAD = r"""
extern void g1 (int *);
extern long g0 (void);
extern long g8 (long, long, long, long, long, long, long, long);
extern long g9 (long, long, long, long, long, long, long, long, long);
extern long g12 (long, long, long, long, long, long, long, long, long, long,
                 long, long);
extern int vp (const char *, ...);
struct big { long a[10]; };
extern void gbig (struct big);
extern struct big rbig (long);
extern void gnr (long) __attribute__ ((noreturn));
struct mid { long a[4]; };
extern struct mid rmid (long);
#define f ATTR f
"""


def sec_attrs(cc, args):
    if cc.kind == "lcc":
        attrs = ["", "check_stack", "check_mem", "avoid_stack"]
        opts = kernel_sets(cc)["kernel"]
    else:
        attrs = ["", "interrupt", "check_stack"]
        opts = KERNEL_GCC
    for a in attrs:
        print("  __attribute__ ((%s)):" % (a or "none"))
        for name, src in ATTR_CASES:
            head = ATTR_HEAD.replace(
                "ATTR", "__attribute__ ((%s))" % a if a else "")
            ok, asm, diag = cc.compile(head + src, opts)
            if not ok:
                print("    %s: ERROR %s" % (name, short_diag(diag)))
                continue
            t = body(asm, "f")
            o = ops(t)
            getsp = len([x for x in o if x.startswith("getsp")])
            calls = len([x for x in o if x.startswith("call")])
            stack = len([x for x in o if re.match(r"(st|ld)[bhwdq]", x)
                         and re.search(r"%r\d+,", x)])
            d = short_diag(diag) if diag else ""
            print("    %s: getsp %d, calls %d, wsz %s%s" % (
                name, getsp, calls, wsz(t),
                "; " + d if d else ""))
        head = ATTR_HEAD.replace(
            "ATTR", "__attribute__ ((%s))" % a if a else "")
        ok, asm, diag = cc.compile(head + ATTR_CASES[1][1], opts + " -pg")
        t = body(asm, "f")
        print("    call, 0 arguments, -pg: %s" % (
            "ERROR " + short_diag(diag) if not ok else
            "getsp %d, calls %d" % (len(re.findall(r"getsp", t)),
                                    len(re.findall(r"\bcall\s", t)))))


# --------------------------------------------------------------------------
# open: the kernel's NATIVE_GET_[DS]REG_OPEN (a non-volatile asm with one
# output and no inputs) around the volatile writes of
# NATIVE_SET_UPSR_IRQ_BARRIER, in loops and after calls.

OPEN_SRC = r"""
#define OPEN(reg) ({ unsigned long _r; \
  __asm__ ("rrd %%" #reg ", %0" : "=r" (_r)); _r; })
#define CLOSED(reg) ({ unsigned long _r; \
  __asm__ __volatile__ ("rrd %%" #reg ", %0" : "=r" (_r)); _r; })
#define WRITE_UPSR(v) __asm__ __volatile__ ("{rws %0, %%upsr}{wait all_e=1}" \
  : : "ri" ((unsigned) (v)) : "memory", "g19", "g18", "g20")
extern void ext (void);
static inline unsigned long rd (void) { return OPEN (upsr); }
unsigned long twice (void) { return OPEN (upsr) + OPEN (upsr); }
unsigned long around_write (void)
{ unsigned long a = OPEN (upsr); WRITE_UPSR (a & ~2UL); return a ^ OPEN (upsr); }
unsigned long around_call (void)
{ unsigned long a = OPEN (upsr); ext (); return a ^ OPEN (upsr); }
unsigned long inlined_after_call (void)
{ unsigned long a = rd (); ext (); return a ^ rd (); }
unsigned long in_loop (long n)
{ unsigned long s = 0; for (long i = 0; i < n; i++) { s += OPEN (upsr); WRITE_UPSR (s); } return s; }
unsigned long in_loop_call (long n)
{ unsigned long s = 0; for (long i = 0; i < n; i++) { s += OPEN (upsr); ext (); } return s; }
unsigned long clkr_twice (void) { return CLOSED (clkr) - CLOSED (clkr); }
void unused (void) { (void) OPEN (upsr); }
unsigned long with_input (unsigned long x)
{ unsigned long a, b; __asm__ ("addd %1, 0x1, %0" : "=r" (a) : "r" (x));
  __asm__ ("addd %1, 0x1, %0" : "=r" (b) : "r" (x)); return a ^ b; }
#define OPENS(reg) ({ unsigned int _r; \
  __asm__ ("rrs %%" #reg ", %0" : "=r" (_r)); _r; })
extern double gd;
unsigned long ip_twice (void) { return OPEN (ip) - OPEN (ip); }
unsigned long clkr_open_twice (void) { return OPEN (clkr) - OPEN (clkr); }
void sclkr_wait (unsigned long t) { while (OPEN (sclkr) < t) ; }
unsigned int fpsr_after_fp (double a)
{ unsigned int x = OPENS (fpsr); gd = gd / a; return x ^ OPENS (fpsr); }
unsigned long lsr_twice (void) { return OPEN (lsr) ^ OPEN (lsr); }
"""


def sec_open(cc, args):
    if cc.kind == "lcc":
        base = kernel_sets(cc)["kernel"]
        sets = {"kernel (-masm-inline)": base,
                "kernel -mno-asm-inline": base + " -mno-asm-inline",
                "kernel -O2": base.replace("-O3", "-O2"),
                "-O3 (lcc's default: no -masm-inline)": "-O3"}
    else:
        sets = {"kernel as GCC gets it": KERNEL_GCC}
    for name, opts in sets.items():
        ok, asm, diag = cc.compile(OPEN_SRC, opts)
        if not ok:
            print("  %s: ERROR %s" % (name, short_diag(diag)))
            continue
        res = []
        for fn in ("twice", "around_write", "around_call",
                   "inlined_after_call", "in_loop", "in_loop_call",
                   "clkr_twice", "unused", "with_input", "ip_twice",
                   "clkr_open_twice", "sclkr_wait", "fpsr_after_fp",
                   "lsr_twice"):
            t = body(asm, fn)
            n = len(re.findall(r"addd\S*\s+%[rg]\d+, 0x1," if
                               fn == "with_input" else
                               r"rr[ds]\S*\s+%(upsr|clkr|ip|sclkr|fpsr|lsr)",
                               t))
            # In a loop: is the read inside the loop body (after the
            # first label)?
            res.append("%s %d" % (fn, n))
        print("  %s: asm count: %s" % (name, ", ".join(res)))
        for fn in ("in_loop", "in_loop_call", "sclkr_wait"):
            t = body(asm, fn)
            ins = [i for i in instructions(t)]
            # index of the first label (loop head) and positions of rrd
            pos = [k for k, i in enumerate(ins) if i and
                   any(re.match(r"rrd\S*\s+%s?(upsr|sclkr)", x) for x in i)]
            labels = [k for k, i in enumerate(ins) if i is None]
            back = [k for k, i in enumerate(ins) if i and
                    any(re.match(r"(ct|ibranch)\s", x) and "%ctpr3" not in x
                        for x in i)]
            print("    %s: rrd at instruction %s, labels at %s, "
                  "branches at %s" % (fn, pos, labels, back))


# --------------------------------------------------------------------------
# modifiers: %L and %H on the operand types the kernel uses.

MOD_CASES = [
    ("__int128, r", "__int128", "r", "v"),
    ("unsigned __int128, =&r out", "unsigned __int128", "=&r", None),
    ("struct { u64 lo, hi }, r", "struct q", "r", "v"),
    ("struct { u64 lo, hi }, =&r out", "struct q", "=&r", None),
    ("union { __int128; struct }, r", "union u", "r", "v"),
    ("u64, r", "unsigned long", "r", "v"),
    ("u32, r", "unsigned int", "r", "v"),
    ("long double, x", "long double", "x", "v"),
    ("__int128, x", "__int128", "x", "v"),
    ("constant 5, i", None, "i", "5"),
    ("constant 1L << 40, i", None, "i", "1L << 40"),
]


def sec_modifiers(cc, args):
    head = ("struct q { unsigned long lo, hi; }; "
            "union u { unsigned __int128 w; struct q s; }; ")
    opts = kernel_sets(cc)["kernel"] if cc.kind == "lcc" else KERNEL_GCC
    for mods in (("%0", "%L0", "%H0"),):
        for name, typ, con, val in MOD_CASES:
            row = []
            for m in mods:
                if con.startswith("="):
                    src = head + ("void f (%s *p) { %s v; __asm__ "
                                  "__volatile__ (\"! OUT %s\" : \"%s\" (v)); "
                                  "*p = v; }" % (typ, typ, m, con))
                elif typ is None:
                    src = head + ("void f (void) { __asm__ __volatile__ "
                                  "(\"! OUT %s\" : : \"%s\" (%s)); }"
                                  % (m, con, val))
                else:
                    src = head + ("void f (%s *p) { %s v = *p; __asm__ "
                                  "__volatile__ (\"! OUT %s\" : : \"%s\" "
                                  "(v)); }" % (typ, typ, m, con))
                ok, asm, diag = cc.compile(src, opts)
                if not ok:
                    row.append("%s: ERROR %s" % (m, short_diag(diag, 10)))
                    continue
                mm = re.search(r"! OUT\s*([^\n]*)", asm)
                row.append("%s: %s" % (m, mm.group(1).strip() if mm
                                       else "(text not found)"))
            print("  %s: %s" % (name, "; ".join(row)))


# --------------------------------------------------------------------------
# codegen: the kernel-mode code generation switches.

CODEGEN_SRC = r"""
struct big { long a[32]; };
extern long gvar, garr[1024];
extern void ext (void);
extern long ext1 (long);
register unsigned long cpu_off __asm__ ("%g18");
volatile int *mmio;
/* Conditional loads of four kinds of address.  */
long ld_ptr (long *p, long *q, int c) { long r = 0; if (c) r = *p + *q; return r; }
long ld_glob (int c) { long r = 0; if (c) r = gvar + garr[5]; return r; }
long ld_stack (int c, int i) { long a[8]; for (int k = 0; k < 8; k++) a[k] = ext1 (k); long r = 0; if (c) r = a[i & 7]; return r; }
long ld_greg (int c) { long r = 0; if (c) r = *(long *) (cpu_off + 64); return r; }
long ld_loop (long *a, long *b, long n) { long s = 0; for (long i = 0; i < n; i++) if (b[i]) s += a[i]; return s; }
/* Stores through volatile pointers under a condition, of loaded data.  */
void st_vol (int c, int *p) { if (c) *mmio = *p + 1; }
void st_vol_loop (volatile int *v, int *a, int *c, long n) { for (long i = 0; i < n; i++) if (c[i]) v[i] = a[i] * 3; }
/* Array prefetch and DAM candidates.  */
double sum (double *a, double *b, int n) { double s = 0; for (int i = 0; i < n; i++) s += a[i] * b[i]; return s; }
void axpy (long *x, long *y, long n) { for (long i = 0; i < n; i++) y[i] += 3 * x[i]; }
void dam (long *p, int *q, long n) { for (long i = 0; i < n; i++) { *q = (int) i; p[i] = p[i + 1] + 1; } }
/* A switch, a loop with a call, an indirect call.  */
extern int f0 (int), f1 (int), f2 (int), f3 (int), f4 (int), f5 (int),
  f6 (int), f7 (int), f8 (int);
int sw (int x) { switch (x) { case 0: return f0 (x); case 1: return f1 (x + 1);
  case 2: return f2 (x * 3); case 3: return f3 (x - 7); case 4: return f4 (x ^ 5);
  case 5: return f5 (x | 9); case 6: return f6 (x & 3); case 7: return f7 (x << 2);
  case 8: return f8 (x >> 1); default: return 0; } }
long calls (long n, long (*fp) (long)) { long s = 0; for (long i = 0; i < n; i++) s += ext1 (i) + fp (i); return s; }
/* Block moves.  */
void copy (struct big *d, struct big *s) { *d = *s; }
void bcopy512 (char *d, char *s) { __builtin_memcpy (d, s, 512); }
void bzero512 (char *d) { __builtin_memset (d, 0, 512); }
struct huge { long a[512]; };
void copy4k (struct huge *d, struct huge *s) { *d = *s; }
void bcopyn (char *d, char *s, long n) { __builtin_memcpy (d, s, n); }
void bzeron (char *d, long n) { __builtin_memset (d, 0, n); }
/* Inlining of plain `inline'.  */
static inline long helper (long *p, long n)
{ long s = 0;
  for (long i = 0; i < n; i++) s += p[i] * 3 + (p[i] >> 1) - (p[i] ^ 0x55) + p[i] / 3;
  for (long i = 0; i < n; i++) s += p[i] * 4 + (p[i] >> 2) - (p[i] ^ 0x56) + p[i] / 4;
  for (long i = 0; i < n; i++) s += p[i] * 5 + (p[i] >> 3) - (p[i] ^ 0x57) + p[i] / 5;
  for (long i = 0; i < n; i++) s += p[i] * 6 + (p[i] >> 4) - (p[i] ^ 0x58) + p[i] / 6;
  for (long i = 0; i < n; i++) s += p[i] * 7 + (p[i] >> 5) - (p[i] ^ 0x59) + p[i] / 7;
  for (long i = 0; i < n; i++) s += p[i] * 8 + (p[i] >> 6) - (p[i] ^ 0x5a) + p[i] / 8;
  for (long i = 0; i < n; i++) s += p[i] * 9 + (p[i] >> 7) - (p[i] ^ 0x5b) + p[i] / 9;
  for (long i = 0; i < n; i++) s += p[i] * 10 + (p[i] >> 1) - (p[i] ^ 0x5c) + p[i] / 10;
  for (long i = 0; i < n; i++) s += p[i] * 11 + (p[i] >> 2) - (p[i] ^ 0x5d) + p[i] / 11;
  for (long i = 0; i < n; i++) s += p[i] * 12 + (p[i] >> 3) - (p[i] ^ 0x5e) + p[i] / 12;
  for (long i = 0; i < n; i++) s += p[i] * 13 + (p[i] >> 4) - (p[i] ^ 0x5f) + p[i] / 13;
  for (long i = 0; i < n; i++) s += p[i] * 14 + (p[i] >> 5) - (p[i] ^ 0x60) + p[i] / 14;
  for (long i = 0; i < n; i++) s += p[i] * 15 + (p[i] >> 6) - (p[i] ^ 0x61) + p[i] / 15;
  for (long i = 0; i < n; i++) s += p[i] * 16 + (p[i] >> 7) - (p[i] ^ 0x62) + p[i] / 16;
  for (long i = 0; i < n; i++) s += p[i] * 17 + (p[i] >> 1) - (p[i] ^ 0x63) + p[i] / 17;
  for (long i = 0; i < n; i++) s += p[i] * 18 + (p[i] >> 2) - (p[i] ^ 0x64) + p[i] / 18;
  for (long i = 0; i < n; i++) s += p[i] * 19 + (p[i] >> 3) - (p[i] ^ 0x65) + p[i] / 19;
  for (long i = 0; i < n; i++) s += p[i] * 20 + (p[i] >> 4) - (p[i] ^ 0x66) + p[i] / 20;
  for (long i = 0; i < n; i++) s += p[i] * 21 + (p[i] >> 5) - (p[i] ^ 0x67) + p[i] / 21;
  for (long i = 0; i < n; i++) s += p[i] * 22 + (p[i] >> 6) - (p[i] ^ 0x68) + p[i] / 22;
  for (long i = 0; i < n; i++) s += p[i] * 23 + (p[i] >> 7) - (p[i] ^ 0x69) + p[i] / 23;
  for (long i = 0; i < n; i++) s += p[i] * 24 + (p[i] >> 1) - (p[i] ^ 0x6a) + p[i] / 24;
  for (long i = 0; i < n; i++) s += p[i] * 25 + (p[i] >> 2) - (p[i] ^ 0x6b) + p[i] / 25;
  for (long i = 0; i < n; i++) s += p[i] * 26 + (p[i] >> 3) - (p[i] ^ 0x6c) + p[i] / 26;
  return s; }
long use_helper (long *p, long *q, long n) { return helper (p, n) + helper (q, n) + helper (p + 1, n - 1) + helper (q + 1, n - 1); }
/* Unaligned 8-byte read.  */
unsigned long get8 (const unsigned char *p) { unsigned long v; __builtin_memcpy (&v, p, 8); return v; }
"""


def count_codegen(asm):
    res = {}
    for fn in ("ld_ptr", "ld_glob", "ld_stack", "ld_greg", "ld_loop"):
        o = ops(body(asm, fn))
        sm = [x for x in o if re.match(r"ld[bhwdq]\S*,sm", x)]
        m3 = [x for x in sm if "mas=0x3" in x]
        res[fn] = "%d ld,sm (%d mas=0x3)" % (len(sm), len(m3))
    for fn in ("st_vol", "st_vol_loop"):
        o = ops(body(asm, fn))
        res[fn] = "%d st,sm of %d st" % (
            len([x for x in o if re.match(r"st[bhwdq]\S*,sm", x)]),
            len([x for x in o if re.match(r"st[bhwdq]", x)]))
    for fn in ("sum", "axpy", "dam"):
        t = body(asm, fn)
        res[fn] = "bap %d, mova %d, lock loads %d, rbranch %d" % (
            len(re.findall(r"\bbap\b", t)), len(re.findall(r"\bmova", t)),
            len(re.findall(r"mas=0x4", t)), len(re.findall(r"\brbranch", t)))

    whole = "\n".join(body(asm, f) for f in
                      ("ld_ptr", "ld_glob", "ld_stack", "ld_greg", "ld_loop",
                       "st_vol", "st_vol_loop", "sum", "axpy", "dam", "sw",
                       "calls", "copy", "bcopy512", "bzero512", "copy4k",
                       "bcopyn", "bzeron", "use_helper", "get8"))
    rb = [x for x in ops(whole) if x.startswith("rbranch")]
    intfp = [x.split()[0] for x in ops(whole.replace(body(asm, "sum"), ""))
             if (re.match(r"f[a-z]+", x) and not x.startswith("fapb"))
             or re.match(r"mov[fi][fi]\b", x)]
    res["all functions"] = ("%%ctpr2 %d, APB (ldisp %d, mova %d), "
                            "rbranch ? %%dt_alN %d, "
                            "rbranch without a condition (DAM) %d, FP ops in "
                            "integer functions %s" % (
        len(re.findall(r"%ctpr2", whole)),
        len(re.findall(r"\bldisp\s+%ctpr2", whole)),
        len(re.findall(r"\bmova", whole)),
        len([x for x in rb if "%dt_al" in x]),
        len([x for x in rb if "?" not in x]), ",".join(sorted(set(intfp)))
        or "none"))
    t = body(asm, "sw")
    res["sw"] = "jump table %s" % (
        "yes (load from .T.N, movtd)" if re.search(r"\.T\.\d", t)
        and re.search(r"movtd", t) else
        "yes (movtd)" if re.search(r"movtd", t) else "no")
    def callees(t):
        return [m.group(1) for m in
                re.finditer(r"\bdisp\s+%ctpr\d,\s*([\w.$@]+)(=?)", t)
                if not m.group(2) and not m.group(1).startswith(".L")]
    for fn in ("copy", "bcopy512", "bzero512", "copy4k", "bcopyn", "bzeron",
               "use_helper"):
        res[fn] = "calls %s" % (",".join(callees(body(asm, fn))) or "none")
    o = ops(body(asm, "get8"))
    res["get8"] = "loads %s" % ",".join(sorted(
        {re.match(r"(ld[bhwdq]\w*)", x).group(1) for x in o
         if re.match(r"ld[bhwdq]", x)}) or ["none"])
    return res


def sec_codegen(cc, args):
    if cc.kind == "lcc":
        sets = kernel_sets(cc)
        sets = {k: v for k, v in sets.items()
                if "-fno-global-regs" not in k and "-fglobal-regs" not in k}
        if cc.iset >= 5:
            sets["kernel -mno-strict-align"] = sets["kernel"] + \
                " -mno-strict-align"
        sets["kernel -fno-dam"] = sets["kernel"] + " -fno-dam"
        sets["kernel -fexclude-ctpr2"] = sets["kernel"] + " -fexclude-ctpr2"
        if cc.v131:
            sets["kernel -fspec-volatile-st"] = sets["kernel"].replace(
                "-fno-spec-volatile-st", "-fspec-volatile-st")
            # 1.30+ split the DAM of software-pipelined loops off -fno-dam.
            sets["kernel -fno-dam -fno-swp-dam"] = sets["kernel"] + \
                " -fno-dam -fno-swp-dam"
    else:
        sets = {"kernel as GCC gets it": KERNEL_GCC,
                "kernel -Os": KERNEL_GCC.replace("-O2", "-Os")}
    for name, opts in sets.items():
        ok, asm, diag = cc.compile(CODEGEN_SRC, opts)
        if not ok:
            print("  %s: ERROR %s" % (name, short_diag(diag)))
            continue
        res = count_codegen(asm)
        print("  %s:" % name)
        for k, v in res.items():
            print("    %s: %s" % (k, v))
    if cc.kind == "lcc":
        ok, asm, diag = cc.compile(
            "double f (double a) { return a * 2.0; }\n"
            "long g (long a, long b) { return a / b + a % b; }",
            "-O3 -fkernel -fforbid-fp")
        print("  -fforbid-fp on a double function: %s" % (
            "compiles" if ok else "ERROR " + short_diag(diag)))


# --------------------------------------------------------------------------
# bug123567: how close lcc puts a load after a store that may intersect it.

BUG_SRC = r"""
long st_ld (long *p, int *q) { *q = 5; return *p; }
long st_ld2 (long *p, long *q, long x) { *q = x; return *p + 1; }
int st_ld_part (char *p, long x) { *(long *) p = x; return *(volatile int *) (p + 4); }
long st_ld_loop (long *a, int *b, long n) { long s = 0; for (long i = 0; i < n; i++) { *b = (int) i; s += a[i]; } return s; }
"""


def store_load_gaps(text, checks=False):
    """For each load, the number of wide instructions since the latest
    store in the same block (1 = the next instruction; 0 = the same
    instruction with the store taking effect first, that is the store in
    ALC0-2 and the load in ALC3-5: bundle.md).  DAM check loads
    (mas=0x3) are left out unless CHECKS.  Empty instructions ('nop' on
    its own line) count like any other."""
    def chan(x):
        m = re.match(r"\w+,(\d)", x)
        return int(m.group(1)) if m else None
    gaps = []
    last = None
    for k, ins in enumerate(instructions(text)):
        if ins is None:
            last = None
            continue
        lds = [x for x in ins if re.match(r"ld[bhwdq]", x)
               and "%empty" not in x and (checks or "mas=0x3" not in x)]
        sts = [x for x in ins if re.match(r"st[bhwdq]", x)]
        if lds and last is not None:
            gaps.append(k - last)
        if any(chan(s) is not None and chan(s) <= 2 and chan(l) is not None
               and chan(l) >= 3 for s in sts for l in lds):
            gaps.append(0)
        if sts:
            last = k
    return gaps


def sec_bug123567(cc, args):
    base = kernel_sets(cc)["kernel"] if cc.kind == "lcc" else KERNEL_GCC
    sets = {"-O3": "-O3", "-O3 -ffix-lcc-bug123567":
            "-O3 -ffix-lcc-bug123567",
            "-O3 -fno-fix-lcc-bug123567": "-O3 -fno-fix-lcc-bug123567",
            "kernel": base, "kernel -ffix-lcc-bug123567":
            base + " -ffix-lcc-bug123567"} if cc.kind == "lcc" else \
        {"-O2": "-O2"}
    for name, opts in sets.items():
        ok, asm, diag = cc.compile(BUG_SRC, opts)
        if not ok:
            print("  %s: ERROR %s" % (name, short_diag(diag)))
            continue
        res = []
        empty = sum(len(re.findall(r"^\s*nop\s*$", body(asm, fn), re.M))
                    for fn in ("st_ld", "st_ld2", "st_ld_part", "st_ld_loop"))
        for fn in ("st_ld", "st_ld2", "st_ld_part", "st_ld_loop"):
            g = store_load_gaps(body(asm, fn))
            gc = store_load_gaps(body(asm, fn), checks=True)
            res.append("%s %s%s" % (fn, min(g) if g else "-",
                                    " (DAM check load %d)" % min(gc)
                                    if gc and min(gc) < (min(g) if g else 99)
                                    else ""))
        print("  %s: smallest store-to-load distance in wide instructions "
              "(0: the same one, store first): "
              "%s; empty wide instructions (a bare nop) %d; md5 %s" % (
                  name, ", ".join(res), empty,
                  hashlib.md5(asm.encode()).hexdigest()[:8]))


# --------------------------------------------------------------------------
# int128: which helper functions 128-bit and bit arithmetic call.

INT128_SRC = r"""
typedef unsigned long u64;
typedef unsigned __int128 u128;
u64 mul_u64_u64_shr (u64 a, u64 mul, unsigned int shift)
{ return (u64) (((u128) a * mul) >> shift); }
u64 mul_u64_u32_shr (u64 a, unsigned int mul, unsigned int shift)
{ return (u64) (((u128) a * mul) >> shift); }
u128 mul128 (u128 a, u128 b) { return a * b; }
u128 shl128 (u128 a, unsigned n) { return a << n; }
u128 shr128 (u128 a, unsigned n) { return a >> n; }
__int128 sar128 (__int128 a, unsigned n) { return a >> n; }
u128 div128 (u128 a, u128 b) { return a / b; }
u128 mod128 (u128 a, u128 b) { return a % b; }
int cmp128 (u128 a, u128 b) { return a < b; }
u64 div64 (u64 a, u64 b) { return a / b; }
int clz64 (u64 a) { return __builtin_clzl (a); }
int ctz64 (u64 a) { return __builtin_ctzl (a); }
int pop64 (u64 a) { return __builtin_popcountl (a); }
int ffs64 (long a) { return __builtin_ffsl (a); }
u64 bswap64 (u64 a) { return __builtin_bswap64 (a); }
int parity64 (u64 a) { return __builtin_parityl (a); }
"""

INT128_FNS = ["mul_u64_u64_shr", "mul_u64_u32_shr", "mul128", "shl128",
              "shr128", "sar128",
              "div128", "mod128", "cmp128", "div64", "clz64", "ctz64",
              "pop64", "ffs64", "bswap64", "parity64"]


def sec_int128(cc, args):
    if cc.kind == "lcc":
        sets = {"kernel": kernel_sets(cc)["kernel"],
                "kernel -O2": kernel_sets(cc)["kernel"].replace("-O3", "-O2")}
    else:
        sets = {"kernel -O2": KERNEL_GCC,
                "kernel -Os": KERNEL_GCC.replace("-O2", "-Os")}
    for name, opts in sets.items():
        ok, asm, diag = cc.compile(INT128_SRC, opts)
        if not ok:
            print("  %s: ERROR %s" % (name, short_diag(diag)))
            continue
        res = []
        for fn in INT128_FNS:
            t = body(asm, fn)
            callees = sorted(set(re.findall(
                r"disp\s+%ctpr\d,\s*([\w.$@]+)", t)))
            fp = sorted({x.split()[0].split(",")[0] for x in ops(t)
                         if re.match(r"f[a-z]", x) or x.startswith("mov")
                         and x.split()[0].startswith(("movif", "movfi"))})
            res.append("%s: %s%s" % (fn, ",".join(callees) or "-",
                                     " (FP ops %s)" % ",".join(fp)
                                     if fp else ""))
        print("  %s: %s" % (name, "; ".join(res)))


# --------------------------------------------------------------------------
# relocs: what relocations a module object carries.

RELOC_SRC = r"""
extern int ext_var;
static int st_var[100];
extern void ext_fn (int);
static void st_fn (int x) { st_var[x & 63] = x; }
void (*fn_table[3]) (int) = { ext_fn, st_fn, 0 };
const char *msg = "hello";
int __attribute__ ((section (".entry.text"))) in_entry (int x) { return x + 1; }
int sw (int x) { switch (x) { case 0: return 11; case 1: return 23; case 2: return 37;
  case 3: return 41; case 4: return 53; case 5: return 67; case 6: return 79;
  case 7: return 83; case 8: return 97; default: return 0; } }
int use (int x)
{
  ext_fn (x); st_fn (x); fn_table[x & 1] (x);
  return ext_var + st_var[x & 63] + msg[x & 3] + in_entry (x) + sw (x);
}
struct ksym { unsigned long value; const char *name; };
static const struct ksym ksym_use __attribute__ ((used, section ("___ksymtab+use")))
  = { (unsigned long) use, "use" };
"""


def sec_relocs(cc, args):
    opts = (kernel_sets(cc)["kernel"] if cc.kind == "lcc" else KERNEL_GCC) + \
        " -DMODULE"
    ok, obj, diag = cc.compile(RELOC_SRC, opts, out="-c", ext="o")
    if not ok and cc.kind != "lcc":
        ok, asm, diag = cc.compile(RELOC_SRC, opts)
        if ok:
            s = os.path.join(cc.work, "k.s")
            obj = os.path.join(cc.work, "k.o")
            r = subprocess.run([os.path.join(args.binutils, "e2k-linux-as"),
                                "-mcpu=elbrus-v%d" % cc.iset, s, "-o", obj],
                               capture_output=True, text=True)
            ok = r.returncode == 0
            diag = r.stderr
    if not ok:
        print("  ERROR %s" % short_diag(diag))
        return
    r = subprocess.run([os.path.join(args.binutils, "e2k-linux-readelf"),
                        "-rW", obj], capture_output=True, text=True)
    sec, counts = None, {}
    for line in r.stdout.splitlines():
        m = re.match(r"Relocation section '\.rela?(\S+)'", line)
        if m:
            sec = m.group(1)
            continue
        m = re.match(r"\s*[0-9a-f]+\s+[0-9a-f]+\s+(R_E2K_\w+)", line)
        if m and sec:
            counts.setdefault(sec, {}).setdefault(m.group(1), 0)
            counts[sec][m.group(1)] += 1
    for s, c in counts.items():
        print("  %s: %s" % (s, ", ".join("%s %d" % kv for kv in
                                         sorted(c.items()))))


# --------------------------------------------------------------------------
# pg: the -pg call shape against scripts/recordmcount.pl (e2k part) and
# arch/e2k/kernel/ftrace.c (the call's instruction needs SS and CS1; ftrace
# flips SS.ct).

PG_SRC = r"""
extern long ext (long);
long leaf (long a) { return a * 3; }
long nonleaf (long a) { return ext (a) + 1; }
long many (long a, long b, long c, long d, long e, long f, long g, long h,
           long i, long j) { return ext (a + b + c + d + e + f + g + h + i + j); }
__attribute__ ((no_instrument_function)) long notrace_fn (long a) { return ext (a); }
long loop (long *p, long n) { long s = 0; for (long i = 0; i < n; i++) s += ext (p[i]); return s; }
"""


def recordmcount_e2k(dump):
    """scripts/recordmcount.pl's e2k logic on `objdump -dr` output: the
    call sites it records, and its errors."""
    sites, errors = [], []
    ip = 0
    disp_ctpr = mcount_ctpr = ""
    found = icall = False
    for line in dump.splitlines():
        m = re.match(r"^\s*([0-9a-fA-F]+):\s+([0-9a-fA-F][0-9a-fA-F])+", line)
        m2 = re.match(r"^\s*([0-9a-fA-F]+):\s*$", line)
        if m:
            ip = int(m.group(1), 16)
        elif m2:
            ip = int(m2.group(1), 16)
        elif re.match(r"^\s*disp %(ctpr[1-3]),", line):
            disp_ctpr = re.match(r"^\s*disp %(ctpr[1-3]),", line).group(1)
            icall = False
        elif re.match(r"^\s*icall\s", line):
            icall = True
        elif re.match(r"^\s*[0-9a-fA-F]+:\s*R_E2K_DISP\s*_?mcount", line):
            if icall:
                icall = False
                sites.append(ip)
            else:
                if found:
                    errors.append("mcount_disp_found already set at %x" % ip)
                found = True
                mcount_ctpr = disp_ctpr
        elif found:
            if re.match(r"^\s*[ir]branch", line):
                errors.append("branch at %x" % ip)
            if re.match(r"^\s*ct ", line):
                errors.append("ct at %x" % ip)
            if re.match(r"^\s*call %%%s, wbs = 0x[0-9a-fA-F]+" % mcount_ctpr,
                        line):
                found = False
                sites.append(ip)
    return sites, errors


def sec_pg(cc, args):
    opts = ((kernel_sets(cc)["kernel"] if cc.kind == "lcc" else KERNEL_GCC)
            + " -pg")
    ok, asm, diag = cc.compile(PG_SRC, opts)
    if not ok:
        print("  -S: ERROR %s" % short_diag(diag))
        return
    for fn in ("leaf", "nonleaf", "many", "notrace_fn", "loop"):
        t = body(asm, fn)
        ins = [i for i in instructions(t) if i]
        first = []
        for i in ins:
            first.append("{ " + "; ".join(re.sub(r"\s+", " ", x) for x in i)
                         + " }")
            if any(x.startswith("call") and "_mcount" in t for x in i):
                break
            if len(first) >= 4:
                break
        print("  %s: %s" % (fn, " ".join(first)))
    s = os.path.join(cc.work, "pg.s")
    with open(s, "w") as f:
        f.write(asm)
    obj = os.path.join(cc.work, "pg.o")
    r = subprocess.run([os.path.join(args.binutils, "e2k-linux-as"),
                        "-mcpu=elbrus-v%d" % cc.iset, s, "-o", obj],
                       capture_output=True, text=True)
    if r.returncode != 0:
        print("  as: ERROR %s" % short_diag(r.stderr))
        return
    r = subprocess.run([os.path.join(args.binutils, "e2k-linux-objdump"),
                        "-w", "-dr", obj], capture_output=True, text=True,
                       env=dict(os.environ, LANG="C", LC_ALL="C"))
    sites, errors = recordmcount_e2k(r.stdout)
    data = open(obj, "rb").read()
    # Section .text offset for checking HS bits of the call instruction.
    rr = subprocess.run([os.path.join(args.binutils, "e2k-linux-readelf"),
                         "-SW", obj], capture_output=True, text=True)
    off = None
    for line in rr.stdout.splitlines():
        m = re.search(r"\]\s+\.text\s+\S+\s+[0-9a-f]+\s+([0-9a-f]+)", line)
        if m:
            off = int(m.group(1), 16)
    checks = []
    for ip in sites:
        if off is None:
            break
        hs = int.from_bytes(data[off + ip:off + ip + 4], "little")
        ss = int.from_bytes(data[off + ip + 4:off + ip + 8], "little")
        # HS.s is bit 12, HS.c bits 14-15 (CS1 = bit 15); SS.ct (the
        # condition type ftrace.c flips between 1 and 0) is bits 5-8
        # (kernel asm/instr_regs_types.h, instr_ss_t).
        checks.append("%x: HS.s %d, HS.c1 %d, SS.ct %d" % (
            ip, (hs >> 12) & 1, (hs >> 15) & 1, (ss >> 5) & 0xf))
    print("  recordmcount.pl (e2k): %d call sites %s; errors: %s" % (
        len(sites), "[" + "; ".join(checks) + "]",
        "; ".join(errors) or "none"))


# --------------------------------------------------------------------------
# wsz: the setwd lines arch/e2k/kernel/Makefile reads back with
# `objdump -d | sed 's/.*setwd wsz = \(0x...\).*/'`.

WSZ_SRC = r"""
extern long ext (long);
long user_trap_handler (long *p, long n)
{ long s = 0; for (long i = 0; i < n; i++) s += ext (p[i]) * p[i + 1]; return s; }
long handle_sys_call (long a) { return a + 1; }
"""


def sec_wsz(cc, args):
    opts = ((kernel_sets(cc)["kernel entry (ttable.o)"] if cc.kind == "lcc"
             else KERNEL_GCC))
    ok, asm, diag = cc.compile(WSZ_SRC, opts)
    if not ok:
        print("  ERROR %s" % short_diag(diag))
        return
    s = os.path.join(cc.work, "w.s")
    with open(s, "w") as f:
        f.write(asm)
    obj = os.path.join(cc.work, "w.o")
    r = subprocess.run([os.path.join(args.binutils, "e2k-linux-as"),
                        "-mcpu=elbrus-v%d" % cc.iset, s, "-o", obj],
                       capture_output=True, text=True)
    if r.returncode != 0:
        print("  as: ERROR %s" % short_diag(r.stderr))
        return
    dump = subprocess.run([os.path.join(args.binutils, "e2k-linux-objdump"),
                           "-me2k", "-d", obj], capture_output=True,
                          text=True).stdout
    for fn in ("user_trap_handler", "handle_sys_call"):
        # The Makefile's sed: lines from <fn> to the next "<...>:", the
        # setwd wsz values, sort -g, last.
        m = re.search(r"<%s>:(.*?)(?=\n[0-9a-f]+ <|\Z)" % fn, dump, re.S)
        vals = re.findall(r"setwd wsz = (0x[0-9a-f]+)", m.group(1)) if m \
            else []
        print("  %s: compiler prints %s; objdump setwd values %s; the "
              "Makefile's sed takes %s" % (
                  fn, wsz(body(asm, fn)), vals or "none",
                  max(vals, key=lambda v: int(v, 16)) if vals else
                  "nothing (empty #define)"))


# --------------------------------------------------------------------------
# gregapi: what lcc's global-register options do, and whether a file-scope
# register variable reserves its register in the functions that don't use
# it (in GCC it does: the register becomes fixed for the rest of the file).

GREGAPI_SRC = r"""
register unsigned long gv20 __asm__ ("%g20");
register unsigned long gv28 __asm__ ("%g28");
extern void ext (void);
long straight (long *p)
{
  long a = p[0] * p[1], b = p[2] * p[3], c = p[4] * p[5], d = p[6] * p[7];
  long e = p[8] * p[9], f = p[10] * p[11], g = p[12] * p[13];
  long h = p[14] * p[15], i = p[16] * p[17], j = p[18] * p[19];
  return (a ^ b) + (c ^ d) * (e ^ f) + (g ^ h) * (i ^ j) + a * j + b * i;
}
unsigned long rd_call_rd (void) { unsigned long a = gv20; ext (); return a ^ gv20; }
unsigned long wr_call_rd (unsigned long v) { gv20 = v; ext (); return gv20; }
void wr_twice (unsigned long v) { gv20 = v; gv20 = v + 1; }
"""

# The same function in a file without register variables.
GREGAPI_PLAIN_SRC = GREGAPI_SRC[GREGAPI_SRC.index("long straight"):
                                GREGAPI_SRC.index("unsigned long rd_call_rd")]


def sec_gregapi(cc, args):
    if cc.kind == "lcc":
        sets = ["-O3", "-O3 -fnon-std-greg-api", "-O3 -fno-global-regs",
                "-O3 -fglobal-regs", "-O3 -fkernel",
                "-O3 -fkernel -fglobal-regs",
                "-O3 -fkernel -fglobal-regs " + FIXED_KERNEL]
    else:
        sets = ["-O2", "-O2 " + FIXED_KERNEL]
    for opts in sets:
        ok, asm, diag = cc.compile(GREGAPI_SRC, opts)
        name = opts.replace(FIXED_KERNEL, "-ffixed-g16..g25")
        if not ok:
            print("  %s: ERROR %s" % (name, short_diag(diag)))
            continue
        ok2, asm2, _ = cc.compile(GREGAPI_PLAIN_SRC, opts)
        st = gregs(body(asm, "straight"))
        st2 = gregs(body(asm2, "straight")) if ok2 else []
        d = sorted(set(re.findall(r"warning[^\n]*", diag)))
        print("  %s: straight() in the file with gv20/gv28 uses %s (%%g20 %s, "
              "%%g28 %s); in a file without them %s; reads of %%g20 in "
              "rd_call_rd %d; writes in wr_twice %d%s" % (
                  name, fmt_set(st),
                  "clobbered" if 20 in st else "kept",
                  "clobbered" if 28 in st else "kept", fmt_set(st2),
                  len(re.findall(r"%g20\b", body(asm, "rd_call_rd"))),
                  len(re.findall(r"%g20\b", body(asm, "wr_twice"))),
                  "; " + " | ".join(" ".join(x.split()[:8]) for x in d)
                  if d else ""))


# --------------------------------------------------------------------------
# attrs2: where check_stack and check_mem draw the line.

ATTRS2_CASES = [
    ("check_mem", "a global read",
     "extern long gvar; ATTR long f (void) { return gvar; }"),
    ("check_mem", "a store through a pointer",
     "ATTR void f (long *p) { *p = 1; }"),
    ("check_mem", "a load through a pointer",
     "ATTR long f (long *p) { return *p; }"),
    ("check_mem", "arithmetic only", "ATTR long f (long a) { return a * 3; }"),
    ("check_mem", "a load inside an asm",
     'ATTR unsigned long f (unsigned long *p) { unsigned long v; '
     '__asm__ __volatile__ ("ldd %1, 0x0, %0" : "=r" (v) : "r" (p)); '
     'return v; }'),
    ("check_stack", "a global read",
     "extern long gvar; ATTR long f (void) { return gvar; }"),
    ("check_stack", "loads through a pointer",
     "ATTR long f (long *p) { return *p + p[1]; }"),
    ("check_stack", "a store through a pointer",
     "ATTR void f (long *p, long v) { p[3] = v; }"),
    ("check_stack", "a local array passed out",
     "extern void g (long *); ATTR void f (void) { long x[2]; x[0] = 1; "
     "x[1] = 2; g (x); }"),
    ("check_stack", "two calls, a value live across",
     "extern void g (long); ATTR void f (long a) { long b = a * 3; g (a); "
     "g (b); }"),
    ("check_stack", "a 16-byte struct result",
     "struct s { long a, b; }; extern struct s g (void); ATTR long f (void) "
     "{ struct s r = g (); return r.a + r.b; }"),
]


def sec_attrs2(cc, args):
    if cc.kind != "lcc":
        return
    base = kernel_sets(cc)["kernel"]
    for attr, name, src in ATTRS2_CASES:
        row = []
        for olev in ("-O0", "-O3"):
            ok, asm, diag = cc.compile(
                src.replace("ATTR", "__attribute__ ((%s))" % attr),
                base.replace("-O3", olev))
            row.append("%s %s" % (olev, ("getsp %d" % len(
                re.findall(r"\bgetsp", body(asm, "f")))) if ok else
                "ERROR " + short_diag(diag, 8)))
        print("  %s, %s: %s" % (attr, name, "; ".join(row)))
    # The caller of a check_stack function is unchanged.
    ok, asm, diag = cc.compile(
        "__attribute__ ((check_stack)) long cs (long, long); "
        "long f (long a) { return cs (a, 2) + 1; }", base)
    print("  caller of a check_stack function: getsp %d" % len(
        re.findall(r"\bgetsp", body(asm, "f"))))


# --------------------------------------------------------------------------
# jumptab: whether -fno-jump-tables is honoured (arch/e2k/p2v, kexec and
# boot code run at physical addresses and pass it).

JUMPTAB_SRC = r"""
extern int f0 (int), f1 (int), f2 (int), f3 (int), f4 (int), f5 (int),
  f6 (int), f7 (int), f8 (int);
int sw (int x) { switch (x) { case 0: return f0 (x); case 1: return f1 (x + 1);
  case 2: return f2 (x * 3); case 3: return f3 (x - 7); case 4: return f4 (x ^ 5);
  case 5: return f5 (x | 9); case 6: return f6 (x & 3); case 7: return f7 (x << 2);
  case 8: return f8 (x >> 1); default: return 0; } }
"""


def sec_jumptab(cc, args):
    levels = ("-O0", "-O1", "-O2", "-O3") if cc.kind == "lcc" else \
        ("-O0", "-O2")
    row = []
    for lev in levels:
        for jt in ("", " -fno-jump-tables"):
            ok, asm, diag = cc.compile(JUMPTAB_SRC, lev + jt)
            if not ok:
                row.append("%s%s ERROR" % (lev, jt))
                continue
            t = body(asm, "sw")
            table = re.search(r"\.T\.\d", t) or re.search(
                r"movtd|\.L\w+\s*\]", t)
            row.append("%s%s %s" % (lev, jt, "table" if table else "branches"))
    print("  " + "; ".join(row))
    if cc.kind == "lcc":
        p = kernel_sets(cc)["kernel physical mode (p2v, kexec)"]
        ok, asm, diag = cc.compile(JUMPTAB_SRC, p)
        print("  kernel physical-mode options: %s" % (
            "table (absolute .dword entries)" if re.search(r"\.T\.\d", asm)
            else "branches"))


# --------------------------------------------------------------------------
# blockmove: which aggregate moves become memcpy/memset calls (arch/e2k/p2v
# redirects only explicit calls to boot_memcpy/boot_memset).

BLOCKMOVE_SRC = r"""
struct huge { long a[512]; };
struct mid { long a[16]; };
extern void use (void *);
void zero_local (void) { struct huge h = { 0 }; use (&h); }
void zero_ptr (struct huge *d) { *d = (struct huge) { 0 }; }
void zero_mid (struct mid *d) { *d = (struct mid) { 0 }; }
void copy_huge (struct huge *d, const struct huge *s) { *d = *s; }
void bzero_n (char *d, unsigned long n) { __builtin_memset (d, 0, n); }
void bcopy_n (char *d, const char *s, unsigned long n) { __builtin_memcpy (d, s, n); }
void loop_zero (long *d, long n) { for (long i = 0; i < n; i++) d[i] = 0; }
void loop_copy (long *d, const long *s, long n) { for (long i = 0; i < n; i++) d[i] = s[i]; }
"""

BLOCKMOVE_FNS = ["zero_local", "zero_ptr", "zero_mid", "copy_huge", "bzero_n",
                 "bcopy_n", "loop_zero", "loop_copy"]


def sec_blockmove(cc, args):
    if cc.kind == "lcc":
        k = kernel_sets(cc)["kernel"]
        sets = {"-O3": "-O3", "kernel": k,
                "kernel -fno-use-memset": k + " -fno-use-memset",
                "kernel physical mode": kernel_sets(cc)[
                    "kernel physical mode (p2v, kexec)"]}
    else:
        sets = {"kernel as GCC gets it": KERNEL_GCC,
                "kernel -ffreestanding": KERNEL_GCC + " -ffreestanding",
                "kernel -Os": KERNEL_GCC.replace("-O2", "-Os")}
    for name, opts in sets.items():
        ok, asm, diag = cc.compile(BLOCKMOVE_SRC, opts)
        if not ok:
            print("  %s: ERROR %s" % (name, short_diag(diag)))
            continue
        res = []
        for fn in BLOCKMOVE_FNS:
            calls = sorted({m.group(1) for m in re.finditer(
                r"\bdisp\s+%ctpr\d,\s*([A-Za-z_][\w.]*)", body(asm, fn))
                if m.group(1) not in ("use",)})
            res.append("%s %s" % (fn, ",".join(calls) or "-"))
        print("  %s: %s" % (name, "; ".join(res)))


# --------------------------------------------------------------------------
# pragmas: lcc's asm pragmas (asm/compiler.h:43-67) under the kernel's
# -Wall -Werror; asmops: the largest asm the kernel writes
# (TRAP_HANDLER_DONE, asm/e2k_api.h:7372: 16 outputs, 23 inputs).

PRAGMA_CASES = [
    ("asm_inline", '_Pragma ("asm_inline")'),
    ("no_asm_inline", '_Pragma ("no_asm_inline")'),
    ("asm_length (3)", '_Pragma ("asm_length(3)")'),
]


def sec_pragmas(cc, args):
    row = []
    for name, prag in PRAGMA_CASES:
        ok, asm, diag = cc.compile(
            "void f (void) { %s __asm__ __volatile__ (\"nop\"); }" % prag,
            "-O2 -Wall -Werror")
        row.append("%s %s" % (name, "accepted" if ok else
                              "ERROR " + short_diag(diag, 8)))
    print("  -Wall -Werror: " + "; ".join(row))


def sec_asmops(cc, args):
    for nout, nin in ((16, 14), (16, 23), (0, 50), (0, 51)):
        outs = ", ".join('"=&r" (o%d)' % i for i in range(nout))
        ins = ", ".join('"r" (p[%d])' % i for i in range(nin))
        decl = ("long %s;" % ", ".join("o%d" % i for i in range(nout))
                if nout else "")
        src = ("void f (long *p, long *q) { %s __asm__ __volatile__ "
               "(\"nop\" : %s : %s); %s }" % (
                   decl, outs, ins,
                   " ".join("q[%d] = o%d;" % (i, i) for i in range(nout))))
        ok, asm, diag = cc.compile(src, "-O2")
        print("  %d outputs + %d inputs = %d operands: %s" % (
            nout, nin, nout + nin, "accepted" if ok else
            "ERROR " + short_diag(diag, 8)))


# --------------------------------------------------------------------------
# --compare-wsz: the largest `setwd wsz` of each function in two builds of
# one object (lcc's and another compiler's), as the kernel's Makefile reads
# it back with objdump.

def function_wsz(objdump, obj):
    dump = subprocess.run([objdump, "-me2k", "-d", obj], capture_output=True,
                          text=True).stdout
    res, cur = {}, None
    for line in dump.splitlines():
        m = re.match(r"^[0-9a-f]+ <([^>]+)>:", line)
        if m:
            cur = m.group(1)
            continue
        m = re.search(r"setwd wsz = (0x[0-9a-f]+)", line)
        if m and cur:
            res[cur] = max(res.get(cur, 0), int(m.group(1), 16))
    return res


def compare_wsz(args, a, b):
    objdump = os.path.join(args.binutils, "e2k-linux-objdump")
    wa, wb = function_wsz(objdump, a), function_wsz(objdump, b)
    common = sorted(set(wa) & set(wb))
    more = [f for f in common if wb[f] > wa[f]]
    less = [f for f in common if wb[f] < wa[f]]
    print("  %s: %d functions in both; the second has a larger window in %d, "
          "a smaller one in %d; sum of wsz %d vs %d; largest %s %d vs %s %d"
          % (os.path.basename(a), len(common), len(more), len(less),
             sum(wa[f] for f in common), sum(wb[f] for f in common),
             max(common, key=lambda f: wa[f]) if common else "-",
             max(wa[f] for f in common) if common else 0,
             max(common, key=lambda f: wb[f]) if common else "-",
             max(wb[f] for f in common) if common else 0))
    for f in sorted(more, key=lambda f: wb[f] - wa[f], reverse=True)[:5]:
        print("    %s: 0x%x vs 0x%x" % (f, wa[f], wb[f]))


SECTIONS = {
    "options": sec_options, "gregs": sec_gregs, "gvars": sec_gvars,
    "barrier": sec_barrier, "attrs": sec_attrs, "open": sec_open,
    "modifiers": sec_modifiers, "codegen": sec_codegen,
    "bug123567": sec_bug123567, "int128": sec_int128, "relocs": sec_relocs,
    "pg": sec_pg, "wsz": sec_wsz, "gregapi": sec_gregapi,
    "attrs2": sec_attrs2, "jumptab": sec_jumptab, "blockmove": sec_blockmove,
    "pragmas": sec_pragmas, "asmops": sec_asmops,
}


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--lcc", action="append")
    ap.add_argument("--gcc", action="append", default=[])
    ap.add_argument("--section", action="append")
    ap.add_argument("--binutils", default=DEFAULT_BINUTILS)
    ap.add_argument("--work", default="toolchains/kernelbuild")
    ap.add_argument("--compare-wsz", nargs=2, action="append",
                    metavar=("LCC_OBJ", "OTHER_OBJ"),
                    help="compare the windows of two builds of one object")
    args = ap.parse_args()
    args.binutils = os.path.abspath(args.binutils)
    if args.compare_wsz:
        print("[compare-wsz]")
        for a, b in args.compare_wsz:
            compare_wsz(args, a, b)
        return
    os.makedirs(args.work, exist_ok=True)
    work = os.path.abspath(args.work)
    ccs = [Compiler([os.path.abspath(l) if os.sep in l else l], "lcc", work)
           for l in (args.lcc or ([] if args.gcc else [DEFAULT_LCC]))]
    for g in args.gcc:
        argv = g.split()
        argv[0] = os.path.abspath(argv[0]) if os.sep in argv[0] else argv[0]
        argv = [re.sub(r"^-B(.*)", lambda m: "-B" + os.path.abspath(m.group(1))
                       + "/", a) for a in argv]
        ccs.append(Compiler(argv, "gcc", work))
    for name in args.section or list(SECTIONS):
        if name not in SECTIONS:
            sys.exit("unknown section %s" % name)
        for cc in ccs:
            print("[%s] %s" % (name, cc.label))
            SECTIONS[name](cc, args)
            sys.stdout.flush()


if __name__ == "__main__":
    main()
