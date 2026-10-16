# SPDX-License-Identifier: GPL-3.0-or-later
"""The machine model behind isa-score: operations parsed from decoded wide
instructions, their registers, memory accesses and resources, and the
latencies measured on the Elbrus 4C (spec/isa/timing.md, apb.md,
control-transfer.md).

v3 and v5 timing is measured on the 4C and the 8C2 (the guide says v1-v5
share it), v6 on the Elbrus-2C3 (timing.md, "Elbrus-2C3"); v7 uses the
v6 latencies and the report says so.
"""

import math
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "isa-decode"))
import e2kdis  # noqa: E402

_LOADER = getattr(yaml, "CSafeLoader", yaml.SafeLoader)
_YAML = {}


def load_yaml(text):
    """Parse TEXT once (the opcode table is large)."""
    if text not in _YAML:
        _YAML[text] = yaml.load(text, Loader=_LOADER)
    return _YAML[text]


# The decoder reads the same tables; let it share the parsed copies.
e2kdis.yaml = type("yaml", (), {"safe_load": staticmethod(load_yaml)})

MEASURED_ISETS = {1, 2, 3, 4, 5, 6}

# --- Latency classes (spec/isa/timing.md) ---------------------------------
#
# lat: to a consumer in the same domain and cluster; to_int: to an integer
# operation (FP-side results pay extra); fpside: the result lives on the
# FP side, so reaching the other cluster costs +2 (else +1).
CLASSES = {
    "int":    dict(lat=1, to_int=1, fpside=False),
    "cint":   dict(lat=2, to_int=2, fpside=False),   # add_addd etc.
    "getsp":  dict(lat=2, to_int=2, fpside=False),   # timing.md (pmu.c)
    "load":   dict(lat=3, to_int=3, fpside=False),   # L1 hit
    "ldqp":   dict(lat=11, to_int=11, fpside=False),  # v5: served by L2
    # Integer divides leave their result on the FP side (13 to an
    # integer op); smodx/umodx 10, 12 (timing.md, "Every v3 operation").
    "idiv":   dict(lat=11, to_int=13, fpside=True),
    "imod":   dict(lat=10, to_int=12, fpside=True),
    "imul":   dict(lat=4, to_int=6, fpside=True),
    "fp":     dict(lat=4, to_int=6, fpside=True),    # fadd, fmul, fmin
    "cfp":    dict(lat=8, to_int=10, fpside=True),   # fmul_add etc.
    "fdivs":  dict(lat=11, to_int=13, fpside=True),
    "fdivd":  dict(lat=14, to_int=16, fpside=True),
    "fsqrt":  dict(lat=12, to_int=14, fpside=True),   # fsqrts, pfsqrts
    "fsqrtid": dict(lat=11, to_int=13, fpside=True),  # fsqrttd: fdivd's
    "pfdivd": dict(lat=14, to_int=16, fpside=True),   # pfdivd, pfsqrttd
    "frcp":   dict(lat=6, to_int=8, fpside=True),     # frcps, frsqrts
    # Conversions: 4 to a consumer that reads the result in the format
    # it was produced in; reading it in another one (a double as a
    # single, an integer as a double: +1; a single as a double: +2) is
    # not modelled (timing.md, "Formats").
    "conv":   dict(lat=4, to_int=6, fpside=True),
    "fcmpr":  dict(lat=2, to_int=4, fpside=True),    # FP compare to a register
    "fminmax": dict(lat=2, to_int=4, fpside=True),   # fmax/fmin, also packed
    "pslow":  dict(lat=4, to_int=6, fpside=True),    # qphminposuh, qpsgn2mskb
    "packed": dict(lat=1, to_int=4, fpside=True),
    "pmul":   dict(lat=2, to_int=4, fpside=True),
    "fma":    dict(lat=5, to_int=7, fpside=True),    # v6 fused fma*, qpfma*
    "fx":     dict(lat=5, to_int=7, fpside=True),    # 80-bit add
    "fxmul":  dict(lat=4, to_int=6, fpside=True),    # 80-bit result; movx
    "fxconv": dict(lat=5, to_int=7, fpside=True),    # to and from 80-bit
    "fxcmpr": dict(lat=4, to_int=6, fpside=True),    # fxcmp*f flags word
    "fxsqrti": dict(lat=11, to_int=13, fpside=True),
    "fxdiv":  dict(lat=16, to_int=18, fpside=True),
    # State register reads: %ip, %upsr ... 2; the FP state 11.
    "rr":     dict(lat=2, to_int=2, fpside=False),
    "rrfp":   dict(lat=11, to_int=11, fpside=False),
    "mova":   dict(lat=5, to_int=5, fpside=None),    # no cluster penalty
    "aaur":   dict(lat=3, to_int=3, fpside=False),   # not measured
    # AAU loads (ldaa*, ALC2/5): served by L2 even with the line in L1,
    # 9 cycles to an integer op on the 4C from either cluster (apb.md,
    # "AAU loads"; timing.md; tools/isa-probes/memleft.c t_use_ldaad*,
    # measured with the line in L1, as the model assumes for loads; a
    # line only in L2 presumably costs the same).  Other consumers are
    # not measured and get the same.
    "aauld":  dict(lat=9, to_int=9, fpside=None),
}

