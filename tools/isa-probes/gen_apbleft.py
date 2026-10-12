#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Write apbleft.s, apbleft-v5.s and their -cases.h files for apbleft.c:
the open items of spec/isa/apb.md's "Not checked yet" (AAU loads with a
MAS, misaligned AAU accesses and their costs, and hardware monitor
counts for the two slowdowns of "Which half an area is in" and "Plain
loads beside an area").

  python3 tools/isa-probes/gen_apbleft.py

Every function is  long al_NAME(long a0, long a1, long a2, long a3);
the case's kind says what the arguments mean (apbleft.c):

  L   load value: (out, base, -, -): an AAU load from base (the AAU
      descriptor's base; the case may add an offset in %aasti1 or the
      literal), out[0] the 64-bit register with its tag cleared, out[1]
      its tag, out[2..3] a quad's second register and its tag, out[4..5]
      the raw 16 bytes of a v5 quad register; the destination register
      is preset to 0xa5a5... first
  S   store value: (base, v, -, -): two AAU stores of v and v + 1 (a
      quad: the pair v, v + 1) at base and one element on
  T   cost: (base, value, -, -): %clkr cycles of 32 steps (NAME) or
      128 (NAME_L): a step is a wide instruction of the case's accesses
      (loads never read, stores of value; t_* repeat one address, s_*
      move on every step) or a load and its use (t_use_*)
  C   column walk (gen_pocchk3.py's p3_col_*): (a, b, lsr, stride,
      cycles *): one or two areas, a loop_mode loop of one instruction
  B   a[i] * b[...] at II 4 (gen_pocchk3.py's p3_bk_*): (a, blo, lsr,
      cycles *, span)
