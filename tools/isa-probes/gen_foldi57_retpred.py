#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Write foldi57_retpred.s and foldi57_retpred-cases.h for
foldi57_retpred.c: which value a predicate reader sees when its
predicate's compare is shortly before it and a control transfer shortly
after it (the proof-of-concept back end's finding I57; spec/isa/timing.md,
"Hazards"; predicates.md, "Timing").

  python3 tools/isa-probes/gen_foldi57_retpred.py

Each case is a callee in_<name>, called through out1 or out0 (cell,
in_<name>): the caller sets its own %pred0 to 1 or 0, passes a pointer
to a cell holding 7 and returns the callee's result.  The callee sets
%pred0 to the old value o, waits, and then runs

  producer   writes %pred0 = n = !o            (instruction P)
  reader     reads %pred0                      (P + dcr cycles)
  transfer                                     (reader + drt cycles)

The reader writes 9 when the predicate it sees is true, else the result
stays 7: merges %r4 (7), %r5 (9) (merge), addd 9 under ? %pred0 in ALC0
(add0) or ALC3 (add3), or std 9 to the cell under ? %pred0 (std; the
driver reads the cell).  A distance of 2 or more is the earlier
instruction's nop count + 1; drt 0 puts the reader in the transfer's own
instruction.

Producers: i cmpesb, f fcmplesb, d fcmpledb, x fxcmplesb (an extended
operand), l predicate logic (pass, landp, pass of two predicates set long
before; I38), fl predicate logic whose input an fcmplesb wrote in the
instruction before (P - 1).  All in ALC0.