# Producers of predicates: (to a predicated ALU op, to ct, to PLS logic).
# pcmpp: the v6 packed compares into a predicate (pcmp*op, qpcmp*ap, ...).
PRED_LAT = {"icmpp": (2, 3, 1), "fcmpp": (4, 5, 3), "plog": (2, 3, 1),
            "pcmpp": (4, 5, 3)}

# v6 and later (the Elbrus-2C3, timing.md "Elbrus-2C3"): an L1 load takes
# 5 cycles to any consumer in either cluster (3, +1 across, before), ldqp
# goes through L1 (5; 11 on v5), and an integer compare or predicate
# logic reaches a predicated ALU op in 1 cycle (2 before).
V6_CLASSES = {"load": dict(lat=5, to_int=5, fpside=None),
              "ldqp": dict(lat=5, to_int=5, fpside=None),
              "aauld": dict(lat=13, to_int=13, fpside=None)}  # the L2 hit
# v5 and later (the Elbrus-8C2 and 2C3): pfdivd and pfsqrttd take 16;
# an AAU load 11 on the 8C2 (its L2 hit).
V5_CLASSES = {"pfdivd": dict(lat=16, to_int=18, fpside=True),
              "aauld": dict(lat=11, to_int=11, fpside=None)}
V6_PRED_LAT = {"icmpp": (1, 3, 1), "plog": (1, 3, 1)}

# Prepare -> transfer through a CTPR.
# One array-prefetch area delivers at most this many bytes a cycle (the
# 8C2: two movaqp of one area in a loop_mode instruction take 2 cycles
# an iteration; timing.md, "Array prefetch"); 32 on v6 (the 2C3: 1 cycle).
APB_AREA_BYTES = 16
V6_APB_AREA_BYTES = 32
MOVA_BYTES = {"movab": 1, "movah": 2, "movaw": 4, "movad": 8, "movaq": 16,
              "movaqp": 16}

CTP_LAT = {"disp": 5, "sdisp": 5, "return": 6, "movtd": 9, "ldisp": 3}
# A conditional ct that is not taken waits for its CTPR only this long
# after a disp (monitors.md, ct_false_dK in tools/isa-probes/gen_pmu.py)
# or a return (tools/isa-probes/aaurwct.c ct_return_dK against ct_disp_dK,
# 4C and 8C2; lcc's early exits `ct %ctpr3 ? cond` a few cycles after the
# return; control-transfer.md).
CTP_LAT_NOT_TAKEN = 2
CTP_NOT_TAKEN_PREPARES = ("disp", "sdisp", "return")

BAP_TO_MOVA = 22    # apb.md, data in the caches
V6_BAP_TO_MOVA = 20  # the 2C3: the first mova may issue 2 cycles sooner


def bap_to_mova(op):
    """Cycles from a bap (OP) to the first mova that doesn't stall."""
    return V6_BAP_TO_MOVA if getattr(op, "iset", 3) >= 6 else BAP_TO_MOVA

# timing.md, "Write-back conflicts": an FP-side result (muld, faddd, ...)
# bound for an integer operation arrives one cycle late when an
# integer-side operation (ALU or load, not a store) issues in the same
# channel PORT_DISTANCE cycles after it.
PORT_DISTANCE = 2
INT_SIDE = {"int", "cint", "getsp", "load", "ldqp", "aauld", "rr", "rrfp"}
# FP-side classes whose results suffer no write-back conflict: packed
# integer results (pandd on the 4C, 8C2 and 2C3, plog and qplog on the
# 8C2 and 2C3 measured; the rest of the class inferred) reach an integer
# operation in 4 cycles with or without an integer operation 2 cycles
# after them in their channel (timing.md, "Write-back conflicts").
NO_WB_CONFLICT = {"packed"}

