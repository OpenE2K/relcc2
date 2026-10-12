# SPDX-License-Identifier: GPL-3.0-or-later
"""Blocks, loops and their cycle counts for isa-score.

achieved: the compiler's schedule as written (instructions + nops +
          interlock stalls in 4-cycle steps), every load an L1 hit.
possible: the same operations list-scheduled on the chosen ISA version's
          channels and latencies, under two dependence models:
            regs  keep the compiler's registers (true, anti and output
                  dependences) and order memory accesses unless they
                  provably don't overlap;
            flow  only true register dependences, and memory order only
                  where accesses provably overlap.
For a single-block loop the measure is the initiation interval: the
achieved one from simulating two iterations, the possible one
max(ResMII, RecMII) under each model.
"""

import itertools
import math
from dataclasses import dataclass, field

import model

REGKINDS = ("r", "b", "g")


# --- Blocks and functions --------------------------------------------------

@dataclass
class Block:
    func: str
    instrs: list
    succ_self: bool = False      # last transfer goes back to its start
    rotates: bool = False        # abn in the loop: %b[] rotates by 2
    back_edge: bool = False      # transfer to an earlier block
    cond: bool = False           # the last transfer is conditional
    ibranch: bool = False        # the last transfer is an ibranch
    ibranch_taken: bool = False  # ... assumed taken (+6): unconditional or
                                 # a back edge (control-transfer.md)
    target: int = None           # address the last transfer goes to
    falls: bool = True           # can continue to the next block

    @property
    def addr(self):
        return self.instrs[0].addr

    def ops(self):
        return [(i, op) for i, ins in enumerate(self.instrs) for op in ins.ops]


def apb_units(data, off):
    """Length of an APB program at OFF: 16-byte units up to ct = 1."""
    n = 0
    while off + 16 * n + 4 <= len(data):
        word = int.from_bytes(data[off + 16 * n:off + 16 * n + 4], "little")
        n += 1
        if word >> 31:
            break
    return n


def functions(elf, dec):
    """Yield (name, [Instr]) for every function symbol in code sections."""
    for sec in elf.sections:
        end = sec.addr + len(sec.data)
        syms = sorted((s for s in elf.symbols.values()
                       if sec.addr <= s.addr < end), key=lambda s: s.addr)
        instrs = list(dec.decode(sec.data, sec.addr))
        # Drop APB programs (data reached through ldisp).
        skip = set()
        for ins in instrs:
            for op in ins.ops:
                if op.name == "ldisp" and op.target is not None:
                    a = op.target
                    if sec.addr <= a < end:
                        n = apb_units(sec.data, a - sec.addr)
                        skip.update(range(a, a + 16 * n))
        instrs = [i for i in instrs if i.addr not in skip]
        for k, s in enumerate(syms):
            stop = s.addr + s.size if s.size else \
                (syms[k + 1].addr if k + 1 < len(syms) else end)
            body = [i for i in instrs if s.addr <= i.addr < stop]
            if body:
                yield s.name, body


def blocks(name, instrs):
    """Split a function into basic blocks."""
    addrs = {i.addr for i in instrs}
    leaders = {instrs[0].addr}
    for k, ins in enumerate(instrs):
        for op in ins.ops:
            if op.target is not None and op.target in addrs \
                    and op.name != "ldisp":
                leaders.add(op.target)
            if op.unit == "ct" and k + 1 < len(instrs):
                leaders.add(instrs[k + 1].addr)
    out, cur = [], []
    ctpr = {}                      # last disp target per CTPR, program order
    for ins in instrs:
        if ins.addr in leaders and cur:
            out.append(Block(name, cur))
            cur = []
        cur.append(ins)
        for op in ins.ops:
            if op.name == "disp" and op.dsts:
                ctpr[op.dsts[0]] = op.target
        last = cur[-1]
        cts = [op for op in last.ops if op.unit == "ct"]
        if cts:
            b = Block(name, cur)
            ct = cts[0]
            tgt = ct.target if ct.name in ("ibranch", "rbranch") else \
                ctpr.get(ct.srcs[0]) if ct.srcs and ct.name == "ct" else None
            b.target = tgt
            b.falls = ct.name == "call" or bool(ct.preds)
            if tgt is not None:
                b.succ_self = tgt == cur[0].addr
                b.back_edge = tgt <= cur[0].addr
            b.cond = bool(ct.preds) and ct.name in ("ct", "ibranch")
            b.ibranch = ct.name == "ibranch"
            b.ibranch_taken = b.ibranch and (b.back_edge or not ct.preds)
            b.rotates = any(op.name == "abn" for i in cur for op in i.ops)
            out.append(b)
            cur = []
    if cur:
        out.append(Block(name, cur))
    return out