"""

from pathlib import Path

HERE = Path(__file__).resolve().parent
groups = {"": [], "-v5": []}
PRESET = "_f64,_lts0 0xa5a5a5a5a5a5a5a5"


def wi(*ops):
    return "\t{\n" + "".join(f"\t  {o}\n" for o in ops if o) + "\t}\n"


def nops(n):
    out = ""
    while n > 0:
        k = min(n, 8)
        out += wi(f"nop {k - 1}" if k > 1 else "nop")
        n -= k
    return out


def m(mas):
    return f", mas={mas:#x}" if mas else ""


def func(group, name, kind, body, wsz=0x10, extra="", meta=(0, "b")):
    """META: (offset, how) for the C harness: the byte offset the case
    reads at, and how it gets there (b: the caller adds it to the base;
    i: %aasti1; l: the literal)."""
    fn = f"al_{name}"
    groups[group].append(
        (name, kind, meta,
         f"\n\t.global\t{fn}\n\t.type\t{fn}, #function\n\t.align\t8\n{fn}:\n"
         + wi(f"setwd\twsz = {wsz:#x}, nfx = 0x1, dbl = 0x0", "return\t%ctpr3")
         + body + wi("nop 5") + wi("ct\t%ctpr3") + extra
         + f"\t.size\t{fn}, .-{fn}\n"))


def result(quad=None):
    """%dr4's value (tag cleared) and tag to out[0], out[1]; for a pair
    (%qr4) %dr5's to out[2], out[3]; for a %qpr its raw 16 bytes to
    out[4..5] with a speculative store."""
    b = (wi("nop 7") + wi("gettagd,2\t%dr4, %r20", "gettagd,5\t%dr5, %r22")
         + wi("nop 2")
         + wi("puttagd,2\t%dr4, 0x0, %dr21", "puttagd,5\t%dr5, 0x0, %dr23")
         + wi("nop 2")
         + wi("std,2\t%dr0, 0x0, %dr21", "std,5\t%dr0, 0x8, %dr20"))
    if quad == "pair":
        b += wi("std,2\t%dr0, 0x10, %dr23", "std,5\t%dr0, 0x18, %dr22")
    elif quad:
        b += wi(f"stqp,2,sm\t%dr0, 0x20, {quad}")
    return b


# --- 1. AAU loads: values ----------------------------------------------

def aau_load(name, op, mas, sm=False, ch=5, group="", off=0, how="b"):
    """OFF through HOW: b the caller adds it to the base, i in %aasti1,
    l the literal."""
    s = ",sm" if sm else ""
    if op == "ldaaq":
        dst, quad = "%qr4", "pair"
    elif op == "ldaaqp":
        dst, quad = "%qpr4", "%qpr4"
    elif op == "ldaad":
        dst, quad = "%dr4", None
    else:
        dst, quad = "%r4", None
    idx = off if how == "i" else 0
    lit = f" + {off:#x}" if how == "l" and off else ""
    body = (wi(f"addd,0\t0x0, {idx:#x}, %dr24", f"addd,1\t0x0, {PRESET}, %dr4")
            + wi("addd,0\t%dr4, 0x0, %dr5")
            + wi("aaurwd,2\t%dr1, %aad0", "aaurwd,5\t%dr24, %aasti1") + nops(8)
            + wi(f"{op},{ch}{s}\t%aad0[ %aasti1{lit} ], {dst}{m(mas)}")
            + result(quad))
    func(group, name, "L", body, meta=(off, how))


def mname(mas, sm):
    return f"{mas:02x}{'sm' if sm else ''}"


# Narrow AAU loads with each MAS, plain and speculative, both channels.
for op in ("ldaab", "ldaah", "ldaaw"):
    for mas in (0x0, 0x2, 0x3, 0x20, 0x40, 0x60, 0x8, 0x68):
        for ch in (2, 5):
            aau_load(f"{op}_{mname(mas, False)}_c{ch}", op, mas, ch=ch)
# Speculative forms of every width: mod 0, 3, 4, 7, the cache hints, be.
for op in ("ldaab", "ldaah", "ldaaw", "ldaad"):
    for mas in (0x0, 0x3, 0x4, 0x7, 0x20, 0x40, 0x60, 0x8, 0x23, 0x68, 0x24,
                0x27):
        for ch in (2, 5):
            aau_load(f"{op}_{mname(mas, True)}_c{ch}", op, mas, sm=True, ch=ch)
aau_load("ldaad_68_c5", "ldaad", 0x68)
for mas, sm in ((0x8, False), (0x68, False), (0x20, True), (0x60, True),
                (0x8, True), (0x23, True)):
    for ch in (2, 5):
        aau_load(f"ldaaq_{mname(mas, sm)}_c{ch}", "ldaaq", mas, sm=sm, ch=ch)
        aau_load(f"ldaaqp_{mname(mas, sm)}_c{ch}", "ldaaqp", mas, sm=sm, ch=ch,
                 group="-v5")

# Misaligned AAU loads: the offset through the base, %aasti1 or the
# literal; plain and speculative.  apbleft.c also runs them across a
# page boundary into a page the process may not read.
MIS = (("ldaah", (1,)), ("ldaaw", (1, 2)), ("ldaad", (1, 2, 4, 7)),
       ("ldaaq", (1, 4, 8)), ("ldaaqp", (1, 4, 8)))
for op, offs in MIS:
    for off in offs:
        for sm in (False, True):
            grp = "-v5" if op == "ldaaqp" else ""
            ch = 2
            aau_load(f"mis_{op}_o{off}{'sm' if sm else ''}", op, 0, sm=sm,
                     ch=ch, off=off, group=grp)
            if op == "ldaad":
                aau_load(f"mis_{op}_i{off}{'sm' if sm else ''}", op, 0, sm=sm,
                         ch=5, off=off, how="i")
                aau_load(f"mis_{op}_l{off}{'sm' if sm else ''}", op, 0, sm=sm,
                         ch=5, off=off, how="l")


# --- 2. Misaligned AAU stores: values ------------------------------------

def aau_store(name, op, group=""):
    """Two stores, v then v + 1 (a pair or quad: v, v + 1 at once), with
    incr %aaincr0 after the first: element 0 and element 1."""
    if op in ("staaq", "staaqp"):
        n = 1
    else:
        n = 2
    reg = {"staab": "%r", "staah": "%r", "staaw": "%r", "staad": "%dr"}.get(op)
    body = (wi("addd,0\t0x0, 0x0, %dr24", "addd,1\t%dr1, 0x1, %dr21",
               "addd,3\t%dr1, 0x0, %dr20")
            + wi("aaurwd,2\t%dr0, %aad0", "aaurwd,5\t%dr24, %aasti1"))
    if op == "staaqp":
        body += wi("qppackdl,0\t%dr21, %dr20, %qpr26")
    else:
        body += wi("addd,0\t%dr20, 0x0, %dr26", "addd,1\t%dr21, 0x0, %dr27")
    body += nops(6)
    srcs = {"staaq": ["%qr26"], "staaqp": ["%qpr26"]}.get(
        op, [f"{reg}20", f"{reg}21"])
    for k in range(n):
        body += wi(f"{op},5\t{srcs[k]}, %aad0[ %aasti1 ]", "incr,5\t%aaincr0")
    func(group, f"st_{op}", "S", body + nops(4))


for op in ("staah", "staaw", "staad", "staaq"):
    aau_store(op, op)
aau_store("staaqp", "staaqp", group="-v5")


# --- 3. Costs of misaligned accesses -------------------------------------

def timed(name, setup, step, group=""):
    """NAME repeats STEP 32 times, NAME_L 128 times: apbleft.c reports
    both per step and the difference per step, free of fixed costs."""
    for n, suffix in ((32, ""), (128, "_L")):
        body = setup + nops(8) + wi("rrd,0\t%clkr, %dr10")
        for k in range(n):
            body += step(k) if callable(step) else step
        body += (wi("rrd,0\t%clkr, %dr11") + nops(2)
                 + wi("subd,0\t%dr11, %dr10, %dr0"))
        func(group, name + suffix, "T", body, wsz=0x20, meta=(n, "t"))


AAUT = (wi("addd,0\t0x0, 0x0, %dr24", "addd,1\t0x0, 0x0, %dr9",
           "addd,3\t%dr1, 0x0, %dr2")
        + wi("aaurwd,2\t%dr0, %aad0", "aaurwd,5\t%dr24, %aasti1"))
LDT = wi("addd,1\t0x0, 0x0, %dr9", "addd,3\t%dr1, 0x0, %dr2")
# Two loads a wide instruction (ALC2 and ALC5, 8 bytes apart), one, and
# two stores (0x80 apart: no bank conflict), one; AAU and ordinary.
for w, (aop, lop, sop, stop, reg, step) in {
        "d": ("ldaad", "ldd", "staad", "std", "%dr", 8),
        "w": ("ldaaw", "ldw", "staaw", "stw", "%r", 4),
        "h": ("ldaah", "ldh", "staah", "sth", "%r", 2)}.items():
    timed(f"t_{aop}2", AAUT,
          wi(f"{aop},2\t%aad0[ %aasti1 ], {reg}8",
             f"{aop},5\t%aad0[ %aasti1 + {step:#x} ], {reg}12"))
    timed(f"t_{aop}1", AAUT, wi(f"{aop},5\t%aad0[ %aasti1 ], {reg}8"))
    timed(f"t_{lop}2", LDT,
          wi(f"{lop},2\t%dr0, 0x0, {reg}8", f"{lop},5\t%dr0, {step:#x}, {reg}12"))
    timed(f"t_{lop}1", LDT, wi(f"{lop},5\t%dr0, 0x0, {reg}8"))
    timed(f"t_{sop}2", AAUT,
          wi(f"{sop},2\t{reg}2, %aad0[ %aasti1 ]",
             f"{sop},5\t{reg}2, %aad0[ %aasti1 + 0x80 ]"))
    timed(f"t_{sop}1", AAUT, wi(f"{sop},5\t{reg}2, %aad0[ %aasti1 ]"))
    timed(f"t_{stop}2", LDT,
          wi(f"{stop},2\t%dr0, 0x0, {reg}2", f"{stop},5\t%dr0, 0x80, {reg}2"))
    timed(f"t_{stop}1", LDT, wi(f"{stop},5\t%dr0, 0x0, {reg}2"))
# Two stores 8 bytes apart in one instruction, as staaq makes them.
timed("t_std8", LDT, wi("std,2\t%dr0, 0x0, %dr2", "std,5\t%dr0, 0x8, %dr2"))
timed("t_staad8", AAUT, wi("staad,2\t%dr2, %aad0[ %aasti1 ]",
                           "staad,5\t%dr2, %aad0[ %aasti1 + 0x8 ]"))
# Quad AAU accesses: a pair (v3+), one a wide instruction.
timed("t_ldaaq1", AAUT, wi("ldaaq,2\t%aad0[ %aasti1 ], %qr8"))
timed("t_staaq1", AAUT + wi("addd,0\t%dr1, 0x0, %dr26", "addd,1\t%dr1, 0x0, %dr27"),
      wi("staaq,2\t%qr26, %aad0[ %aasti1 ]"))
timed("t_ldaaqp1", AAUT, wi("ldaaqp,2\t%aad0[ %aasti1 ], %qpr8"), group="-v5")
timed("t_staaqp1", AAUT + wi("qppackdl,0\t%dr1, %dr1, %qpr26"),
      wi("staaqp,2\t%qpr26, %aad0[ %aasti1 ]"), group="-v5")


# Streams: each step moves on (8 bytes a step, 16 for two accesses or a
# quad), so no step repeats an address (timing.md: two stores to the
# same places again in every instruction conflict where a stream
# doesn't).  AAU accesses step with incr: %aaincr0 is one element,
# %aaincr2 two; %aasti2 starts one doubleword on.
AAUS2 = AAUT + wi("addd,0\t0x0, 0x2, %dr25", "addd,1\t0x0, 0x8, %dr26",
                  "addd,3\t%dr1, 0x0, %dr27") \
    + wi("aaurwd,2\t%dr25, %aaincr2", "aaurwd,5\t%dr26, %aasti2") \
    + wi("addd,0\t%dr1, 0x0, %dr28", "addd,1\t%dr1, 0x0, %dr29")
timed("s_ldaad1", AAUS2, wi("ldaad,5\t%aad0[ %aasti1 ], %dr8",
                            "incr,5\t%aaincr0"))
timed("s_ldaad2", AAUS2, wi("ldaad,2\t%aad0[ %aasti1 ], %dr8", "incr,2\t%aaincr2",
                            "ldaad,5\t%aad0[ %aasti2 ], %dr12",
                            "incr,5\t%aaincr2"))
timed("s_staad1", AAUS2, wi("staad,5\t%dr2, %aad0[ %aasti1 ]",
                            "incr,5\t%aaincr0"))
timed("s_staad2", AAUS2, wi("staad,2\t%dr2, %aad0[ %aasti1 ]", "incr,2\t%aaincr2",
                            "staad,5\t%dr2, %aad0[ %aasti2 ]",
                            "incr,5\t%aaincr2"))
timed("s_ldaaq1", AAUS2, wi("ldaaq,2\t%aad0[ %aasti1 ], %qr8",
                            "incr,2\t%aaincr0"))
timed("s_staaq1", AAUS2, wi("staaq,2\t%qr28, %aad0[ %aasti1 ]",
                            "incr,2\t%aaincr0"))
timed("s_ldaaqp1", AAUS2, wi("ldaaqp,2\t%aad0[ %aasti1 ], %qpr8",
                             "incr,2\t%aaincr0"), group="-v5")
timed("s_staaqp1", AAUS2 + wi("qppackdl,0\t%dr1, %dr1, %qpr28"),
      wi("staaqp,2\t%qpr28, %aad0[ %aasti1 ]", "incr,2\t%aaincr0"), group="-v5")
# Plain streams: offsets from a fixed base (no address chain).
timed("s_ldd1", LDT, lambda k: wi(f"ldd,5\t%dr0, {8 * k:#x}, %dr8"))
timed("s_ldd2", LDT, lambda k: wi(f"ldd,2\t%dr0, {16 * k:#x}, %dr8",
                                  f"ldd,5\t%dr0, {16 * k + 8:#x}, %dr12"))
timed("s_std1", LDT, lambda k: wi(f"std,5\t%dr0, {8 * k:#x}, %dr2"))
timed("s_std2", LDT, lambda k: wi(f"std,2\t%dr0, {16 * k:#x}, %dr2",
                                  f"std,5\t%dr0, {16 * k + 8:#x}, %dr2"))


# Load to use (32 pairs of { load into %dr8 } { addd %dr8 }), AAU load
# against ldd, both in ALC5 (memleft.c's t_use_*).
def use_pairs(name, load, setup, group=""):
    timed(name, setup, wi(load) + wi("addd,0\t%dr8, %dr9, %dr9"), group)


use_pairs("t_use_ldaad", "ldaad,5\t%aad0[ %aasti1 ], %dr8", AAUT)
use_pairs("t_use_ldd", "ldd,5\t%dr0, 0x0, %dr8", LDT)
use_pairs("t_use_ldaaw", "ldaaw,5\t%aad0[ %aasti1 ], %r8", AAUT)
use_pairs("t_use_ldw", "ldw,5\t%dr0, 0x0, %r8", LDT)


# --- 4. Column walks (gen_pocchk3.py's col, plus a consecutive area) -----

def col(name, layout):
    """LAYOUT: list of (half, asz, kind) per array, the first array a:
    half L (channels 0, 1) or R (2, 3); kind 'c' steps the stride (in
    %aaincr1), '1' one element (%aaincr0)."""
    instr = {"L": [], "R": []}
    used = {"L": 0, "R": 0}
    movas = []
    for j, (half, asz, kind) in enumerate(layout):
        idx = len(instr[half])
        ch = (0 if half == "L" else 2) + (idx % 2)
        movas.append(f"movad,{ch}\tarea={idx}, ind=0, am=1, be=0, %b[{j}]")
        incr = 1 if kind == "c" else 0
        instr[half].append(f"dcd=0, fmt=4, mrng=8, d={j}, incr={incr}, "
                           f"ind=0, asz={asz}, abs={used[half]}, disp=0")
        used[half] += 2 ** asz
    n = max(len(instr["L"]), len(instr["R"]))
    prog = ""
    for i in range(n):
        ops = []
        if i < len(instr["L"]):
            ops.append(f"fapb\tct={int(i == n - 1)}, {instr['L'][i]}")
        if i < len(instr["R"]):
            ops.append(f"fapb\tdpl=0, {instr['R'][i]}")
        prog += wi(*ops)
    adds = [f"addd,{j}\t%dr{9 + j}, %b[{12 + j}], %dr{9 + j}"
            for j in range(len(layout))]
    fn = f"al_col_{name}"
    body = ("".join(wi(*(f"addd,{c}\t0x0, 0x0, %b[{6 * k + c}]"
                         for c in range(6) if 6 * k + c < 16))
                    for k in range(3))
            + wi(f"ldisp\t%ctpr2, .L{fn}_prog", "aaurwd,2\t%dr0, %aad0",
                 "aaurwd,5\t%dr1, %aad1", "addd,0\t0x0, 0x0, %dr9",
                 "addd,1\t0x0, 0x0, %dr10")
            + wi("aaurwd,2\t%dr3, %aaincr1", f"disp\t%ctpr1, .L{fn}_loop",
                 "rwd,0\t%dr2, %lsr")
            + wi("nop 7", "bap") + wi("nop 7") + wi("nop 7", "rrd,0\t%clkr, %dr6")
            + f".L{fn}_loop:\n"
            + wi("loop_mode", "alc\talcf=1, alct=1", "abn\tabnf=1, abnt=1",
                 "ct\t%ctpr1 ? %NOT_LOOP_END", *movas, *adds)
            + wi("rrd,0\t%clkr, %dr7")
            + wi("nop 7", "disp\t%ctpr2, disp=0x0")
            + wi("addd,0\t%dr9, %dr10, %dr0", "subd,1\t%dr7, %dr6, %dr8")
            + wi("std,2\t%dr4, 0x0, %dr8"))
    # setbn after setwd (func emits setwd and return %ctpr3 first).
    body = wi("setbn\trsz = 0x7, rbs = 0x8, rcur = 0x0") + body
    func("", f"col_{name}", "C", body,
         extra=f"\t.align\t8\n.L{fn}_prog:\n{prog}",
         meta=(len(layout), "".join(k for _, _, k in layout)))


for name, layout in (("L4", [("L", 4, "c")]),
                     ("LL4", [("L", 4, "c"), ("L", 4, "c")]),
                     ("LR4", [("L", 4, "c"), ("R", 4, "c")]),
                     ("LL4c1", [("L", 4, "c"), ("L", 4, "1")]),
                     ("LR4c1", [("L", 4, "c"), ("R", 4, "1")])):
    col(name, layout)


# --- 5. a[i] * b[...] at II 4 (gen_pocchk3.py's bk, more sources) --------
# Sources of a: P plain ldd D iterations ahead, A area, Z an area with
# step 0 (%aaincr2 = 0: every mova reads a[0]), N an area the loop
# never reads (the program runs, no mova), K the constant 1.0.
# Sources of b: P plain ldd backward, F plain ldd forward, A area
# backward, C an ldd of b's lowest element (an L1 hit), L ldaad backward
# (%aasti1, incr %aaincr1 = -1), K 1.0.  Options: bmas the plain loads'
# MAS, adcd the area's dcd, asz its size.
ONE = "0x3ff0000000000000"


def bk(name, asrc, bsrc, bch=2, asz=5, d=12, bmas=0, adcd=0):
    ring = 2 * d + 2
    rsz = ring // 2 - 1
    wsz = 8 + ring // 2 + 2
    wsz += wsz % 2
    fch, ach = 3, 4
    bup = 5 if bch != 5 else 2
    ops = []
    areas = []
    if asrc == "P":
        ops += ["ldd,0\t%dr5, 0x0, %b[0]", "addd,1\t%dr5, 0x8, %dr5"]
    elif asrc in "AZN":
        incr = 2 if asrc == "Z" else 0
        if asrc == "N":     # an area the loop never reads
            ops.append("addd,1\t%dr12, 0x0, %b[0]")
        areas.append(("a", f"dcd={adcd}, fmt=4, mrng=8, d=0, incr={incr}, "
                           f"ind=0, asz={asz}, abs=0, disp=0"))
    else:
        ops.append("addd,1\t%dr12, 0x0, %b[0]")
    if bsrc in "PF":
        ops += [f"ldd,{bch}\t%dr6, 0x0, %b[1]{m(bmas)}",
                f"{'subd' if bsrc == 'P' else 'addd'},{bup}\t%dr6, 0x8, %dr6"]
    elif bsrc == "C":
        ops.append(f"ldd,{bch}\t%dr6, 0x0, %b[1]")
    elif bsrc == "L":
        ops += [f"ldaad,{bch}\t%aad1[ %aasti1 ], %b[1]{m(bmas)}",
                f"incr,{bch}\t%aaincr1"]
    elif bsrc == "A":
        areas.append(("b", "dcd=0, fmt=4, mrng=8, d=1, incr=1, ind=1, "
                           "asz=5, abs=0, disp=0"))
    else:
        ops.append("addd,5\t%dr12, 0x0, %b[1]")
    prog = ""
    fn = f"al_bk_{name}"
    for k, (who, fields) in enumerate(areas):
        ch = 0 if k == 0 else 2
        if who == "a" and asrc == "N":
            continue
        ops.append(f"movad,{ch}\tarea=0, ind=0, am=1, be=0, "
                   f"%b[{0 if who == 'a' else 1}]")
    if areas:
        left = f"fapb\tct=1, {areas[0][1]}"
        right = f"fapb\tdpl=0, {areas[1][1]}" if len(areas) > 1 else None
        prog = f"\t.align\t8\n.L{fn}_prog:\n" + wi(left, right)
    ops += [f"fmuld,{fch}\t%b[{2 * d}], %b[{2 * d + 1}], %dr10",
            f"faddd,{ach}\t%dr11, %dr10, %dr11"]
    zero = [f"%b[{k}]" for k in range(ring)] + ["%dr10", "%dr11"]
    body = wi(f"setbn\trsz = {rsz:#x}, rbs = 0x8, rcur = 0x0")
    for k in range(0, len(zero), 6):
        body += wi(*(f"addd,{c}\t0x0, 0x0, {r}"
                     for c, r in enumerate(zero[k:k + 6])))
    bstart = "%dr4" if bsrc == "P" else "0x0"
    body += wi("addd,0\t%dr0, 0x0, %dr5", f"addd,1\t%dr1, {bstart}, %dr6",
               f"addd,3\t0x0, _f64,_lts0 {ONE}, %dr12", "subd,4\t0x0, 0x1, %dr13",
               "addd,5\t0x0, 0x0, %dr7")
    if areas:
        body += wi(f"ldisp\t%ctpr2, .L{fn}_prog",
                   "aaurwd,2\t%dr0, %aad0", "aaurwd,5\t%dr1, %aad1")
        body += wi("aaurwd,2\t%dr4, %aaind1", "aaurwd,5\t%dr13, %aaincr1")
        if asrc == "Z":
            body += wi("aaurwd,2\t%dr7, %aaincr2")
    if bsrc == "L":
        body += wi("aaurwd,2\t%dr4, %aasti1", "aaurwd,5\t%dr1, %aad1")
        body += wi("aaurwd,5\t%dr13, %aaincr1")
    body += wi(f"disp\t%ctpr1, .L{fn}_loop", "rwd,0\t%dr2, %lsr")
    body += wi("nop 7", "bap" if areas else None) + wi("nop 7")
    body += wi("nop 7", "rrd,0\t%clkr, %dr14")
    body += (f".L{fn}_loop:\n"
             + wi("loop_mode", "alc\talcf=1, alct=1", "abn\tabnf=1, abnt=1",
                  "ct\t%ctpr1 ? %NOT_LOOP_END", "nop 3", *ops))
    body += wi("rrd,0\t%clkr, %dr15")
    body += wi("nop 7", "disp\t%ctpr2, disp=0x0" if areas else None)
    body += wi("faddd,4\t%dr11, %dr10, %dr0", "subd,1\t%dr15, %dr14, %dr9")
    body += wi("std,2\t%dr3, 0x0, %dr9") + wi("nop 3")
    func("", f"bk_{name}", "B", body, wsz=wsz, extra=prog,
         meta=(d, asrc + bsrc))


for a, b in (("P", "P"), ("K", "P"), ("A", "P"), ("A", "F"), ("P", "A"),
             ("A", "A"), ("A", "C"), ("Z", "P"), ("K", "L"), ("A", "L"),
             ("Z", "K"), ("A", "K"), ("N", "P"), ("N", "K")):
    bk(f"{a}{b}", a, b)
bk("AP_b5", "A", "P", bch=5)
bk("AP_m20", "A", "P", bmas=0x20)
bk("KP_m20", "K", "P", bmas=0x20)
bk("AP_dcd2", "A", "P", adcd=2)
bk("AP_dcd3", "A", "P", adcd=3)
bk("ZP_dcd3", "Z", "P", adcd=3)
bk("AP_asz0", "A", "P", asz=0)


def main():
    for g, cases in groups.items():
        (HERE / f"apbleft{g}.s").write_text(
            "! SPDX-License-Identifier: GPL-3.0-or-later\n"
            "! Generated by tools/isa-probes/gen_apbleft.py; see apbleft.c.\n"
            "\t.ignore\tld_st_style\n\t.text\n"
            + "".join(t for _, _, _, t in cases))
        (HERE / f"apbleft{g}-cases.h").write_text(
            "/* SPDX-License-Identifier: GPL-3.0-or-later */\n"
            "/* Generated by tools/isa-probes/gen_apbleft.py.  */\n"
            + "".join(f'CASE ({n}, "{k}", {o}, "{h}")\n'
                      for n, k, (o, h), _ in cases))
        print(f"apbleft{g}.s: {len(cases)} functions")


if __name__ == "__main__":
    main()