# timing.md, "Formats": under setwd dbl = 0 a 32-bit integer operation
# writes only the low half of its register, and an operation that reads
# the whole register (addd, andd, shld, movtd, gettagd measured; an
# address or store data inferred) gets the result DBL0_WIDE_READ cycles
# after the producer issues instead of 1, from either cluster (the
# crossing adds nothing); a slower producer is not delayed further
# (muls -> addd 6).  A reader of the 32-bit operand (adds, sxt) gets it
# in 1, and under dbl = 1 every reader does (2 from the other cluster)
# [tools/isa-probes/tagx.c; 4C, 8C2 and 2C3 alike].  The producers are
# the operations dbl governs (spec/abi/registers.md, dbl): the 32-bit
# integer ALU operations and combined operations, the tag operations,
# rrs; not loads (ldw zero-extends), sxt, or FP-unit operations
# (fadds, fstois, movts).
DBL0_WIDE_READ = 4
# (pocchk4.c rechecked it with more producers and readers: the flag
# forms' 32-bit flags word, combined operations, compares.)

# The x86 flag forms (adds_fw ...) and the combined integer operations
# (class cint) reach an integer consumer in their own cluster in 2 cycles
# and in the other in 4 (+2, not an integer op's +1), and store data in
# 1, 3 across [tools/isa-probes/pocchk4.c; 4C, 8C2 and 2C3; timing.md].
CINT_CROSS = 2
CINT_STORE_DATA = 1
DBL_CLASSES = {"int", "cint", "imul", "idiv", "imod", "rr"}
DBL_NOT = ("movt",)          # movts and its forms: FP unit, dbl ignored

# timing.md, "Elbrus-8C2": on v5 an FP operation's result reaches the
# single or double operand of an 80-bit operation 2 cycles later than
# on the 4C and 2C3 (faddd -> fxadddd 6, fxadddd -> fxadddd 7, fxtofd ->
# fxadddx 7, fxdivdd -> fxdivdd 18), and a load in the same cluster
# 1 later (4, as from the other cluster); an integer operation, a load
# from the other cluster and edges into the 80-bit operand are unchanged
# [tools/isa-probes/sregs.c latg, lp].  Modelled as a floor: such an
# operand waits as if the value came from the other cluster (FP lat + 2,
# load lat + 1).  An FP result from the other cluster (6 on the 4C) was
# not measured on the 8C2; the floor leaves it at 6.  Integer multiplies
# and divides, packed results and the fx compares and narrow-input
# conversions (fdtofx) were not measured and are left alone.
FX_NARROW_ISETS = {5}
FX_NARROW_FP = 2
FX_NARROW_LOAD = 1
NOT_FP_OPS = {"imul", "idiv", "imod", "packed", "pmul", "pslow"}

# timing.md, "Memory", "Store to load": a load that a store just made
# keeps out of L1 is served by L2, so its value comes at the L2 hit
# latency (4C 9, 8C2 11, 2C3 13) in any channel, no cluster crossing
# added.  Which loads, by D, the cycles from the store's issue to the
# load's (tools/isa-probes/damlist.c):
#   every version: D 1, a load of the stored aligned doubleword;
#   v5:  D 2-3, a load of the stored 32-byte line that reads a byte the
#        store did not write;
#   v6+: D 2-4, a load of the stored doubleword that overlaps the stored
#        bytes at another width; D 4 also any other load of the stored
#        doubleword except the stored bytes at the store's width.
# v4 was not measured and gets v3's rule.  On the 2C3 other bytes and
# other doublewords at D 1, and on all three a load in the store's own
# instruction, pay in some runs only; they are left out.  Addresses are
# compared by base register and literal offset, the base assumed 32-byte
# aligned: loads through other registers (a list walk) are left to
# profile.py's measurement.
STORE_RELOAD_LAT = {3: 9, 4: 9, 5: 11, 6: 13, 7: 13}
STORE_RELOAD_WINDOW = 4             # the longest D that can pay


def store_reload(iset, d, st, ld):
    """True if a load LD = (base, offset, size) issued D cycles after a
    store ST = (base, offset, size) gets its value from L2."""
    if st[0] != ld[0] or not 1 <= d <= STORE_RELOAD_WINDOW:
        return False
    s0, s1 = st[1], st[1] + st[2]
    l0, l1 = ld[1], ld[1] + ld[2]
    same_dw = s0 // 8 == l0 // 8
    if d == 1:
        return same_dw
    if iset == 5:
        return d <= 3 and s0 // 32 == l0 // 32 and not (s0 <= l0 and l1 <= s1)
    if iset >= 6 and same_dw:
        if l0 < s1 and s0 < l1 and st[2] != ld[2]:
            return True
        return d == 4 and (l0, l1) != (s0, s1)
    return False

