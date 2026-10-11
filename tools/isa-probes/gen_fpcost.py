#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Write fpcost.s and fpcost-cases.h for fpcost.c: what denormal
operands and results cost, with and without flush to zero (fz) and
denormals are zero (daz) in %pfpfr (spec/isa/timing.md).

  python3 tools/isa-probes/gen_fpcost.py

Each kernel is  long fc_N(long iters, double v[4], u64 ctl): it writes
ctl to %pfpfr, loads x = v[0], a = v[1], b = v[2], runs ITERS
iterations of 8 instructions and returns the %clkr cycles they took,
storing the final x in v[0] (and %pfpfr in v[3]).  In a "chain" kernel
the 8 operations form one dependent chain, x = op(x, a), x = op(x, b),
...; in a "4 chains" kernel instruction i works on chain i % 4, so each
chain has an operation every 4 instructions (the FP latency), which
shows whether a slow case blocks the pipeline or only its own chain.
A "distance D" kernel is one chain whose instructions each end with
nop D-1, so it runs at D cycles an operation exactly when the latency
is at most D (the smallest such D is the scheduled latency; timing.md,
"Stalls cost more than the latency").
"""

from pathlib import Path

HERE = Path(__file__).resolve().parent

# name, operation, x, a, b, chains, single?
DEN = 0x0008000000000001        # a denormal double
SDEN = 0x00400001               # a denormal single
CASES = [
    ("fmuld normal x*1", "fmuld", 0x3ff8000000000000, 0x3ff0000000000000,
     0x3ff0000000000000, 1),
    ("fmuld denormal x*1 (in and out)", "fmuld", DEN, 0x3ff0000000000000,
     0x3ff0000000000000, 1),
    ("fmuld normal->denormal->normal", "fmuld", 0x1ed0000000000000,
     0x1ed0000000000000, 0x6120000000000000, 1),
    ("fmuld x*denormal->0 (underflow)", "fmuld", 0x3ff0000000000000,
     DEN, 0x3ff0000000000000, 1),
    ("faddd normal x+0", "faddd", 0x3ff8000000000000, 0, 0, 1),
    ("faddd denormal x+0", "faddd", DEN, 0, 0, 1),
    ("faddd normal x+denormal", "faddd", 0x3ff0000000000000, DEN, DEN, 1),
    ("fdivd normal x/1", "fdivd", 0x3ff8000000000000, 0x3ff0000000000000,
     0x3ff0000000000000, 1),
    ("fdivd denormal x/1", "fdivd", DEN, 0x3ff0000000000000,
     0x3ff0000000000000, 1),
    ("fmuls normal x*1", "fmuls", 0x3fc00000, 0x3f800000, 0x3f800000, 1),
    ("fmuls denormal x*1", "fmuls", SDEN, 0x3f800000, 0x3f800000, 1),
    ("fadds denormal x+0", "fadds", SDEN, 0, 0, 1),
    ("pfmuls denormal x*1 (both lanes)", "pfmuls", SDEN | SDEN << 32,
     0x3f8000003f800000, 0x3f8000003f800000, 1),
    ("fmuld normal x*1, 4 chains", "fmuld", 0x3ff8000000000000,
     0x3ff0000000000000, 0x3ff0000000000000, 4),
    ("fmuld denormal x*1, 4 chains", "fmuld", DEN, 0x3ff0000000000000,
     0x3ff0000000000000, 4),
    ("faddd denormal x+0, 4 chains", "faddd", DEN, 0, 0, 4),
]
LAT = [
    ("fmuld normal x*1", "fmuld", 0x3ff8000000000000, 0x3ff0000000000000,
     0x3ff0000000000000),
    ("fmuld denormal x*1", "fmuld", DEN, 0x3ff0000000000000,
     0x3ff0000000000000),
    ("fmuld normal*normal->denormal, *2^530", "fmuld", 0x1ed0000000000000,
     0x1ed0000000000000, 0x6110000000000000),
    ("faddd denormal x+0", "faddd", DEN, 0, 0),
    ("faddd normal x+denormal", "faddd", 0x3ff0000000000000, DEN, DEN),
    ("fmuls denormal x*1", "fmuls", SDEN, 0x3f800000, 0x3f800000),
    ("pfmuls denormal x*1", "pfmuls", SDEN | SDEN << 32,
     0x3f8000003f800000, 0x3f8000003f800000),
    # only the operand denormal: 2^600 * DEN = 2^-423, * 2^1023 = 2^600
    ("fmuld normal*denormal->normal, *2^1023", "fmuld", 0x6570000000000000,
     DEN, 0x7fe0000000000000),
    # only the result denormal: 2^-530 * 2^-530 = 2^-1060, then an add
    # (a denormal add costs nothing) brings back 2^-530
    ("fmuld normal*normal->denormal, faddd", ("fmuld", "faddd"),
     0x1ed0000000000000, 0x1ed0000000000000, 0x1ed0000000000000),
    # the same chain on normal values: x * 1, x + 0
    ("fmuld x*1, faddd x+0, normal", ("fmuld", "faddd"),
     0x3ff8000000000000, 0x3ff0000000000000, 0),
]
for n, o, x, a, b in LAT:
    for d in (4, 5, 6, 7, 8):
        CASES.append((f"{n}, distance {d}", o, x, a, b, 1, d))
CHAN = [0, 1, 3, 4]


def reg(op, n):
    return f"%r{n}" if op in ("fmuls", "fadds") else f"%dr{n}"


def opk(op, k):
    """The operation of instruction K (OP may alternate two)."""
    return op[k % 2] if isinstance(op, tuple) else op


def fn(i, op, chains, dist=None):
    w = lambda *o: "\t{\n" + "".join(f"\t  {x}\n" for x in o) + "\t}\n"
    ch = (lambda k: 5) if op == "fdivd" else (lambda k: CHAN[k % chains])
    base = op
    b = w("setwd\twsz = 0x10, nfx = 0x1, dbl = 0x0", "return\t%ctpr3")
    b += w("rrs,0\t%pfpfr, %r8")
    b += w("ldd,0\t%dr1, 0x0, %dr3", "ldd,2\t%dr1, 0x8, %dr4",
           "ldd,3\t%dr1, 0x10, %dr5")
    b += w("rws,0\t%r2, %pfpfr", "disp\t%ctpr1, .Lfc_" + str(i))
    b += w("nop 7")
    # four chains start from the same x
    b += w("addd,0\t0x0, %dr3, %dr20", "addd,1\t0x0, %dr3, %dr21",
           "addd,3\t0x0, %dr3, %dr22", "addd,4\t0x0, %dr3, %dr23")
    b += w("nop 7")
    b += w("rrd,0\t%clkr, %dr10")
    body = []
    for k in range(8):
        x = 20 + (k % chains)
        src = 4 + (k % 2)
        op = opk(base, k)
        ops = [f"{op},{ch(k)}\t{reg(op, x)}, {reg(op, src)}, {reg(op, x)}"]
        if k == 0:
            ops.append("subd,2\t%dr0, 0x1, %dr0")
        if k == 1:
            ops.append("cmpedb,3\t%dr0, 0x0, %pred0")
        if k == 7:
            ops.append("ct\t%ctpr1 ? ~%pred0")
        if dist and dist > 1:
            ops.append(f"nop {dist - 1}")
        body.append(w(*ops))
    b += f".Lfc_{i}:\n" + "".join(body)
    b += w("rrd,0\t%clkr, %dr11")
    b += w("nop 7")
    b += w("subd,1\t%dr11, %dr10, %dr0", "rrs,0\t%pfpfr, %r9")
    b += w("std,2\t%dr1, 0x0, %dr20", "stw,5\t%dr1, 0x18, %r9")
    b += w("rws,0\t%r8, %pfpfr") + w("nop 5") + w("ct\t%ctpr3")
    return (f"\n\t.global\tfc_{i}\n\t.type\tfc_{i}, #function\n"
            f"\t.align\t8\nfc_{i}:\n{b}\t.size\tfc_{i}, .-fc_{i}\n")


def main():
    (HERE / "fpcost.s").write_text(
        "! SPDX-License-Identifier: GPL-3.0-or-later\n"
        "! Generated by tools/isa-probes/gen_fpcost.py; see fpcost.c.\n"
        "\t.ignore\tld_st_style\n\t.text\n"
        + "".join(fn(i, c[1], c[5], c[6] if len(c) > 6 else None)
                  for i, c in enumerate(CASES)))
    (HERE / "fpcost-cases.h").write_text(
        "/* SPDX-License-Identifier: GPL-3.0-or-later */\n"
        "/* Generated by tools/isa-probes/gen_fpcost.py.  */\n"
        + "".join(f'CASE ({i}, "{c[0]}", {c[2]:#x}UL, {c[3]:#x}UL, '
                  f'{c[4]:#x}UL)\n' for i, c in enumerate(CASES)))
    print(len(CASES), "kernels")


if __name__ == "__main__":
    main()