Transfers: ret (ct %ctpr3 after return %ctpr3), retc (the same ct under
? %pred1, true), br (ct %ctpr1 to a later label, taken), call (a call of
a leaf), none (an instruction without a transfer, then a return 6 cycles
later).  jb: a taken ct %ctpr1 between producer and reader, in the
producer's instruction (dcr 1) or the next one (dcr 2): the reader is
the first instruction at the target and a return follows it (drt 0 or
1).  The full dcr 1-5 x drt 0-2 grid runs for ret; the other transfers
at dcr 1-2 x drt 0-1 (jb: producers i, f, l; readers merge, add0).
"""

import itertools
from pathlib import Path

HERE = Path(__file__).resolve().parent

PRODUCERS = ("i", "f", "d", "x", "l", "fl")
READERS = ("merge", "add0", "add3", "std")
XFERS = ("ret", "retc", "br", "call", "none")


def wi(*ops):
    return "\t{\n" + "".join(f"\t  {o}\n" for o in ops) + "\t}\n"


def fn(name, body):
    return (f"\n\t.global\t{name}\n\t.type\t{name}, #function\n"
            f"\t.align\t8\n{name}:\n{body}\t.size\t{name}, .-{name}\n")


def nops(ops, n):
    """The instruction ops followed by n empty cycles."""
    return list(ops) + ([f"nop {n}"] if n else [])


def producer(kind, n):
    """Instructions that end with %pred0 = n (one, or two for fl)."""
    if kind == "i":
        # %r6 = 0, %r7 = 0x3f800000.
        return [[f"cmpesb,0\t{'%r6' if n else '%r7'}, 0x0, %pred0"]]
    if kind == "f":
        # 0.0f <= 1.0f is true.
        a, b = ("%r6", "%r7") if n else ("%r7", "%r6")
        return [[f"fcmplesb,0\t{a}, {b}, %pred0"]]
    if kind == "d":
        a, b = ("%dr10", "%dr11") if n else ("%dr11", "%dr10")
        return [[f"fcmpledb,0\t{a}, {b}, %pred0"]]
    if kind == "x":
        # %dr12 = 0.0 and %dr13 = 1.0 as extended values.
        a, b = ("%dr12", "%r7") if n else ("%dr13", "%r6")
        return [[f"fxcmplesb,0\t{a}, {b}, %pred0"]]
    logic = ["pass\t%pred2, @p0", "pass\t%pred3, @p1",
             "landp\t@p0, @p1, @p4", "pass\t@p4, %pred0"]
    if kind == "l":
        # %pred2 = n and %pred3 = 1 were set long before.
        return [logic]
    # fl: %pred2 was set to !n long before; the compare makes it n.
    a, b = ("%r6", "%r7") if n else ("%r7", "%r6")
    return [[f"fcmplesb,0\t{a}, {b}, %pred2"], logic]


def reader(kind):
    return {"merge": "merges,0\t%r4, %r5, %r0, %pred0",
            "add0": "addd,0\t0x0, 0x9, %dr0 ? %pred0",
            "add3": "addd,3\t0x0, 0x9, %dr0 ? %pred0",
            "std": "std,2\t%dr8, 0x0, %dr5 ? %pred0"}[kind]


def callee(name, prod, rdr, xfer, dcr, drt, n):
    o = 1 - n
    b = wi("setwd\twsz = 0x8, nfx = 0x1, dbl = 0x0", "return\t%ctpr3",
           "addd,0\t0x0, 0x7, %dr4", "addd,1\t0x0, 0x9, %dr5",
           "addd,3\t0x0, 0x0, %dr6", "addd,4\t0x0, 0x3f800000, %dr7",
           "addd,2\t%dr0, 0x0, %dr8", "addd,5\t0x0, 0x0, %dr10")
    p2 = (1 - n) if prod == "fl" else n
    b += wi(f"cmpesb,0\t0x0, {0 if p2 else 1:#x}, %pred2",
            f"cmpesb,1\t0x0, {0 if o else 1:#x}, %pred0",
            "cmpesb,3\t0x0, 0x0, %pred1", "cmpesb,4\t0x0, 0x0, %pred3",
            "addd,2\t0x0, 0x7, %dr0",
            "addd,5\t0x0, 0x3ff0000000000000, %dr11",
            *([f"disp\t%ctpr1, {'leaf' if xfer == 'call' else '.L' + name + '_t'}"]
              if xfer in ("call", "br", "jb") else []))
    b += wi("fstofx,0\t%r6, %dr12", "fstofx,1\t%r7, %dr13", "nop 7")
    b += wi("nop 7")
    seq = producer(prod, n)
    for ops in seq[:-1]:
        b += wi(*ops)               # fl: the compare right before the logic
    if xfer == "jb":
        # A taken branch between: the reader starts the target block.
        if dcr == 1:
            b += wi(*seq[-1], "ct\t%ctpr1")
        else:
            b += wi(*seq[-1]) + wi("ct\t%ctpr1")
        b += wi("addd,0\t0x0, 0x63, %dr0") * 3
        b += f".L{name}_t:\n"
        if drt == 0:
            return fn("in_" + name, b + wi(reader(rdr), "ct\t%ctpr3"))
        b += wi(*nops([reader(rdr)], drt - 1)) + wi("ct\t%ctpr3")
        return fn("in_" + name, b)
    b += wi(*nops(seq[-1], dcr - 1))
    ct = {"ret": ["ct\t%ctpr3"], "retc": ["ct\t%ctpr3 ? %pred1"],
          "br": ["ct\t%ctpr1"], "call": ["call\t%ctpr1, wbs = 0x4"],
          "none": ["addd,1\t0x0, 0x0, %dr9"]}[xfer]
    if drt == 0:
        b += wi(reader(rdr), *ct)
    else:
        b += wi(*nops([reader(rdr)], drt - 1))
        b += wi(*ct)
    if xfer == "retc":
        b += wi("addd,0\t0x0, 0x63, %dr0") + wi("ct\t%ctpr3")
    elif xfer == "br":
        # Skipped by the taken branch: would spoil the result.
        b += wi("addd,0\t0x0, 0x63, %dr0") * 3
        b += f".L{name}_t:\n" + wi("nop 5") + wi("ct\t%ctpr3")
    elif xfer == "call":
        # The call empties the CTPRs: prepare the return again.
        b += wi("return\t%ctpr3") + wi("nop 5") + wi("ct\t%ctpr3")
    elif xfer == "none":
        b += wi("nop 5") + wi("ct\t%ctpr3")
    return fn("in_" + name, b)


def caller(c):
    """out<c> (cell, f): %pred0 = c, then f (cell) through %ctpr1."""
    b = wi("setwd\twsz = 0x9, nfx = 0x1, dbl = 0x0",
           "setbn\trsz = 0x3, rbs = 0x5, rcur = 0x0",
           f"cmpesb,0\t0x0, {0 if c else 1:#x}, %pred0")
    b += wi("movtd,0\t%dr1, %ctpr1", "addd,1\t%dr0, 0x0, %db[0]", "nop 7")
    b += wi("nop 3")
    b += wi("call\t%ctpr1, wbs = 0x5")
    b += wi("return\t%ctpr3", "addd,0\t%db[0], 0x0, %dr0")
    b += wi("nop 7")
    b += wi("ct\t%ctpr3")
    return fn(f"out{c}", b)


def shapes():
    """(producer, reader, transfer, dcr, drt): the full grid for a taken
    return, and the other transfers at the distances that matter."""
    for prod, rdr in itertools.product(PRODUCERS, READERS):
        for dcr, drt in itertools.product((1, 2, 3, 4, 5), (0, 1, 2)):
            yield prod, rdr, "ret", dcr, drt
    for prod, rdr in itertools.product(PRODUCERS, ("merge", "add0", "std")):
        for xfer in XFERS[1:]:
            for dcr, drt in itertools.product((1, 2), (0, 1)):
                yield prod, rdr, xfer, dcr, drt
    for prod, rdr in itertools.product(("i", "f", "l"), ("merge", "add0")):
        for dcr, drt in itertools.product((1, 2), (0, 1)):
            yield prod, rdr, "jb", dcr, drt


def main():
    text = fn("leaf", wi("setwd\twsz = 0x4, nfx = 0x1, dbl = 0x0",
                         "return\t%ctpr3") + wi("nop 5") + wi("ct\t%ctpr3"))
    text += caller(1) + caller(0)
    cases = []
    for prod, rdr, xfer, dcr, drt in shapes():
        for n in (1, 0):
            name = f"{prod}_{rdr}_{xfer}_c{dcr}_r{drt}_n{n}"
            text += callee(name, prod, rdr, xfer, dcr, drt, n)
            cases.append(f'CASE (in_{name}, "{prod}", "{rdr}", '
                         f'"{xfer}", {dcr}, {drt}, {n})\n')
    (HERE / "foldi57_retpred.s").write_text(
        "! SPDX-License-Identifier: GPL-3.0-or-later\n"
        "! Generated by tools/isa-probes/gen_foldi57_retpred.py; see "
        "foldi57_retpred.c.\n"
        "\t.ignore\tld_st_style\n\t.text\n" + text)
    (HERE / "foldi57_retpred-cases.h").write_text(
        "/* SPDX-License-Identifier: GPL-3.0-or-later */\n"
        "/* Generated by tools/isa-probes/gen_foldi57_retpred.py.  */\n"
        + "".join(cases))


if __name__ == "__main__":
    main()