# timing.md, "Throughput": the divider in ALC5 (every divide, square
# root and reciprocal estimate, integer, FP, packed and 80-bit) stalls
# one issued 1 cycle after another by 4, an fsqrts or pfsqrts 5 cycles
# after another by 2, and anything within 9 cycles after an smodx or
# umodx until those 9 have passed.  The fractional averages at distance
# 2 (sdivd 3.4, fsqrts 2.8, ...) are not modelled.
DIVIDER_OPS = ("fdivs", "fdivd", "sdivs", "sdivd", "udivs", "udivd",
               "fsqrts", "fsqrtid", "fsqrttd", "sdivx", "udivx", "smodx",
               "umodx", "pfdivs", "pfdivd", "pfsqrts", "pfsqrttd", "frcps",
               "frsqrts")
DIVIDER_STALL = {1: 4}
FSQRT_STALL = {5: 2}
FSQRT_OPS = ("fsqrts", "pfsqrts")
DIVIDER_BUSY = {"smodx": 9, "umodx": 9}


def is_divider(name):
    """Whether NAME issues to the ALC5 divider."""
    return name in DIVIDER_OPS or name.startswith(("fxdiv", "fxsqrt"))

# timing.md, "Throughput": two stores in one instruction whose addresses
# agree in bits 6-7 take 2 cycles.  Addresses are known only as base +
# offset; the model assumes the base is 256-byte aligned (as measured).
STORE_BANK_MASK = 0xc0
STALL_QUANTUM = 4   # timing.md: an early consumer waits in 4-cycle steps


def stall(early):
    """Cycles an instruction waits when an operand is EARLY cycles late."""
    return math.ceil(early / STALL_QUANTUM) * STALL_QUANTUM if early > 0 else 0


FP_PREFIXES = ("fadd", "fsub", "fmul", "fmin", "fmax", "fscale", "frcp",
               "fsqrt", "fdiv", "fabs", "fneg", "fcmp", "fx", "pf", "qpf",
               "fdto", "fsto", "fxto", "movif", "movfi", "fstoi", "fdtoi",
               "frsqrt", "fma", "fms", "fnma", "fnms")


def class_of(op):
    """Latency class of OP on its ISA version."""
    if getattr(op, "iset", 3) >= 6 and op.cls in V6_CLASSES:
        return V6_CLASSES[op.cls]
    if getattr(op, "iset", 3) >= 5 and op.cls in V5_CLASSES:
        return V5_CLASSES[op.cls]
    return CLASSES.get(op.cls)


def latency_floor(p, c, key):
    """The least latency from producer P to consumer C through register
    KEY that the operand's width or format imposes: a 32-bit result read
    as 64 bits under dbl = 0, and v5's narrow operand of an 80-bit
    operation (0: no floor)."""
    floor = 0
    if p.half and p.dbl == 0 and key in c.wide:
        floor = DBL0_WIDE_READ
    if key in c.narrow and getattr(c, "iset", 3) in FX_NARROW_ISETS:
        cl = class_of(p)
        if p.cls == "load":
            floor = max(floor, cl["lat"] + FX_NARROW_LOAD)
        elif cl and cl["fpside"] and p.cls not in NOT_FP_OPS:
            floor = max(floor, cl["lat"] + FX_NARROW_FP)
    return floor


def apb_area_bytes(iset):
    """Bytes one array-prefetch area delivers a cycle on ISET."""
    return V6_APB_AREA_BYTES if iset >= 6 else APB_AREA_BYTES


def pred_lat(op):
    """(to a predicated ALU op, to ct, to PLS logic) for a predicate
    producer OP on its ISA version."""
    if getattr(op, "iset", 3) >= 6 and op.cls in V6_PRED_LAT:
        return V6_PRED_LAT[op.cls]
    return PRED_LAT.get(op.cls, (1, 1, 1))