# --- Latencies -------------------------------------------------------------

def cluster(ch):
    return None if ch is None else ch // 3


def latency(p, c, key, pch=None, cch=None):
    """Cycles from producer P's issue until consumer C may issue, for a
    value in KEY; PCH and CCH are their ALU channels (None: unknown)."""
    kind = key[0]
    if kind == "lp":
        return 0
    if kind == "p":
        lats = model.pred_lat(p)
        if c.unit == "ct":
            return lats[1]
        if c.unit == "pls":
            return lats[2]
        return lats[0]
    if kind == "c":
        if p.cls == "movtd":
            return model.CTP_LAT["movtd"]
        return model.CTP_LAT.get(p.name, 1)
    if kind == "aau":
        if p.name == "bap" and c.unit == "aas":
            return model.bap_to_mova(p)
        return 1
    if kind not in REGKINDS:
        return 1
    cl = model.class_of(p)
    if cl is None:
        return 1
    # A store takes its data like an FP consumer: no FP-side -> integer
    # penalty, and an integer result from either cluster the next cycle;
    # an FP-side result from the other cluster 2 cycles later (timing.md,
    # "Clusters"; pocchk2.c t_fpst_*, t_ist_*, all three machines).
    if key in c.data:
        pc, cc = cluster(pch), cluster(cch)
        if cl["fpside"] and pc is not None and cc is not None and pc != cc:
            return cl["lat"] + 2
        return cl["lat"]
    fp_cons = model.is_fp_consumer(c.name) or c.unit == "aas"
    lat = cl["lat"] if fp_cons else cl["to_int"]
    # A 64-bit FP-side result read by a 128-bit (qp) operation pays as
    # if crossing (8C2: faddd -> qpfaddd 6, paddw -> qpaddd 4).
    if cl["fpside"] and c.name.startswith("qp") and not p.name.startswith("qp"):
        lat = max(lat, cl["lat"] + 2, cl["to_int"])
    if c.unit == "ct":               # e.g. a register read by a call
        return lat
    pc, cc = cluster(pch), cluster(cch)
    if pc is not None and cc is not None and pc != cc and p.unit == "alc":
        if cl["fpside"]:
            # The other cluster: FP 6, packed 4 (paddw, qpaddd: both
            # machines), i.e. at least the cost to an integer consumer.
            lat = max(lat, cl["lat"] + 2, cl["to_int"])
        elif cl["fpside"] is False:
            lat += 1
    return lat


# --- Dependences -----------------------------------------------------------

@dataclass
class Dep:
    src: int
    dst: int
    key: tuple
    kind: str            # raw, war, waw, mem, order
    dist: int = 0        # iterations (loop-carried when > 0)


def _overlap(a, b):
    """True/False when two memory accesses provably do/don't overlap,
    None when unknown."""
    (_, ba, oa, sa, va), (_, bb, ob, sb, vb) = a, b
    if ba is None or bb is None or ba != bb or va != vb \
            or oa is None or ob is None:
        return None
    return oa < ob + sb and ob < oa + sa


