#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Write pf.s for pf.c: software prefetch (`ldb,sm addr, %empty` with a
MAS, as lcc lowers __builtin_prefetch) in three loops, and the prefetch
loop used to see which cache level each MAS fills (spec/isa/timing.md,
"Memory").  E2S, E8C2 and E2C3 have no hardware data prefetcher (the
kernel's cpu_features.h gives L1 and TLB prefetchers to v7 only).

  python3 tools/isa-probes/gen_pf.py

Variants per loop: no prefetch ("none") and a prefetch with MAS 0x0,
0x20, 0x40 or 0x60.  Every loop returns the %clkr cycles it took.

  long pg_V (long n, u64 *a, u64 x, u64 y, u64 *sum)
      random gather: n times, x *= A, s += a[x >> 39]; the prefetch is
      for a[y >> 39] where y runs the same generator D steps ahead (the
      caller starts y at x * A^D).
  long pc_V (long n, u64 *p, u64 *sum)
      pointer chase: n times, p = p[0]; the prefetch is for p[1], a jump
      pointer the caller set to the node D steps ahead.
  long ps_V (long n, u64 *a, long dist, u64 *sum)
      stream: n 32-byte lines, four ldd each; the prefetch is for
      a + dist (dist bytes ahead).
  void pre_V (u64 **addr, long k)
      prefetch addr[0..k-1] (V "ld" touches them with an ordinary ldd).
  void pf_touch (u64 *p, long nlines)
      ordinary ldd of every 32-byte line from p (to evict caches: a C loop
      would get lcc's array prefetch or cache-bypass hints).
"""

from pathlib import Path

HERE = Path(__file__).resolve().parent
A = 0x5851F42D4C957F2D          # an odd 64-bit multiplier
VARIANTS = [("none", None), ("m00", 0x0), ("m20", 0x20), ("m40", 0x40),
            ("m60", 0x60)]


def w(*o):
    return "\t{\n" + "".join(f"\t  {x}\n" for x in o) + "\t}\n"


def pf(ch, addr, off, mas):
    m = f", mas={mas:#x}" if mas else ""
    return f"ldb,{ch},sm\t{addr}, {off}, %empty{m}"


def func(name, body):
    return (f"\n\t.global\t{name}\n\t.type\t{name}, #function\n"
            f"\t.align\t8\n{name}:\n{body}\t.size\t{name}, .-{name}\n")


def head(lbl):
    # return and disp both need CS0: two instructions.
    return (w("setwd\twsz = 0x10, nfx = 0x1, dbl = 0x0", "return\t%ctpr3")
            + w("addd,0\t0x0, 0x0, %dr20", "addd,1\t0x0, 0x0, %dr21",
                "addd,3\t0x0, 0x0, %dr22", "addd,4\t0x0, 0x0, %dr23",
                f"disp\t%ctpr1, {lbl}",
                f"addd,2\t0x0, _f64,_lts0 {A:#x}, %dr9")
            + w("nop 7") + w("rrd,0\t%clkr, %dr10"))


def tail(sumreg):
    return (w("rrd,0\t%clkr, %dr11")
            + w("addd,0\t%dr20, %dr21, %dr20", "addd,1\t%dr22, %dr23, %dr22")
            + w("addd,0\t%dr20, %dr22, %dr20",
                "subd,1\t%dr11, %dr10, %dr0")
            + w(f"std,2\t{sumreg}, 0x0, %dr20") + w("ct\t%ctpr3"))


def gather(v, mas):
    lbl = f".Lpg_{v}"
    b = head(lbl)
    pre = [pf(3, "%dr13", "0x0", mas)] if v != "none" else []
    b += f"{lbl}:\n"
    b += w("muld,0\t%dr2, %dr9, %dr2", "muld,3\t%dr3, %dr9, %dr3",
           "subd,1\t%dr0, 0x1, %dr0")
    b += w("shrd,0\t%dr2, 0x27, %dr12", "shrd,3\t%dr3, 0x27, %dr13",
           "cmpedb,1\t%dr0, 0x0, %pred0")
    b += w("shld,0\t%dr12, 0x3, %dr12", "shld,3\t%dr13, 0x3, %dr13")
    b += w("addd,0\t%dr1, %dr12, %dr12", "addd,3\t%dr1, %dr13, %dr13")
    b += w("ldd,0\t%dr12, 0x0, %dr14", *pre)
    b += w("addd,1\t%dr20, %dr14, %dr20", "ct\t%ctpr1 ? ~%pred0")
    return func(f"pg_{v}", b + tail("%dr4"))


def chase(v, mas):
    lbl = f".Lpc_{v}"
    b = head(lbl)
    b += f"{lbl}:\n"
    b += w("ldd,0\t%dr1, 0x0, %dr1", "ldd,2\t%dr1, 0x8, %dr5",
           "subd,1\t%dr0, 0x1, %dr0")
    b += w("cmpedb,1\t%dr0, 0x0, %pred0",
           *([pf(3, "%dr5", "0x0", mas)] if v != "none" else []))
    b += w("addd,0\t%dr20, 0x1, %dr20", "ct\t%ctpr1 ? ~%pred0")
    return func(f"pc_{v}", b + tail("%dr2"))


def stream(v, mas):
    lbl = f".Lps_{v}"
    b = head(lbl)
    b += f"{lbl}:\n"
    b += w("ldd,0\t%dr1, 0x0, %dr14", "ldd,2\t%dr1, 0x8, %dr15",
           "ldd,3\t%dr1, 0x10, %dr16", "ldd,5\t%dr1, 0x18, %dr17",
           "subd,1\t%dr0, 0x1, %dr0")
    b += w("cmpedb,1\t%dr0, 0x0, %pred0", "addd,4\t%dr1, 0x20, %dr1",
           *([pf(0, "%dr1", "%dr2", mas)] if v != "none" else []))
    b += w("addd,0\t%dr20, %dr14, %dr20", "addd,1\t%dr21, %dr15, %dr21",
           "addd,3\t%dr22, %dr16, %dr22", "addd,4\t%dr23, %dr17, %dr23",
           "ct\t%ctpr1 ? ~%pred0")
    return func(f"ps_{v}", b + tail("%dr3"))


def pre(v, mas):
    lbl = f".Lpre_{v}"
    b = w("setwd\twsz = 0x8, nfx = 0x1, dbl = 0x0", "return\t%ctpr3")
    b += w(f"disp\t%ctpr1, {lbl}")
    b += f"{lbl}:\n"
    b += w("ldd,0\t%dr0, 0x0, %dr2", "addd,1\t%dr0, 0x8, %dr0",
           "subd,3\t%dr1, 0x1, %dr1")
    b += w("cmpedb,1\t%dr1, 0x0, %pred0")
    b += w("ldd,0\t%dr2, 0x0, %dr3" if v == "ld" else pf(0, "%dr2", "0x0",
                                                         mas))
    b += w("ct\t%ctpr1 ? ~%pred0")
    b += w("nop 7") + w("ct\t%ctpr3")
    return func(f"pre_{v}", b)


def main():
    out = ("! SPDX-License-Identifier: GPL-3.0-or-later\n"
           "! Generated by tools/isa-probes/gen_pf.py; see pf.c.\n"
           "\t.ignore\tld_st_style\n\t.text\n")
    for v, m in VARIANTS:
        out += gather(v, m) + chase(v, m) + stream(v, m)
        if v != "none":
            out += pre(v, m)
    out += pre("ld", None)
    b = w("setwd\twsz = 0x8, nfx = 0x1, dbl = 0x0", "return\t%ctpr3")
    b += w("disp\t%ctpr1, .Lpf_touch")
    b += ".Lpf_touch:\n"
    b += w("ldd,0\t%dr0, 0x0, %dr2", "ldd,2\t%dr0, 0x20, %dr3",
           "addd,1\t%dr0, 0x40, %dr0", "subd,3\t%dr1, 0x2, %dr1")
    b += w("cmpledb,1\t%dr1, 0x0, %pred0")
    b += w("addd,0\t%dr2, %dr3, %dr4", "ct\t%ctpr1 ? ~%pred0")
    b += w("ct\t%ctpr3")
    out += func("pf_touch", b)
    out += func("pf_clkr", w("setwd\twsz = 0x4, nfx = 0x1, dbl = 0x0",
                             "return\t%ctpr3", "rrd,0\t%clkr, %dr0")
                + w("ct\t%ctpr3"))
    (HERE / "pf.s").write_text(out)


if __name__ == "__main__":
    main()