def classify(name):
    """Latency class of an ALU operation by mnemonic."""
    n = name
    if n == "getsp":
        return "getsp"
    if n.startswith("popcnt"):
        return "int"      # 1 to an integer op (timing.md), not packed
    if n.startswith("ldaa"):
        return "aauld"
    if re.match(r"(addcd|subcd)", n):
        return "int"      # 1 to any consumer in its cluster (timing.md)
    if re.match(r"q?plog_", n):
        return "packed"   # 1 to packed, FP, store data; 4 to an integer op
    if n == "qpmpsadbh":
        return "pmul"
    if n.startswith("mova"):
        return "mova"
    if n.startswith(("aaurr", "apurr")):
        return "aaur"
    if n.startswith("ldqp"):
        return "ldqp"
    if re.match(r"(ld|ldg|ldc)", n) and n != "ldisp":
        return "load"
    if re.match(r"[su]div", n):
        return "idiv"
    if re.match(r"[su]modx", n):
        return "imod"
    if re.match(r"[su]mulx_f", n):
        return "imul"
    if re.match(r"fxsqrti", n):
        return "fxsqrti"
    if re.match(r"fx(div|sqrt)", n):
        return "fxdiv"
    if re.match(r"fxmul[sdx][sd]$", n):
        return "fx"          # a single or double result: 5
    if re.match(r"fxmul", n) or n in ("movx", "movxa", "movxc"):
        return "fxmul"
    if re.match(r"fx(add|sub|rsub|scale)", n):
        return "fx"
    if re.match(r"fxcmp(ud|od)[sdx]f$", n):
        return "fxcmpr"
    if re.match(r"fxto|(f[sd]|i[sd])tofx$", n):
        return "fxconv"
    if n in ("pfdivd", "pfsqrttd"):
        return "pfdivd"
    if n == "fsqrttd":
        return "fdivd"
    if n == "fsqrtid":
        return "fsqrtid"
    if n in ("frcps", "frsqrts"):
        return "frcp"
    if n == "phminposuh":
        return "pmul"
    if re.match(r"(p?f|qpf)divs", n):
        return "fdivs"
    if re.match(r"(p?f|qpf)div", n):
        return "fdivd"
    if re.match(r"(p?f|qpf)sqrt", n):
        return "fsqrt"
    if "_" in n and n.startswith(("f", "p", "qp")) and not n.startswith("pass"):
        return "cfp" if (n.startswith(("f", "pf", "qpf"))) else "cint"
    if "_" in n:
        return "cint"
    if re.match(r"(qp)?fn?m(a|s|as|sa)[sd]$", n):
        return "fma"
    if re.match(r"(u|s)?mul|umulx|smulx|[us]mulhd|clmul", n):
        return "imul"
    if re.match(r"(p|qp)?f(max|min)", n):
        return "fminmax"
    if n in ("qphminposuh", "qpsgn2mskb"):
        return "pslow"
    if n in ("qpfdtoifd", "qpfstoifs", "pfdtoifd", "pfstoifs"):
        return "fp"
    if re.match(r"(f[sdx]|i[sd]|pf|pf[sd]|pi[sd]|qpf|qpf[sd]|qpi[sd])to", n) or n in ("movif", "movfi"):
        return "conv"
    if re.match(r"(p?fcmp|qpfcmp)", n):
        return "fcmpr"
    if re.match(r"(f|pf|qpf)(add|sub|mul|min|max|scale|abs|neg|rcp|rsqrt|hadd|hsub|addsub|mul|ma)", n):
        return "fp"
    if re.match(r"q?pmul|psadbw|qpsadbw|q?pmadd", n):
        return "pmul"
    if re.match(r"q?p(?!ass)", n) and not n.startswith(("puttag", "pref")):
        return "packed"
    return "int"


def is_fp_consumer(name):
    """Whether NAME takes its operands on the FP side, where FP-side
    results arrive without the +2 an integer operation pays (packed
    integer operations count: timing.md)."""
    if name.startswith(("fcmp", "pfcmp", "qpfcmp", "fxcmp", "idto", "isto")):
        return True
    return CLASSES.get(classify(name), {}).get("fpside") is True


# --- Operands --------------------------------------------------------------

REG = re.compile(
    r"%(?:(d|qp|q|x)?(r|g)(\d+)|(d|qp|q|x)?b\[(\d+)\]|pred(\d+)|ctpr(\d+)|"
    r"(aa[a-z_]+\d*)|(empty(?:\.hi|\.lo)?)|([a-z_][a-z0-9_.]*))")


def regs_of(text):
    """Register keys named in operand TEXT.  A %qr name (quad, before
    v5) covers two registers; a %qpr name (v5+) is one register."""
    out = []
    for m in REG.finditer(text):
        if m.group(3) is not None:
            n, k = int(m.group(3)), m.group(2)
            out.append((k, n))
            if m.group(1) == "q":
                out.append((k, n + 1))
        elif m.group(5) is not None:
            n = int(m.group(5))
            out.append(("b", n))
            if m.group(4) == "q":
                out.append(("b", n + 1))
        elif m.group(6) is not None:
            out.append(("p", int(m.group(6))))
        elif m.group(7) is not None:
            out.append(("c", int(m.group(7))))
        elif m.group(8) is not None:
            out.append(("aau", m.group(8)))
        elif m.group(9) is not None:
            pass  # %empty: no register
        else:
            out.append(("sr", m.group(10)))
    for m in re.finditer(r"@p(\d+)", text):
        out.append(("lp", int(m.group(1))))
    return out