def dependences(nodes, mode, carried=False, rotates=False):
    """Dependences among NODES, a list of (instr index, op) in program
    order.  With CARRIED, also loop-carried ones (dist 1, or more for
    rotating %b[] registers)."""
    deps = []
    last_def = {}                  # key -> node index
    readers = {}                   # key -> node indices since last def
    defs_version = {}              # key -> counter, for memory bases
    mem = []                       # (node, (kind, base, off, size, version))
    barrier_last = None
    by_instr = itertools.groupby(range(len(nodes)), key=lambda n: nodes[n][0])
    for _, group in by_instr:
        group = list(group)
        local = {}                 # @p written in this instruction
        writes = []
        for n in group:
            op = nodes[n][1]
            for key in op.srcs + op.preds:
                if key[0] == "lp":
                    if key in local:
                        deps.append(Dep(local[key], n, key, "raw"))
                    continue
                if key in last_def:
                    deps.append(Dep(last_def[key], n, key, "raw"))
                readers.setdefault(key, []).append(n)
            if op.mem:
                kind, base, off, size = op.mem
                acc = (kind, base, off, size, defs_version.get(base, 0))
                for m, other in mem:
                    if kind == "ld" and other[0] == "ld":
                        continue
                    ov = _overlap(acc, other)
                    if ov is False:
                        continue
                    if mode == "flow" and ov is None:
                        continue
                    deps.append(Dep(m, n, ("mem",), "mem"))
                mem.append((n, acc))
            # Barriers (window changes, wait, call) order the operations
            # that touch registers; they may share a wide instruction.
            if op.barrier or op.unit == "ct" and op.name == "call":
                for m in range(n):
                    if _uses_regs(nodes[m][1]) or nodes[m][1].barrier:
                        deps.append(Dep(m, n, ("order",), "order"))
                barrier_last = n
            elif barrier_last is not None and _uses_regs(op):
                deps.append(Dep(barrier_last, n, ("order",), "order"))
            for key in op.dsts:
                if key[0] == "lp":
                    local[key] = n
                else:
                    writes.append((n, key))
        for n, key in writes:
            if mode == "regs":
                # Anti-dependences inside one wide instruction (a register
                # read and rewritten together) are left out: with the
                # memory order they can form zero-latency cycles.
                for r in readers.get(key, []):
                    if r != n and nodes[r][0] != nodes[n][0]:
                        deps.append(Dep(r, n, key, "war"))
                if key in last_def:
                    deps.append(Dep(last_def[key], n, key, "waw"))
            last_def[key] = n
            readers[key] = []
            defs_version[key] = defs_version.get(key, 0) + 1
    if carried:
        deps += _carried(nodes, mode, rotates)
    return deps


def _uses_regs(op):
    return any(k[0] in REGKINDS for k in op.srcs + op.dsts) or bool(op.mem)


def _carried(nodes, mode, rotates):
    """Loop-carried dependences of a single-block loop."""
    deps = []
    first_def, last_def, exposed, all_reads = {}, {}, {}, {}
    defined = set()
    for n, (_, op) in enumerate(nodes):
        for key in op.srcs + op.preds:
            if key[0] == "lp":
                continue
            all_reads.setdefault(key, []).append(n)
            if key not in defined:
                exposed.setdefault(key, []).append(n)
        for key in op.dsts:
            if key[0] == "lp":
                continue
            first_def.setdefault(key, n)
            last_def[key] = n
        defined.update(k for k in op.dsts if k[0] != "lp")
    for key, uses in exposed.items():
        src, dist = None, 0
        if rotates and key[0] == "b":
            # %b[c] now was %b[c - 2d] d iterations ago.
            for d in range(1, 64):
                k = ("b", key[1] - 2 * d)
                if k[1] < 0:
                    break
                if k in last_def:
                    src, dist = last_def[k], d
                    break
        elif key in last_def:
            src, dist = last_def[key], 1
        if src is not None:
            for u in uses:
                deps.append(Dep(src, u, key, "raw", dist))
    if mode == "regs":
        for key, w in first_def.items():
            if rotates and key[0] == "b":
                continue
            for r in all_reads.get(key, []):
                if r >= w:
                    deps.append(Dep(r, w, key, "war", 1))
            deps.append(Dep(last_def[key], w, key, "waw", 1))
    # Memory across iterations: only where accesses provably overlap (the
    # same base register, not changed in the loop, overlapping offsets).
    # A compiler that pipelined the accesses knew they don't alias.
    mems = [(n, op.mem) for n, (_, op) in enumerate(nodes) if op.mem]
    for (a, ma), (b, mb) in itertools.product(mems, mems):
        if (ma[0] == "st" or mb[0] == "st") and ma[1] is not None \
                and ma[1] == mb[1] and ma[1] not in last_def \
                and ma[2] is not None and mb[2] is not None \
                and ma[2] < mb[2] + mb[3] and mb[2] < ma[2] + ma[3]:
            deps.append(Dep(a, b, ("mem",), "mem", 1))
    return deps


