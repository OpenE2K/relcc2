# SPDX-License-Identifier: GPL-3.0-or-later
"""The machine model behind isa-score: operations parsed from decoded wide
instructions, their registers, memory accesses and resources, and the
latencies measured on the Elbrus 4C (spec/isa/timing.md, apb.md,
control-transfer.md).

Only the E2S (elbrus-v3) timing is measured; the guide says v1-v5 share
it.  For v6 and v7 the same latencies are used and the report says so.
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

MEASURED_ISETS = {1, 2, 3, 4, 5}
# v6 and later: loads take 5 cycles to a consumer instead of 3 (binutils
# bfd/elfxx-e2k.c build_plt_entry; the owner's recollection is that this
# is the only timing change).  Not measured.
V6_LOAD_LAT = 5

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
    "idiv":   dict(lat=11, to_int=11, fpside=False),
    "imul":   dict(lat=4, to_int=6, fpside=True),
    "fp":     dict(lat=4, to_int=6, fpside=True),    # fadd, fmul, fmin
    "cfp":    dict(lat=8, to_int=10, fpside=True),   # fmul_add etc.
    "fdivs":  dict(lat=11, to_int=13, fpside=True),
    "fdivd":  dict(lat=14, to_int=16, fpside=True),
    "fsqrt":  dict(lat=12, to_int=14, fpside=True),
    "conv":   dict(lat=5, to_int=6, fpside=True),
    "conv6":  dict(lat=6, to_int=7, fpside=True),    # fdtofs
    "fcmpr":  dict(lat=2, to_int=4, fpside=True),    # FP compare to a register
    "fminmax": dict(lat=2, to_int=4, fpside=True),   # fmax/fmin, also packed
    "pslow":  dict(lat=4, to_int=6, fpside=True),    # qphminposuh, qpsgn2mskb
    "packed": dict(lat=1, to_int=4, fpside=True),
    "pmul":   dict(lat=2, to_int=4, fpside=True),
    "fx":     dict(lat=5, to_int=7, fpside=True),    # 80-bit add
    "fxmul":  dict(lat=4, to_int=6, fpside=True),
    "fxdiv":  dict(lat=16, to_int=18, fpside=True),
    "mova":   dict(lat=5, to_int=5, fpside=None),    # no cluster penalty
    "aaur":   dict(lat=3, to_int=3, fpside=False),   # not measured
}

# Producers of predicates: (to a predicated ALU op, to ct, to PLS logic).
PRED_LAT = {"icmpp": (2, 3, 1), "fcmpp": (4, 5, 3), "plog": (2, 3, 1)}

# Prepare -> transfer through a CTPR.
# One array-prefetch area delivers at most this many bytes a cycle (the
# 8C2: two movaqp of one area in a loop_mode instruction take 2 cycles
# an iteration; timing.md, "Array prefetch").
APB_AREA_BYTES = 16
MOVA_BYTES = {"movab": 1, "movah": 2, "movaw": 4, "movad": 8, "movaq": 16,
              "movaqp": 16}

CTP_LAT = {"disp": 5, "sdisp": 5, "return": 6, "movtd": 9, "ldisp": 3}
# A conditional ct that is not taken waits for its CTPR only this long
# after a disp (monitors.md, ct_false_dK in tools/isa-probes/gen_pmu.py).
CTP_LAT_NOT_TAKEN = 2

BAP_TO_MOVA = 22    # apb.md, data in the caches

# timing.md, "Write-back conflicts": an FP-side result (muld, faddd, ...)
# bound for an integer operation arrives one cycle late when an
# integer-side operation (ALU or load, not a store) issues in the same
# channel PORT_DISTANCE cycles after it.
PORT_DISTANCE = 2
INT_SIDE = {"int", "cint", "getsp", "load", "ldqp", "idiv"}
STALL_QUANTUM = 4   # timing.md: an early consumer waits in 4-cycle steps


def stall(early):
    """Cycles an instruction waits when an operand is EARLY cycles late."""
    return math.ceil(early / STALL_QUANTUM) * STALL_QUANTUM if early > 0 else 0


FP_PREFIXES = ("fadd", "fsub", "fmul", "fmin", "fmax", "fscale", "frcp",
               "fsqrt", "fdiv", "fabs", "fneg", "fcmp", "fx", "pf", "qpf",
               "fdto", "fsto", "fxto", "movif", "movfi", "fstoi", "fdtoi",
               "frsqrt", "fma", "fms", "fnma", "fnms")


def class_of(op):
    """Latency class of OP, with the v6+ load latency."""
    cl = CLASSES.get(op.cls)
    if cl is not None and op.cls == "load" and getattr(op, "iset", 3) >= 6:
        cl = dict(cl, lat=V6_LOAD_LAT, to_int=V6_LOAD_LAT)
    return cl


def classify(name):
    """Latency class of an ALU operation by mnemonic."""
    n = name
    if n == "getsp":
        return "getsp"
    if re.match(r"(q?plog_|addcd|subcd)", n):
        return "int"      # one cycle (v5, measured on the 8C2)
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
    if re.match(r"fx(div|sqrt)", n):
        return "fxdiv"
    if re.match(r"fxmul", n):
        return "fxmul"
    if re.match(r"fx(add|sub|rsub|scale)", n):
        return "fx"
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
    if re.match(r"(u|s)?mul|umulx|smulx|[us]mulhd", n):
        return "imul"
    if re.match(r"(p|qp)?f(max|min)", n):
        return "fminmax"
    if n in ("qphminposuh", "qpsgn2mskb"):
        return "pslow"
    if n in ("qpfdtoifd", "qpfstoifs"):
        return "fp"
    if n in ("fdtofs", "qpfdtofs", "qpfdtois", "qpfdtoistr", "qpidtofs"):
        return "conv6"
    if re.match(r"(f[sdx]|i[sd]|pf|qpf|qpf[sd]|qpi[sd])to", n) or n in ("movif", "movfi"):
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
    if name not in unknown.known_names:
        op.known = False
    store = ALU_STORE.match(name) and not name.startswith("st_") or \
        name.startswith(("aaurw", "apurw"))
    if name.startswith(("aaurw", "apurw")):
        op.srcs = regs_of(operands[0])
        op.dsts = regs_of(operands[-1])
        return op
    if store:
        regs = [regs_of(o) for o in operands]
        op.srcs = [k for r in regs for k in r]
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
        regs = [regs_of(o) for o in operands]
        op.srcs = [k for r in regs[:2] + regs[3:] for k in r]
        op.dsts = regs[2] if len(regs) > 2 else []
        return op
    regs = [regs_of(o) for o in operands]
    if regs:
        op.dsts = regs[-1]
        op.srcs = [k for r in regs[:-1] for k in r]
    if op.cls == "load":
        sz = re.match(r"ld(qp|[bhwdq])", name)
        base = regs[0][0] if regs and regs[0] else None
        off = _lit(operands[1]) if len(operands) > 1 else None
        op.mem = ("ld", base, off, SIZE.get(sz.group(1), 8) if sz else 8)
    if any(k[0] == "p" for k in op.dsts) and name.endswith("b"):
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
                    ins.nop = int(m.group(1) or 1)
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