def wide_regs(texts):
    """Register keys the operand TEXTS name as 64 bits or wider (%dr,
    %db[], %dg and the q, qp and x forms): an operation reading them reads
    the whole register."""
    out = set()
    for text in texts:
        for m in REG.finditer(text):
            if m.group(3) is not None and m.group(1):
                n, k = int(m.group(3)), m.group(2)
                out.add((k, n))
                if m.group(1) == "q":
                    out.add((k, n + 1))
            elif m.group(5) is not None and m.group(4):
                n = int(m.group(5))
                out.add(("b", n))
                if m.group(4) == "q":
                    out.add(("b", n + 1))
    return out


def narrow_dst(text):
    """Whether destination operand TEXT is a 32-bit register (%rN, %b[N],
    %gN)."""
    return any((m.group(3) is not None and not m.group(1))
               or (m.group(5) is not None and not m.group(4))
               for m in REG.finditer(text))


# The single or double operand of an 80-bit operation (the rest are
# 80-bit): src2 of fxadd/fxsub/fxrsub/fxmul/fxdiv<a><r>, src1 of
# fxdivt<a><r> (spec/isa/semantics.md).
FX_NARROW_SRC2 = re.compile(r"fx(add|sub|rsub|mul|div)[sd][sdx]$")
FX_NARROW_SRC1 = re.compile(r"fxdivt[sd][sdx]$")


LIT_LTS = {"_f16s": 0.5, "_f32s": 1, "_f64": 2, "_lit32": 1, "_lit64": 2}

SIZE = {"b": 1, "h": 2, "w": 4, "d": 8, "q": 16, "qp": 16}


@dataclass
class Op:
    text: str
    name: str
    unit: str                # alc, aas, pls, cs0, cs1, ct, ss, hs
    chan: int = None
    srcs: list = field(default_factory=list)
    dsts: list = field(default_factory=list)
    preds: list = field(default_factory=list)   # condition registers read
    cls: str = None          # latency class for register results
    lts: float = 0           # literal syllable use (32-bit units)
    cs1: bool = False        # uses CS1 (MAS, AAU register access)
    mem: tuple = None        # (kind 'ld'/'st', base key, offset or None, size)
    barrier: bool = False    # orders against every other operation
    target: int = None       # branch target address
    known: bool = True       # mnemonic understood
    data: list = field(default_factory=list)    # a store's data registers
    lits: list = field(default_factory=list)    # (syllables, literal text)
    wide: set = field(default_factory=set)      # sources read as 64+ bits
    narrow: set = field(default_factory=set)    # an fx op's narrow operand
    half: bool = False       # a 32-bit integer result that dbl governs
    dbl: int = None          # the function's setwd dbl (analysis.functions)


ALU_STORE = re.compile(r"(st[bhwdq]|stq?p|stgd|stap|stcs|stds|stes|stfs|stgs|stss|staa)")
BARRIER_OPS = {"setwd", "setbn", "setbp", "vfrpsz", "wait", "setei", "flushts"}