def dep_latency(d, nodes, chans=None):
    p, c = nodes[d.src][1], nodes[d.dst][1]
    if d.kind == "raw":
        pch = chans[d.src] if chans else p.chan
        cch = chans[d.dst] if chans else c.chan
        return latency(p, c, d.key, pch, cch)
    if d.kind == "war":
        return 0
    if d.kind == "waw":
        return 1
    if d.kind == "mem":
        return 0 if p.mem and p.mem[0] == "ld" else 1
    return 0


# --- Values in flight across blocks -----------------------------------------
#
# A schedule's "out" maps each register it last wrote to (op, channel,
# issue time relative to the end of the block, so <= 0).  The next block,
# when it has a single predecessor, starts with these in flight: padding
# at the end of one block and a stall at the start of the next cost the
# same in both measures.

def predecessors(blocks_):
    """Map block index -> indices of the blocks that can enter it."""
    index = {b.addr: k for k, b in enumerate(blocks_)}
    preds = {k: [] for k in range(len(blocks_))}
    for k, b in enumerate(blocks_):
        if b.target in index:
            preds[index[b.target]].append(k)
        last_ct = [op for op in b.instrs[-1].ops if op.unit == "ct"]
        if (not last_ct or b.falls) and k + 1 < len(blocks_):
            preds[k + 1].append(k)
    return preds


def _seed_ready(seed, key, op, ch):
    """Cycle (from the block start) a value from SEED is usable by OP."""
    p, pch, rel = seed[key]
    return rel + latency(p, op, key, pch, ch)


# --- Achieved schedule -----------------------------------------------------