def parse_line(line, unknown):
    """One decoded line (objdump syntax) -> Op, or None for attributes."""
    line = line.strip()
    head, _, rest = line.partition(" ")
    rest = rest.strip()
    parts = head.split(",")
    name = parts[0]
    chan = int(parts[1]) if len(parts) > 1 and parts[1].isdigit() else None
    cond = ""
    if " ? " in rest:
        rest, cond = rest.split(" ? ", 1)
    if name in ("nop", "ipd", "loop_mode"):
        return None
    preds = [r for r in regs_of(cond)]
    if "NOT_LOOP_END" in cond or "LOOP_END" in cond:
        preds = [k for k in preds if k != ("sr", "NOT_LOOP_END")
                 and k != ("sr", "LOOP_END")] + [("sr", "lsr")]
    if "MLOCK" in cond:
        preds = [k for k in preds if k[1] != "MLOCK"] + [("sr", "mlock")]

    # Control syllables.
    if name in ("disp", "sdisp", "ldisp", "return", "pref", "gettsd"):
        op = Op(line, name, "cs0", dsts=regs_of(rest.split(",")[0]))
        if name in ("disp", "ldisp"):
            m = re.search(r",\s*(0x[0-9a-f]+)", rest)
            if m:
                op.target = int(m.group(1), 16)
        return op
    if name in ("ct", "call", "ibranch", "rbranch", "done", "iret", "hret",
                "glaunch", "icalld", "ibranchd"):
        op = Op(line, name, "ct", srcs=[k for k in regs_of(rest.split(",")[0])
                                        if k[0] == "c"], preds=preds)
        if name in ("ibranch", "rbranch"):
            m = re.match(r"(0x[0-9a-f]+)", rest)
            if m:
                op.target = int(m.group(1), 16)
        if name == "rbranch":
            op.preds.append(("sr", "mlock"))
        if name in ("call", "icalld"):
            op.barrier = True
            op.cs1 = True
        return op
    if name in ("alc", "abn", "abp", "abg", "bap", "eap"):
        op = Op(line, name, "ss")
        if name in ("bap", "eap"):
            op.dsts = [("aau", "apb")]
            op.srcs = [("c", 2)]
        return op
    if name in BARRIER_OPS:
        return Op(line, name, "cs1", barrier=True, cs1=True)
    if name in ("pass", "landp", "andp", "orp", "movep"):
        regs = [regs_of(o) for o in rest.split(",")]
        return Op(line, name, "pls", srcs=[k for r in regs[:-1] for k in r],
                  dsts=regs[-1], cls="plog")
    if name.startswith("mova"):
        m = re.search(r"area\s*=\s*(\d+).*am\s*=\s*(\d+)", rest)
        dst = rest.split(",")[-1]
        if name == "movaqp":
            # v5's movaqp writes one 128-bit register, though the
            # disassembly names it like a pre-v5 pair (%qb[N]).
            dst = re.sub(r"%q(?!p)", "%qp", dst)
        regs = regs_of(dst)
        half = (chan or 0) // 2
        area = ("aau", f"area{half}.{m.group(1)}") if m else ("aau", "area")
        op = Op(line, name, "aas", chan, srcs=[("aau", "apb"), area],
                dsts=regs, cls="mova")
        if m and m.group(2) == "1":
            op.dsts = op.dsts + [area]
        return op
    if name == "incr":   # encoded in CDS, no ALU channel
        return Op(line, name, "cds", chan, srcs=regs_of(rest),
                  dsts=[("aau", "sti")])

    # ALU operations.
    # Literals: (32-bit syllables, text); gas shares identical ones.
    lits = [(LIT_LTS[m.group(1)], f"{m.group(1)} {m.group(2)}") for m in
            re.finditer(r"(_f16s|_f32s|_f64|_lit32|_lit64)\b[,_a-z0-9]*\s+(\S+?)(?=[\s,\]]|$)",
                        rest)]
    lts = sum(n for n, _ in lits)
    cs1 = "mas" in rest or name.startswith(("aaur", "apur"))
    rest_nomas = re.sub(r",\s*mas\s*=\s*\S+", "", rest)
    operands = [o.strip() for o in rest_nomas.split(",")] if rest_nomas else []
    op = Op(line, name, "alc", chan, lts=lts, cs1=cs1, preds=preds, lits=lits)
    op.cls = classify(name)
    if name in ("rrs", "rrd"):
        op.cls = "rrfp" if re.search(r"%(pfpfr|fpcr|fpsr)\b", rest) else "rr"
    if name not in unknown.known_names:
        op.known = False
    store = ALU_STORE.match(name) and not name.startswith("st_") or \
        name.startswith(("aaurw", "apurw"))
    if name.startswith(("aaurw", "apurw")):
        op.srcs = regs_of(operands[0])
        op.dsts = regs_of(operands[-1])
        # It takes its register like a store's data: an integer result
        # from either cluster the next cycle (tools/isa-probes/aaurwct.c,
        # 4C and 8C2; timing.md "Clusters"; lcc's `aaurw,2 %g, %aabf0`
        # after an `adds,3` ending its APB loops).
        op.data = list(op.srcs)
        op.wide = wide_regs(operands[:1])
        return op
    if store:
        regs = [regs_of(o) for o in operands]
        op.srcs = [k for r in regs for k in r]
        op.wide = wide_regs(operands)
        if regs:
            op.data = regs[0] if name.startswith("staa") else regs[-1]
        if name.startswith("staa"):
            op.mem = ("st", None, None, 8)
        else:
            sz = re.match(r"st(qp|[bhwdq])", name)
            off = _lit(operands[1]) if len(operands) > 1 else None
            base = regs[0][0] if regs and regs[0] else None
            op.mem = ("st", base, off, SIZE.get(sz.group(1), 8) if sz else 8)
        return op
    if name.startswith("merge"):
        # merged a, b, dst, pred; merge_addd a, b, c, dst, pred.
        d = 3 if "_" in name else 2
        regs = [regs_of(o) for o in operands]
        op.srcs = [k for r in regs[:d] + regs[d + 1:] for k in r]
        op.dsts = regs[d] if len(regs) > d else []
        op.wide = wide_regs(operands[:d] + operands[d + 1:])
        op.half = op.cls in DBL_CLASSES and len(operands) > d \
            and narrow_dst(operands[d])
        return op
    regs = [regs_of(o) for o in operands]
    if regs:
        op.dsts = regs[-1]
        op.srcs = [k for r in regs[:-1] for k in r]
        op.wide = wide_regs(operands[:-1])
        op.half = op.cls in DBL_CLASSES and not name.startswith(DBL_NOT) \
            and narrow_dst(operands[-1])
    if FX_NARROW_SRC2.match(name) and len(regs) > 2:
        op.narrow = set(regs[1])
    elif FX_NARROW_SRC1.match(name) and len(regs) > 2:
        op.narrow = set(regs[0])
    if op.cls in ("load", "aauld"):
        sz = re.match(r"ld(?:aa)?(qp|[bhwdq])", name)
        base = regs[0][0] if regs and regs[0] else None
        off = _lit(operands[1]) if len(operands) > 1 else None
        op.mem = ("ld", base, off, SIZE.get(sz.group(1), 8) if sz else 8)
    if any(k[0] == "p" for k in op.dsts) and re.match(
            r"q?pcmp(eq|gt)[bhwd][ao]p$", name):
        op.cls = "pcmpp"
    elif any(k[0] == "p" for k in op.dsts) and (
            name.endswith("b") or name.startswith("cctop")):
        # cctop<c> is a compare of a flags word: 2 to a predicated op (1 on
        # v6), 3 to ct, and no write-back conflict (pocchk4.c).
        op.cls = "fcmpp" if name.startswith(("fcmp", "pfcmp", "fxcmp")) \
            else "icmpp"
    if any(k[0] == "c" for k in op.dsts):          # movtd
        op.cls = "movtd"
    if name == "getsp":
        op.srcs.append(("sr", "sp"))
        op.dsts.append(("sr", "sp"))
    return op


def _lit(text):
    m = re.search(r"(0x[0-9a-f]+|\d+)\s*$", text)
    if not m or "%" in text:
        return None
    v = int(m.group(1), 0)
    if "_f16s" in text and v & 0x8000:
        v -= 0x10000
    elif "_f32s" in text and v & 0x80000000:
        v -= 1 << 32
    return v


# --- Wide instructions -----------------------------------------------------

@dataclass
class Instr:
    addr: int
    size: int
    nop: int                 # extra cycles after it (HS nop)
    ops: list
    loop_mode: bool = False
    text: list = None

    def cycles(self):
        return 1 + self.nop


class Decoder:
    """Decodes code with one cached copy of the spec per ISA version."""

    def __init__(self, iset):
        self.spec = e2kdis.Spec(iset)
        self.printer = e2kdis.Printer(self.spec)
        tables = load_yaml((ROOT / "spec" / "isa" / "binutils-ops.yaml")
                           .read_text())
        self.channels = {}
        for op in tables["alu_ops"]:
            if iset in op["isets"]:
                for n in [op["name"]] + op.get("aliases", []):
                    self.channels.setdefault(n, set()).update(op["channels"])
        self.known_names = set(self.channels)
        self.iset = iset

    def decode(self, data, base):
        """Yield Instr for each wide instruction; APB programs and
        undecodable bytes are skipped (16 bytes at a time)."""
        off = 0
        while off + 8 <= len(data):
            length = (((data[off] >> 4) & 7) + 1) * 8
            b = e2kdis.Bundle(data[off:off + length]) \
                if off + length <= len(data) else None
            if b is None or not b.valid:
                off += 16
                continue
            lines = self.printer.bundle(b, base + off)
            ins = Instr(base + off, length, 0, [], text=lines)
            for line in lines:
                s = line.strip()
                if s.startswith("nop"):
                    m = re.match(r"nop\s*(\d*)", s)
                    # objdump prints an instruction with no extra cycles
                    # as a bare "nop" (gas encodes "nop" as "nop 0").
                    ins.nop = int(m.group(1) or 0)
                elif s == "loop_mode":
                    ins.loop_mode = True
                op = parse_line(s, self)
                if op is not None:
                    op.iset = self.iset
                    ins.ops.append(op)
            yield ins
            off += length

    def allowed(self, op):
        """ALU channels OP may use on this ISA version."""
        if op.unit == "aas":
            half = (op.chan or 0) // 2 * 2
            return {half, half + 1}
        if op.unit != "alc":
            return None
        ch = self.channels.get(op.name)
        if not ch:
            return {op.chan} if op.chan is not None else set(range(6))
        return set(ch)