def achieved(block, seed=None, iterations=1, trace=None, taken=None):
    """Simulate BLOCK as scheduled, with interlock stalls.  Returns
    (cycles of the last iteration, cycles of the first, out).  TRACE, a
    list, receives (iteration, instr, issue, stall, reason).  TAKEN says
    whether a conditional last transfer is taken; by default a back edge
    is and other exits are not."""
    if taken is None:
        taken = block.back_edge if block.cond else True
    seed = seed or {}
    t = 0
    avail = {}                     # key -> (op, chan, issue)
    fpside, intside = [], []       # recent issues, for write-back conflicts
    late = set()                   # (op, issue) whose result arrives late
    slow = {}                      # (op, issue) -> extra cycles (reload)
    last_store = []                # (base, doubleword, issue) of stores
    last_div = None                # (name, issue) of the last divide
    for k, v in seed.items():
        avail[k] = v
    starts = []
    for it in range(iterations):
        starts.append(t)
        if it and block.rotates:
            avail = {((k[0], k[1] + 2) if k[0] == "b" else k): v
                     for k, v in avail.items()}
        for ins in block.instrs:
            issue = t
            wait, why = 0, None
            for op in ins.ops:
                for key in op.srcs + op.preds:
                    if key not in avail:
                        continue
                    p, pch, pis = avail[key]
                    lat = latency(p, op, key, pch, op.chan)
                    # A conditional exit is assumed not taken (the
                    # compiler's likely path falls through); a back edge
                    # is assumed taken.
                    if key[0] == "c" and op.unit == "ct" and op.preds \
                            and not taken \
                            and p.name in model.CTP_NOT_TAKEN_PREPARES:
                        lat = min(lat, model.CTP_LAT_NOT_TAKEN)
                    r = pis + lat + slow.get((id(p), pis), 0)
                    # A write-back conflict delays an FP-side result to an
                    # integer consumer, and to an FP consumer in the other
                    # cluster (timing.md; fmuld,0 -> faddd,3: 6 -> 7).
                    if (id(p), pis) in late and key not in op.data and (
                            not model.is_fp_consumer(op.name)
                            or (cluster(pch) is not None
                                and cluster(op.chan) is not None
                                and cluster(pch) != cluster(op.chan))):
                        r += 1
                    if r > issue:
                        # A transfer waits exactly for its CTPR and its
                        # condition (control-transfer.md); other
                        # operations stall in 4-cycle steps.
                        w = r - issue if key[0] == "c" or op.unit == "ct" \
                            else model.stall(r - issue)
                        if w > wait:
                            wait, why = w, f"{key} from {p.name} ({r - issue} early)"
            # An area delivers apb_area_bytes() a cycle: more from one area
            # in one instruction holds it for the extra cycles.
            area_bytes, iset = {}, 3
            for op in ins.ops:
                iset = getattr(op, "iset", iset)
                if op.name in model.MOVA_BYTES:
                    area = next((k for k in op.srcs if k[0] == "aau"
                                 and k[1].startswith("area")), None)
                    area_bytes[area] = (area_bytes.get(area, 0)
                                        + model.MOVA_BYTES[op.name])
            extra = max([math.ceil(b / model.apb_area_bytes(iset)) - 1
                         for b in area_bytes.values()] + [0])
            if extra > wait:
                wait, why = extra, f"APB area bandwidth ({max(area_bytes.values())} bytes)"
            # The divider (ALC5): a divide 1 cycle after another waits 4,
            # an fsqrts 5 cycles after another fsqrts 2, anything during
            # an smodx's 9 cycles until they have passed.
            for op in ins.ops:
                if model.is_divider(op.name) and last_div is not None:
                    dist = issue - last_div[1]
                    w = model.DIVIDER_STALL.get(dist, 0)
                    if op.name in model.FSQRT_OPS \
                            and last_div[0] in model.FSQRT_OPS:
                        w = max(w, model.FSQRT_STALL.get(dist, 0))
                    busy = model.DIVIDER_BUSY.get(last_div[0], 0)
                    if dist < busy:
                        w = max(w, model.stall(busy - dist))
                    if w > wait:
                        wait, why = w, f"divider ({op.name} {dist} after {last_div[0]})"
            # Two stores in one instruction to the same bank (address bits
            # 6-7, base assumed 256-byte aligned) take a cycle more.
            sts = [op.mem for op in ins.ops if op.mem and op.mem[0] == "st"
                   and op.mem[1] is not None and op.mem[2] is not None]
            bank = 1 if any(a[1] == b[1] and
                            (a[2] & model.STORE_BANK_MASK)
                            == (b[2] & model.STORE_BANK_MASK)
                            for i, a in enumerate(sts) for b in sts[i + 1:]) \
                else 0
            if bank > wait:
                wait, why = bank, "two stores to one L1 bank"
            if trace is not None:
                trace.append((it, ins, issue, wait, why))
            issue += wait
            for op in ins.ops:
                # A load of the doubleword stored in the previous cycle
                # gets its value STORE_RELOAD_DELAY late (timing.md).
                if op.mem and op.mem[0] == "ld" and op.mem[1] is not None \
                        and op.mem[2] is not None:
                    dw = op.mem[2] // 8
                    if any(b == op.mem[1] and d == dw and si == issue - 1
                           for b, d, si in last_store):
                        slow[(id(op), issue)] = model.STORE_RELOAD_DELAY
                for key in op.dsts:
                    if key[0] != "lp":
                        avail[key] = (op, op.chan, issue)
                if model.CLASSES.get(op.cls, {}).get("fpside") \
                        and op.unit == "alc":
                    fpside.append((op, op.chan, issue))
                if (op.cls in model.INT_SIDE and op.unit == "alc"
                        and not (op.mem and op.mem[0] == "st")):
                    intside.append((op.chan, issue))
            last_store = [x for x in last_store if x[2] >= issue - 1]
            for op in ins.ops:
                if op.mem and op.mem[0] == "st" and op.mem[1] is not None \
                        and op.mem[2] is not None:
                    last_store.append((op.mem[1], op.mem[2] // 8, issue))
                if model.is_divider(op.name):
                    last_div = (op.name, issue)
            fpside[:] = [f for f in fpside if f[2] >= issue - 8]
            for (p, ch, pis) in fpside:
                if (ch, pis + model.PORT_DISTANCE) in intside:
                    late.add((id(p), pis))
            t = issue + ins.cycles()
        if block.ibranch and taken:
            t += 6
    starts.append(t)
    out = {k: (p, ch, i - t) for k, (p, ch, i) in avail.items()}
    return starts[-1] - starts[-2], starts[1] - starts[0], out


# --- List scheduling (possible) --------------------------------------------

def _own_latency(op):
    """How long OP's result takes, for scheduling priority."""
    if op.name == "bap":
        return model.bap_to_mova(op)
    if op.unit == "cs0":
        return model.CTP_LAT.get(op.name, 1)
    cl = model.class_of(op)
    return cl["lat"] if cl else 1


def _cs1_kind(op):
    """What an operation uses CS1 for: setwd/setbn/setbp share one
    syllable, as do the MAS fields of several channels; other uses
    (call's wbs, wait, ...) take it alone."""
    if op.name in ("setwd", "setbn", "setbp", "vfrpsz"):
        return "set"
    if op.unit == "alc" and op.cs1:
        return "mas"
    if op.cs1 or op.unit == "cs1":
        return op.name
    return None


def _pls_kind(op):
    """ELP: %pred -> @p; CLP: logic on @p; VDST: @p -> %pred."""
    if op.name == "pass" and op.srcs and op.srcs[0][0] == "p":
        return "elp"
    if op.name == "pass":
        return "vdst"
    return "clp"


class Resources:
    """What one wide instruction can hold."""

    LIMITS = {"elp": 4, "clp": 3, "vdst": 3}

    def __init__(self):
        self.alc = [False] * 6
        self.aas = [False] * 4
        self.lits = {}             # literal text -> 32-bit syllables
        self.cs0 = self.ct = 0
        self.cs1 = None
        self.pls = {"elp": 0, "clp": 0, "vdst": 0}

    def fits(self, op, ch):
        k = _cs1_kind(op)
        if k and self.cs1 and (self.cs1 != k or k not in ("set", "mas")):
            return False
        if op.unit == "alc":
            if ch is not None and self.alc[ch]:
                return False
            return self._lts(op) <= 4
        if op.unit == "aas":
            return not self.aas[ch]
        if op.unit == "cs0":
            return not self.cs0
        if op.unit == "ct":
            return not self.ct
        if op.unit == "pls":
            kind = _pls_kind(op)
            return self.pls[kind] < self.LIMITS[kind]
        return True

    def _lts(self, op):
        """Literal syllables used with OP added (identical literals are
        shared; two 16-bit literals share a syllable)."""
        lits = dict(self.lits)
        for n, text in op.lits:
            lits[text] = n
        return math.ceil(sum(lits.values()))

    def take(self, op, ch):
        if op.unit == "alc":
            if ch is not None:
                self.alc[ch] = True
            for n, text in op.lits:
                self.lits[text] = n
        elif op.unit == "aas":
            self.aas[ch] = True
        elif op.unit == "cs0":
            self.cs0 = 1
        elif op.unit == "ct":
            self.ct = 1
        elif op.unit == "pls":
            self.pls[_pls_kind(op)] += 1
        k = _cs1_kind(op)
        if k:
            self.cs1 = k


def _match(cands):
    """Assign each entry of CANDS (node -> set of channels) a distinct
    channel; return {node: channel} or None (Kuhn's algorithm)."""
    owner = {}

    def place(n, seen):
        for ch in sorted(cands[n]):
            if ch in seen:
                continue
            seen.add(ch)
            if ch not in owner or place(owner[ch], seen):
                owner[ch] = n
                return True
        return False

    for n in cands:
        if not place(n, set()):
            return None
    return {n: ch for ch, n in owner.items()}


def possible(block, dec, mode, seed=None):
    """List-schedule BLOCK's operations as well as the machine allows.
    Returns (cycles, out)."""
    seed = {k: v for k, v in (seed or {}).items()
            if not (k[0] == "c" and v[0].unit == "cs0")}
    nodes = [(i, op) for i, op in block.ops()]
    if not nodes:
        return len(block.instrs), {}
    # Reads of values from before the block.
    exposed = [[] for _ in nodes]
    defined = set()
    for n, (i, op) in enumerate(nodes):
        exposed[n] = [k for k in op.srcs + op.preds
                      if k in seed and k not in defined]
        if n + 1 == len(nodes) or nodes[n + 1][0] != i:
            defined.update(k for m, (j, o) in enumerate(nodes) if j == i
                           for k in o.dsts)
    deps = [d for d in dependences(nodes, mode)
            if not (d.key[0] == "c" and nodes[d.src][1].unit == "cs0")]
    preds = [[] for _ in nodes]
    succs = [[] for _ in nodes]
    for d in deps:
        preds[d.dst].append(d)
        succs[d.src].append(d)
    # Priority: longest path to the end (own-cluster latencies).
    height = [_own_latency(op) for _, op in nodes]
    for n in reversed(range(len(nodes))):
        for d in succs[n]:
            height[n] = max(height[n], dep_latency(d, nodes) + height[d.dst])
    order = sorted(range(len(nodes)), key=lambda n: (-height[n], n))
    allowed = [dec.allowed(op) for _, op in nodes]
    ct_nodes = [n for n, (_, op) in enumerate(nodes) if op.unit == "ct"]
    issue = [None] * len(nodes)
    chans = [None] * len(nodes)
    left = len(nodes)
    t = 0
    while left:
        res = Resources()
        alc = {}                   # node -> channels it could use now
        placed = []
        progress = True
        while progress:
            progress = False
            for n in order:
                if issue[n] is not None:
                    continue
                op = nodes[n][1]
                if op.unit == "ct" and left > len(ct_nodes):
                    continue           # the transfer goes last
                cands = sorted(allowed[n]) if allowed[n] else [None]
                ok_chs = []
                for ch in cands:
                    if not all(_seed_ready(seed, k, op, ch) <= t
                               for k in exposed[n]):
                        continue
                    chans[n] = ch
                    if all(issue[d.src] is not None
                           and issue[d.src] + dep_latency(d, nodes, chans) <= t
                           for d in preds[n]):
                        ok_chs.append(ch)
                    chans[n] = None
                if not ok_chs:
                    continue
                if op.unit == "alc":
                    trial = dict(alc)
                    trial[n] = set(ok_chs)
                    m = _match(trial) if res.fits(op, None) else None
                    if m is None:
                        continue
                    alc = trial
                    for k, ch in m.items():
                        chans[k] = ch
                    res.take(op, None)
                else:
                    ch = next((c for c in ok_chs if res.fits(op, c)), "none")
                    if ch == "none":
                        continue
                    res.take(op, ch)
                    chans[n] = ch
                issue[n] = t
                placed.append(n)
                left -= 1
                progress = True
        t += 1
        if t > 100000:
            raise RuntimeError("scheduler did not converge")
    length = max(issue) + 1
    out = {k: (v[0], v[1], v[2] - length) for k, v in seed.items()}
    for n, (_, op) in enumerate(nodes):
        for key in op.dsts:
            if key[0] != "lp":
                out[key] = (op, chans[n], issue[n] - length)
    return length, out


# --- Loop bounds -----------------------------------------------------------

def res_mii(block, dec):
    nodes = [op for _, op in block.ops()]
    alc = [dec.allowed(op) for op in nodes if op.unit == "alc"]
    bound = 1
    for r in range(1, 7):
        for subset in itertools.combinations(range(6), r):
            s = set(subset)
            n = sum(1 for a in alc if a <= s)
            bound = max(bound, math.ceil(n / r))
    lts = sum(op.lts for op in nodes if op.unit == "alc")
    aas = sum(1 for op in nodes if op.unit == "aas")
    cs0 = sum(1 for op in nodes if op.unit == "cs0")
    kinds = [k for k in map(_cs1_kind, nodes) if k]
    cs1 = len({k for k in kinds if k in ("set", "mas")}) + \
        sum(1 for k in kinds if k not in ("set", "mas"))
    pls = [_pls_kind(op) for op in nodes if op.unit == "pls"]
    plsb = max([math.ceil(pls.count(k) / n)
                for k, n in Resources.LIMITS.items()] + [0])
    return max(bound, math.ceil(lts / 4), math.ceil(aas / 4), cs0, cs1, plsb)


def rec_mii(block, mode):
    nodes = [(i, op) for i, op in block.ops()]
    deps = [d for d in dependences(nodes, mode, carried=True,
                                   rotates=block.rotates)
            if not (d.key[0] == "c" and nodes[d.src][1].unit == "cs0")]
    edges = [(d.src, d.dst, dep_latency(d, nodes), d.dist) for d in deps]
    if not any(e[3] for e in edges):
        return 1

    def feasible(ii):
        dist = [0] * len(nodes)
        for _ in range(len(nodes) + 1):
            changed = False
            for s, t, lat, k in edges:
                v = dist[s] + lat - ii * k
                if v > dist[t]:
                    dist[t] = v
                    changed = True
            if not changed:
                return True
        return False

    lo, hi = 1, max(1, sum(e[2] for e in edges))
    while lo < hi:
        mid = (lo + hi) // 2
        if feasible(mid):
            hi = mid
        else:
            lo = mid + 1
    return lo
