#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Write pmu2.s, pmu2-cases.h and pmu2-events.h for pmu2.c, which counts
the hardware monitor events that pmu.c leaves out (spec/isa/monitors.md,
"The other events") on loop kernels whose counts are known.

  python3 tools/isa-probes/gen_pmu2.py   # rewrites the three files

Every kernel is  long pmu2_NAME(long n, long *p)  and runs its body n
times in a counted loop.  pmu2.c divides the count of 2n iterations
minus that of n by n and by the kernel's units (accesses, lines, pages)
per iteration, so each figure is per unit.

Registers: %dr0 count, %dr1 p, %dr3 = 1, %dr5 = 0, %pred1 false.
Kernels on a cursor buffer (pmu2.c's struct cur: base, offset, mask)
load %dr10 = base, %dr11 = offset, %dr12 = mask and store the offset
back on return, so consecutive runs keep walking the buffer.

The event table comes from MCST's perf event lists (OpenE2K/linux
tools/perf/pmu-events/arch/e2k/v{3,5,6}/Events.json): one entry per
config, with the event's name on v3, v5 and v6 (v6 moves and swaps
some codes).
"""

import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
EVENTS = HERE.parent.parent / "OpenE2K/linux/tools/perf/pmu-events/arch/e2k"


def wi(*ops):
    return "\t{\n" + "".join(f"\t  {o}\n" for o in ops) + "\t}\n"


CASES = []      # (name, n, units, buffer)
TEXT = []


def kernel(name, body, n=2000, units=1, buf="p", pre=(), post=(),
           extra=""):
    """BODY: wide instructions (lists of operations, or raw text) of one
    iteration.  '%%REC' in an operation names a recovery block that
    ibranches back to the instruction after the one holding it;
    '%%LOOP' the loop head; '%%NEXT' the instruction after the next
    ct %ctpr2; '%%FWD' the instruction after the one holding it.
    PRE: operations before the loop (with the loop's disp); POST:
    operations in the returning instruction.  EXTRA: code after the
    function's return (called routines, far blocks)."""
    lab = f".L{name}"
    out = [f"\n\t.global\tpmu2_{name}\n\t.type\tpmu2_{name}, #function\n"
           f"\t.align\t8\npmu2_{name}:\n"]
    out.append(wi("setwd\twsz = 0x20, nfx = 0x1, dbl = 0x0",
                  "return\t%ctpr3", "addd,0\t0x0, 0x1, %dr3",
                  "addd,1\t0x0, 0x0, %dr5"))
    out.append(wi(f"disp\t%ctpr1, {lab}_loop", "cmpedb,1\t%dr3, 0x0, %pred1",
                  *pre))
    out.append(wi("nop 5"))
    out.append(f"{lab}_loop:\n")
    rec = False
    nxt = 0
    for g in body:
        if isinstance(g, str):
            out.append(g.replace("%%LAB", lab))
            continue
        out.append(wi(*[o.replace("%%REC", f"{lab}_rec")
                        .replace("%%LOOP", f"{lab}_loop")
                        .replace("%%NEXT", f"{lab}_next{nxt}")
                        .replace("%%FWD", f"{lab}_fwd{nxt}")
                        .replace("%%LAB", lab) for o in g]))
        if any("%%FWD" in o for o in g):
            out.append(f"{lab}_fwd{nxt}:\n")
            nxt += 1
        if any("%%REC" in o for o in g):
            rec = True
            out.append(f"{lab}_back:\n")
        if any(o.startswith("ct\t%ctpr2") for o in g):
            out.append(f"{lab}_next{nxt}:\n")
            nxt += 1
    out.append(wi("subd,3\t%dr0, 0x1, %dr0"))
    out.append(wi("cmpedb,3\t%dr0, 0x0, %pred0"))
    out.append(wi("nop 2"))
    out.append(wi("ct\t%ctpr1 ? ~%pred0"))
    out.append(wi("ct\t%ctpr3", *post))
    if rec:
        out.append(f"{lab}_rec:\n")
        out.append(wi(f"ibranch\t{lab}_back"))
    out.append(extra.replace("%%LAB", lab))
    out.append(f"\t.size\tpmu2_{name}, .-pmu2_{name}\n")
    TEXT.append("".join(out))
    CASES.append((name, n, units, buf))


def gap(d):
    """D empty cycles as wide instructions (none for 0)."""
    out = []
    while d > 8:
        out.append(["nop 7"])
        d -= 8
    if d:
        out.append([f"nop {d - 1}" if d > 1 else "nop"])
    return out


ALS6 = [f"addd,{c}\t%dr5, %dr5, %dr{40 + c}" for c in range(6)]
CUR_PRE = ["ldd,0\t%dr1, 0x0, %dr10", "ldd,2\t%dr1, 0x8, %dr11",
           "ldd,3\t%dr1, 0x10, %dr12"]
CUR_POST = ["std,2\t%dr1, 0x8, %dr11"]


def leaf(name, wsz=4, first=(), last=()):
    """A function that returns at once (FIRST: more operations in its
    first instruction, LAST: in its returning one)."""
    return (f"\t.global\t{name}\n\t.type\t{name}, #function\n{name}:\n"
            + wi(f"setwd\twsz = {wsz:#x}, nfx = 0x1, dbl = 0x0",
                 "return\t%ctpr3", *first)
            + wi("nop 5") + wi("ct\t%ctpr3", *last))


def call(target, cond=""):
    """Call TARGET, then prepare the loop branch and the return again
    (a call empties the CTPRs)."""
    return [[f"disp\t%ctpr2, {target}"], ["nop 5"],
            [f"call\t%ctpr2, wbs = 0x1c{cond}"],
            ["disp\t%ctpr1, %%LOOP"], ["return\t%ctpr3"], ["nop 5"]]


def mem_body(op, stride, k=4, mas=None, chans=None):
    """K accesses OP (ldd or std...) STRIDE bytes apart at base +
    offset, then the offset advanced by K * STRIDE (masked).  MAS: a
    MAS for the accesses; CHANS: the channels to use."""
    m = f", mas={mas}" if mas else ""
    head = [["addd,0\t%dr10, %dr11, %dr14",
             f"addd,1\t%dr11, {k * stride:#x}, %dr11"],
            ["andd,1\t%dr11, %dr12, %dr11"]]
    if op.startswith("st"):
        chans = chans or (2, 5)
        groups = []
        for i in range(0, k, 2):
            groups.append([f"{op},{chans[j]}\t%dr14, {(i + j) * stride:#x}, %dr5{m}"
                           for j in range(min(2, k - i))])
        return head + groups
    chans = chans or (0, 2, 3, 5)
    if op.startswith("pf"):     # prefetch: a speculative byte load to %empty
        mas = op[2:]
        return head + [[f"ldb,{chans[j]},sm\t%dr14, {j * stride:#x}, %empty"
                        + (f", mas={mas}" if mas != "0x0" else "")
                        for j in range(k)]]
    return head + [[f"{op},{chans[j]}\t%dr14, {j * stride:#x}, %dr{20 + j}{m}"
                    for j in range(k)]]


KARG = "\\k + "


def tlb_off(k):
    return f"(({k}) * 4096 + ((({k}) >> 7) & 63) * 64)"


# gas macros that keep pmu2.s small: als6 is a 32-byte wide instruction
# of six ALU operations; dline SELF, TGT[, IPD] a 256-byte line (8 such
# instructions) labelled SELF whose first one also prepares %ctpr2 for
# TGT (with ipd IPD, else without SS: still 32 bytes); tlbq K / tlbs K
# four loads / two stores, one per 4 KiB page, for pages K, K+1, ...
MACROS = ("\t.macro\tals6\n" + wi(*ALS6) + "\t.endm\n"
          "\t.macro\tdline self, tgt, ipd=-1\n\\self\\():\n\t.if\t\\ipd < 0\n"
          + wi("disp\t%ctpr2, \\tgt", *ALS6[:5]) + "\t.else\n"
          + wi("disp\t%ctpr2, \\tgt; ipd \\ipd", *ALS6[:4])
          + "\t.endif\n\t.rept\t7\n\tals6\n\t.endr\n\t.endm\n"
          "\t.macro\ttlbq k\n"
          + wi(*[f"ldd,{c}\t%dr10, {tlb_off(KARG + str(j))}, %dr{20 + j}"
                 for j, c in enumerate((0, 2, 3, 5))])
          + "\t.endm\n\t.macro\ttlbs k\n"
          + wi(*[f"std,{c}\t%dr10, {tlb_off(KARG + str(j))}, %dr5"
                 for j, c in enumerate((2, 5))])
          + "\t.endm\n")


# Called from pmu2.c's C kernels.  pmu2x_apb (a, n, iters) is
# apb-loop.s's loopA: a loop_mode loop counted by %lsr that sums a[0..n-1]
# through one movad an iteration.  pmu2x_lmst (p, lsr) runs a
# one-instruction loop_mode loop under LSR with two stores to p[0], p[1]
# (loops.md: stores are dropped while pcnt > 0 and from store strmd on in
# the last iteration); both keep the caller's %lsr.
RAW = ("""
	.global	pmu2x_apb
	.type	pmu2x_apb, #function
	.align	8
pmu2x_apb:
""" + wi("setwd\twsz = 0x10, nfx = 0x1, dbl = 0x0", "return\t%ctpr3",
         "rrd,0\t%lsr, %dr11")
    + wi("ldisp\t%ctpr2, .Lx_aprog", "aaurwd,2\t%dr0, %aad0",
         "addd,0\t0x0, _f64,_lts0 0xff2000000000, %dr3",
         "addd,1\t0x0, 0x0, %dr8", "addd,3\t0x0, 0x0, %dr9",
         "addd,4\t0x0, 0x0, %dr10")
    + wi("insfd,0\t%dr3, _f32s,_lts0 0x8800, %dr1, %dr3")
    + wi("disp\t%ctpr1, .Lx_aloop", "rwd,0\t%dr3, %lsr")
    + wi("nop 7", "bap") + wi("nop 7") + wi("nop 7")
    + ".Lx_aloop:\n"
    + wi("loop_mode", "alc\talcf=1, alct=1", "ct\t%ctpr1 ? %NOT_LOOP_END",
         "movad,0\tarea=0, ind=0, am=1, be=0, %dr8",
         "addd,0\t%dr9, %dr8, %dr9", "addd,1\t%dr10, 0x1, %dr10")
    + wi("nop 7", "disp\t%ctpr2, disp=0x0")
    + wi("addd,0\t%dr9, %dr8, %dr0", "std,2\t%dr2, 0x0, %dr10")
    + wi("ct\t%ctpr3", "rwd,0\t%dr11, %lsr")
    + "\t.align\t8\n.Lx_aprog:\n"
    + wi("fapb\tct=1, dcd=0, fmt=4, mrng=8, d=0, incr=0, ind=0, asz=5, "
         "abs=0, disp=0")
    + "\t.size\tpmu2x_apb, .-pmu2x_apb\n"
    + """
	.global	pmu2x_lmst
	.type	pmu2x_lmst, #function
	.align	8
pmu2x_lmst:
""" + wi("setwd\twsz = 0x8, nfx = 0x1, dbl = 0x0", "return\t%ctpr3",
         "rrd,0\t%lsr, %dr2")
    + wi("disp\t%ctpr1, .Lx_lloop", "rwd,0\t%dr1, %lsr") + wi("nop 7")
    + ".Lx_lloop:\n"
    + wi("loop_mode", "alc\talcf=1, alct=1", "ct\t%ctpr1 ? %NOT_LOOP_END",
         "std,2\t%dr0, 0x0, %dr1", "std,5\t%dr0, 0x8, %dr1")
    + wi("rwd,0\t%dr2, %lsr") + wi("ct\t%ctpr3")
    + "\t.size\tpmu2x_lmst, .-pmu2x_lmst\n")


def main():
    # ------------------------------------------------------------ base
    kernel("empty", [])
    kernel("add8", [["addd,0\t%dr5, 0x1, %dr7"]] * 8)
    kernel("nop7", [["nop 7"]])
    kernel("ld_use", [["ldd,0\t%dr1, 0x0, %dr8"], ["addd,0\t%dr8, 0x1, %dr9"]])
    kernel("mul_use", [["muld,0\t%dr3, %dr3, %dr8"],
                       ["addd,1\t%dr8, 0x1, %dr9"]])

    # ----------------------------------------- control transfers (v6 0x527)
    kernel("ct_none", [["addd,0\t%dr5, 0x1, %dr7"]])
    kernel("ct_ib_taken", [["ibranch\t%%FWD"]])
    kernel("ct_ib_false", [["ibranch\t%%FWD ? %pred1"]])
    kernel("ct_ct_taken", [["disp\t%ctpr2, %%NEXT"], ["nop 5"], ["ct\t%ctpr2"]])
    kernel("ct_ct_ptrue", [["disp\t%ctpr2, %%NEXT"], ["nop 5"],
                           ["ct\t%ctpr2 ? ~%pred1"]])
    kernel("ct_ct_false", [["disp\t%ctpr2, %%NEXT"], ["nop 5"],
                           ["ct\t%ctpr2 ? %pred1"]])
    kernel("ct_call", call("pmu2_leaf"), extra=leaf("pmu2_leaf"))
    kernel("ct_call_false", [["disp\t%ctpr2, pmu2_leaf"], ["nop 5"],
                             ["call\t%ctpr2, wbs = 0x1c ? %pred1"]])

    # ---------------------------------------------- instruction fetch (IB)
    # Straight code in the loop (32-byte instructions: 8 a 256-byte line).
    for size in (256, 1024, 4096):
        kernel(f"ib_hot{size}", [ALS6] * (size // 32))
    # 256 KiB of straight code (twice the instruction cache), 1,024
    # lines of 256 bytes: every line misses.
    kernel("ib_cold", ["\t.rept\t8192\n\tals6\n\t.endr\n"], n=4, units=1024)
    # The same with a disp in the first instruction of every line to the
    # line 8 lines (2 KiB) ahead (the last 8 lines to the first 8).
    kernel("ib_cold_disp",
           [f"\tdline\t%%LAB_l{i}, %%LAB_l{(i + 8) % 1024}\n"
            for i in range(1024)], n=4, units=1024)
    # A disp of a cached target that is never used, with each prefetch
    # depth (ipd) and without SS; two disps.
    kernel("ib_disp_hot", [["disp\t%ctpr2, %%LOOP"]])
    for ipd in (0, 1, 2, 3):
        kernel(f"ib_disp_ipd{ipd}", [[f"disp\t%ctpr2, %%LOOP; ipd {ipd}"]])
    kernel("ib_disp_hot2", [["disp\t%ctpr2, %%LOOP"], ["nop 5"],
                            ["disp\t%ctpr2, pmu2_leaf"]])
    # Prefetch depth: 256 KiB of straight code (its pages in the
    # instruction TLB), a disp in every fourth line to the line 64 ahead
    # (16 KiB, wrapping), so that no two prepares fetch the same lines;
    # with each ipd and without SS.  Units: disps (4 lines each).
    for ipd in (None, 0, 1, 2, 3):
        tag = "none" if ipd is None else str(ipd)
        arg = "" if ipd is None else f", {ipd}"
        lines = []
        for i in range(1024):
            if i % 4:
                lines.append(f"%%LAB_l{i}:\n\t.rept\t8\n\tals6\n\t.endr\n")
            else:
                lines.append(f"\tdline\t%%LAB_l{i}, "
                             f"%%LAB_l{(i + 64) % 1024}{arg}\n")
        kernel(f"ib_depth_ipd{tag}", lines, n=4, units=256)
    # Disps (one a line, as ib_cold_disp) to 256 targets 1 KiB apart in a
    # 256 KiB region that never runs (cold), or whose 64 pages a chain of
    # taken ibranches through each page's last instruction runs first
    # (warm: in the instruction TLB, but none of the targets' lines in the
    # instruction cache).
    for tag in ("cold", "warm"):
        region = ["\t.balign\t4096\n"]
        for j in range(256):
            region.append(f".L{tag}_t{j}:\n")
            if tag == "warm" and j % 4 == 3:
                pg = j // 4
                nxt = f".Lwarm_c{pg + 1}" if pg < 63 else "%%LAB_back"
                region.append(f"\t.rept\t31\n\tals6\n\t.endr\n.Lwarm_c{pg}:\n"
                              + wi(f"ibranch\t{nxt}"))
            else:
                region.append("\t.rept\t32\n\tals6\n\t.endr\n")
        lines = ([["ibranch\t.Lwarm_c0"], "%%LAB_back:\n"]
                 if tag == "warm" else [])
        lines += [f"\tdline\t%%LAB_l{i}, .L{tag}_t{i % 256}\n"
                  for i in range(1024)]
        kernel(f"ib_near_{tag}", lines, n=4, units=1024, extra="".join(region))
    # A chain of P blocks, one per 4 KiB page, at offset (k / 8 % 16) *
    # 256 so that up to 128 of them take 128 different sets of the
    # instruction cache, linked by a taken ibranch or by disp + ct:
    # instruction TLB reach.
    for kind in ("ib", "ct"):
        for npages in ((8, 16, 32, 64, 128, 256, 384, 512) if kind == "ib"
                       else (16, 64, 256, 512)):
            name = f"ib_pages_{kind}{npages}"
            far = ["\t.text\n"]
            for k in range(npages):
                nxt = (f"%%LAB_p{k + 1}" if k + 1 < npages
                       else "%%LAB_back")
                o = (k // 8) % 16 * 256
                far.append("\t.balign\t4096\n"
                           + (f"\t.skip\t{o}\n" if o else "")
                           + f"%%LAB_p{k}:\n")
                if kind == "ib":
                    far.append(wi(f"ibranch\t{nxt}"))
                else:
                    far.append(wi(f"disp\t%ctpr2, {nxt}") + wi("nop 4")
                               + wi("ct\t%ctpr2"))
            kernel(name, [["ibranch\t%%LAB_p0"], "%%LAB_back:\n"],
                   n=200, units=npages, extra="".join(far))

    # --------------------------------------------- data side: L1, L2, MAU
    for op, stride, buf in (("ldd", 32, "l1"), ("ldd", 8, "l1"),
                            ("ldd", 32, "l2"), ("ldd", 64, "l2"),
                            ("ldd", 32, "mem"), ("ldd", 64, "mem"),
                            ("std", 32, "l1"), ("std", 8, "l1"),
                            ("std", 32, "l2"), ("std", 64, "l2"),
                            ("std", 32, "mem"), ("std", 64, "mem"),
                            ("pf0x0", 64, "mem"), ("pf0x20", 64, "mem"),
                            ("pf0x60", 64, "mem"), ("pf0x0", 32, "l2")):
        n = {"l1": 1024, "l2": 2048, "mem": 2048}[buf]
        kernel(f"{op}{stride}_{buf}", mem_body(op, stride), n=n, units=4,
               buf=buf, pre=CUR_PRE, post=CUR_POST)
    # One or two loads a wide instruction instead of four.
    for k in (1, 2):
        kernel(f"ldd32_l1_k{k}", mem_body("ldd", 32, k=k), n=1024, units=k,
               buf="l1", pre=CUR_PRE, post=CUR_POST)
    # Accesses with the cache-bypass hints (timing.md, "Cache hints").
    for op in ("ldd", "std"):
        for mas in ("0x40", "0x60"):
            kernel(f"{op}64m{mas[2:]}_mem", mem_body(op, 64, mas=mas),
                   n=2048, units=4, buf="mem", pre=CUR_PRE, post=CUR_POST)
    # One prefetch a wide instruction, in each load channel.
    for c in (0, 2, 3, 5):
        kernel(f"pf1c{c}_mem", mem_body("pf0x0", 64, k=1, chans=(c,)),
               n=2048, units=1, buf="mem", pre=CUR_PRE, post=CUR_POST)
    # A load from memory used by the next instruction.
    kernel("ldd64_mem_use", mem_body("ldd", 64, k=1)
           + [["addd,0\t%dr20, 0x1, %dr21"]], n=500, units=1, buf="mem",
           pre=CUR_PRE, post=CUR_POST)
    # Speculative loads from address 0 (an invalid page): semi-speculative
    # (,sm) and fully speculative (mas 0x3; speculation.md).
    kernel("inv_sm", [["ldd,0,sm\t%dr5, 0x0, %dr8"]])
    kernel("inv_sm3", [["ldd,0,sm\t%dr5, 0x0, %dr8, mas=0x3"]])
    # A call through a CTPR written by movtd (p[32] holds pmu2_leaf).
    kernel("call_ind", [["movtd,0\t%dr20, %ctpr2"], ["nop 7"], ["nop 1"],
                        ["call\t%ctpr2, wbs = 0x1c"],
                        ["disp\t%ctpr1, %%LOOP"], ["return\t%ctpr3"],
                        ["nop 5"]], pre=["ldd,0\t%dr1, 0x100, %dr20"])
    # Misaligned doublewords in one 32-byte line, across a line, a store.
    kernel("mis_ld_in", [["ldd,0\t%dr1, 0x4, %dr8"]])
    kernel("mis_ld_cross", [["ldd,0\t%dr1, 0x1c, %dr8"]])
    kernel("mis_st", [["std,2\t%dr1, 0x4, %dr5"]])
    kernel("mis_ldw_in", [["ldw,0\t%dr1, 0x2, %r8"]])
    kernel("mis_ldw_cross", [["ldw,0\t%dr1, 0x1e, %r8"]])
    kernel("mis_stw_in", [["stw,2\t%dr1, 0x2, %r5"]])
    kernel("mis_stw_cross", [["stw,2\t%dr1, 0x6, %r5"]])
    # DTLB reach: P loads, one per 4 KiB page, at k * 4096 + (k / 128 %
    # 64) * 64 (gas's tlbq/tlbs macros), so that up to 8,192 of them take
    # different L2 sets (in L2 after the first pass; L1 holds up to 16).
    for npages in (8, 16, 32, 64, 128, 256, 512, 640, 768, 896, 1024, 2048,
                   4096):
        body = [f"\t.set\t.Ltlbk, 0\n\t.rept\t{npages // 4}\n"
                "\ttlbq\t.Ltlbk\n\t.set\t.Ltlbk, .Ltlbk + 4\n\t.endr\n"]
        n = 200 if npages <= 512 else 50
        kernel(f"tlb{npages}", body, n=n, units=npages, buf="tlb",
               pre=CUR_PRE)
        if npages in (512, 4096):
            kernel(f"tlbh{npages}", body, n=n, units=npages, buf="tlbh",
                   pre=CUR_PRE)
    kernel("stlb4096", ["\t.set\t.Ltlbk, 0\n\t.rept\t2048\n"
                        "\ttlbs\t.Ltlbk\n\t.set\t.Ltlbk, .Ltlbk + 2\n"
                        "\t.endr\n"],
           n=50, units=4096, buf="tlb", pre=CUR_PRE)

    # ------------------------------------------------------- E0 repeats
    # getsp: allocate 64 bytes, D cycles, free them; two reads.
    for d in (0, 1, 2, 4):
        kernel(f"gsp_d{d}", [["getsp,0\t-64, %dr8"]] + gap(d)
               + [["getsp,0\t0x40, %dr9"]])
    kernel("gsp0_d0", [["getsp,0\t0x0, %dr8"], ["getsp,0\t0x0, %dr9"]])
    kernel("gsp_one", [["getsp,0\t-64, %dr8"], ["nop 7"],
                       ["getsp,0\t0x40, %dr9"], ["nop 7"]])
    # An FP compare and an integer compare into predicates D cycles apart,
    # in one channel (s) or in two (o).
    for d in (0, 1, 2, 3, 4):
        kernel(f"fcc_s_d{d}", [["fcmpeqdb,0\t%dr5, %dr5, %pred2"]] + gap(d)
               + [["cmpedb,0\t%dr3, 0x0, %pred3"]])
        kernel(f"fcc_o_d{d}", [["fcmpeqdb,0\t%dr5, %dr5, %pred2"]] + gap(d)
               + [["cmpedb,1\t%dr3, 0x0, %pred3"]])
    kernel("ccf_s_d0", [["cmpedb,0\t%dr3, 0x0, %pred3"],
                        ["fcmpeqdb,0\t%dr5, %dr5, %pred2"]])
    # An FP compare in the call's instruction or the one before it, a
    # callee whose first instruction compares.
    lc = leaf("pmu2_leaf_cmp", first=["cmpedb,0\t%dr0, 0x0, %pred0"])
    fc = "fcmpeqdb,0\t%dr5, %dr5, %pred2"
    ic = "cmpedb,1\t%dr3, 0x0, %pred2"
    for tag, before, inside in (("fc_in", [], [fc]), ("fc_before", [fc], []),
                                ("ic_in", [], [ic]), ("no", [], [])):
        kernel(f"callcmp_{tag}",
               [["disp\t%ctpr2, pmu2_leaf_cmp"], ["nop 4"],
                before if before else ["nop"],
                ["call\t%ctpr2, wbs = 0x1c", *inside],
                ["disp\t%ctpr1, %%LOOP"], ["return\t%ctpr3"], ["nop 5"]],
               extra=lc if tag == "fc_in" else "")
    # Two calls back to back.
    kernel("call2", call("pmu2_leaf")[:3] + call("pmu2_leaf"))

    # ------------------------------------------------------- E2 repeats
    # Two divisions D cycles apart.
    for d in (0, 1, 2, 4, 8, 12):
        kernel(f"div_d{d}", [["sdivd,5\t%dr3, %dr3, %dr8"]] + gap(d)
               + [["sdivd,5\t%dr3, %dr3, %dr9"]] + [["nop 7"]] * 2)
    # A combined operation whose third operand (c) or first operand (a)
    # comes from an FP add D cycles earlier, in ALC0 and ALC3.
    for ch in (0, 3):
        for d in (0, 1, 2, 3, 4, 6):
            kernel(f"comb{ch}_c_d{d}",
                   [[f"faddd,{ch}\t%dr5, %dr5, %dr20"]] + gap(d)
                   + [[f"fmul_addd,{ch}\t%dr5, %dr5, %dr20, %dr21"]]
                   + [["nop 7"]])
        for d in (0, 3):
            kernel(f"comb{ch}_a_d{d}",
                   [[f"faddd,{ch}\t%dr5, %dr5, %dr20"]] + gap(d)
                   + [[f"fmul_addd,{ch}\t%dr20, %dr5, %dr5, %dr21"]]
                   + [["nop 7"]])
    # %pfpfr: read right after an FP operation; written (with its own
    # value, read before the loop), then an FP or combined operation.
    kernel("pfpfr_rd", [["rrs,0\t%pfpfr, %r8"], ["nop 7"], ["nop 7"]])
    kernel("pfpfr_rd_fadd", [["faddd,1\t%dr5, %dr5, %dr20"],
                             ["rrs,0\t%pfpfr, %r8"], ["nop 7"], ["nop 7"]])
    kernel("pfpfr_wr", [["rws,0\t%r9, %pfpfr"], ["nop 7"], ["nop 7"]],
           pre=["rrs,0\t%pfpfr, %r9"])
    kernel("pfpfr_wr_fadd", [["rws,0\t%r9, %pfpfr"],
                             ["faddd,1\t%dr5, %dr5, %dr20"], ["nop 7"],
                             ["nop 7"]], pre=["rrs,0\t%pfpfr, %r9"])
    kernel("pfpfr_wr_comb", [["rws,0\t%r9, %pfpfr"],
                             ["fmul_addd,1\t%dr5, %dr5, %dr5, %dr20"],
                             ["nop 7"], ["nop 7"]],
           pre=["rrs,0\t%pfpfr, %r9"])
    kernel("pfpfr_wr_rd", [["rws,0\t%r9, %pfpfr"], ["rrs,0\t%pfpfr, %r8"],
                           ["nop 7"], ["nop 7"]], pre=["rrs,0\t%pfpfr, %r9"])
    kernel("pfpfr_fadd_wr", [["faddd,1\t%dr5, %dr5, %dr20"],
                             ["rws,0\t%r9, %pfpfr"], ["nop 7"], ["nop 7"]],
           pre=["rrs,0\t%pfpfr, %r9"])
    kernel("pfpfr_comb_wr", [["fmul_addd,1\t%dr5, %dr5, %dr5, %dr20"],
                             ["rws,0\t%r9, %pfpfr"], ["nop 7"], ["nop 7"]],
           pre=["rrs,0\t%pfpfr, %r9"])
    # A square root or an FP division, then an integer division D cycles
    # later (the shared divider).
    for d in (0, 1, 2, 3, 4, 5, 6):
        kernel(f"sqdiv_d{d}", [["fsqrts,5\t%r5, %r8"]] + gap(d)
               + [["sdivd,5\t%dr3, %dr3, %dr9"]] + [["nop 7"]] * 3)
        kernel(f"fdivdiv_d{d}", [["fdivd,5\t%dr5, %dr3, %dr8"]] + gap(d)
               + [["sdivd,5\t%dr3, %dr3, %dr9"]] + [["nop 7"]] * 3)
    # A callee leaves a load from memory pending into a register beyond
    # the caller's window (its %dr30) or inside it (%dr2); the caller
    # at once calls a function whose window covers that register.
    for tag, dst in (("new", "%dr30"), ("old", "%dr2")):
        name = f"pmu2_leaf_ld_{tag}"
        ext = (leaf(name, wsz=0x10, last=[f"ldd,0\t%dr0, 0x0, {dst}"])
               if tag == "new" else
               leaf(name, wsz=0x10, last=[f"ldd,0\t%dr0, 0x0, {dst}"])
               + leaf("pmu2_leaf16", wsz=0x10))
        # (A callee's setwd may not shrink the window it inherits, the
        # caller's wsz - wbs: leaf16 needs wbs >= 0x10.)
        for v, tgt, wbs in (("", "pmu2_leaf16", "0x1c"),
                            ("_w4", "pmu2_leaf", "0x1c"),
                            ("_wbs1e", "pmu2_leaf16", "0x1e"),
                            ("_nocall", None, None)):
            second = ([[f"disp\t%ctpr2, {tgt}"], ["nop 4"],
                       [f"call\t%ctpr2, wbs = {wbs}"]] if tgt else [])
            kernel(f"setwd_{tag}{v}",
                   [["addd,0\t%dr10, %dr11, %dr56",
                     "addd,1\t%dr11, 0x40, %dr11"],
                    ["andd,1\t%dr11, %dr12, %dr11"]]
                   + call(name)[:3] + second
                   + [["disp\t%ctpr1, %%LOOP"], ["return\t%ctpr3"],
                      ["nop 5"]],
                   n=1000, buf="mem", pre=CUR_PRE, post=CUR_POST,
                   extra=ext if not v else "")
    # The caller itself leaves a load from memory pending, then calls a
    # function that grows the window (leaf16) or doesn't (leaf, wsz 4).
    for tag, tgt in (("16", "pmu2_leaf16"), ("4", "pmu2_leaf")):
        kernel(f"setwd_caller{tag}",
               [["addd,0\t%dr10, %dr11, %dr14", "addd,1\t%dr11, 0x40, %dr11"],
                ["andd,1\t%dr11, %dr12, %dr11"],
                ["ldd,0\t%dr14, 0x0, %dr20"]]
               + call(tgt), n=1000, buf="mem", pre=CUR_PRE, post=CUR_POST)
    # The same with a division pending instead of a load.
    for tag, op in (("div", "sdivd,5\t%dr3, %dr3, %dr20"),
                    ("fdiv", "fdivd,5\t%dr5, %dr3, %dr20")):
        for w, tgt in (("16", "pmu2_leaf16"), ("4", "pmu2_leaf")):
            kernel(f"setwd_{tag}{w}", [[op]] + call(tgt))
    # Operations cancelled by a false predicate, by channel and kind.
    kernel("am_st_off", [["std,2\t%dr1, 0x0, %dr5 ? %pred1",
                          "std,5\t%dr1, 0x8, %dr5 ? %pred1"]])
    kernel("am_st_on", [["std,2\t%dr1, 0x0, %dr5 ? ~%pred1",
                         "std,5\t%dr1, 0x8, %dr5 ? ~%pred1"]])
    kernel("am_ld25_off", [["ldd,2\t%dr1, 0x0, %dr8 ? %pred1",
                            "ldd,5\t%dr1, 0x8, %dr9 ? %pred1"]])
    kernel("am_ld03_off", [["ldd,0\t%dr1, 0x0, %dr8 ? %pred1",
                            "ldd,3\t%dr1, 0x8, %dr9 ? %pred1"]])
    kernel("am_alu25_off", [["addd,2\t%dr5, 0x1, %dr8 ? %pred1",
                             "addd,5\t%dr5, 0x1, %dr9 ? %pred1"]])
    kernel("am_alu4_off", [[f"addd,{c}\t%dr5, 0x1, %dr{8 + c} ? %pred1"
                             for c in (0, 1, 3, 4)]])
    kernel("am_alu6_off", [[f"addd,{c}\t%dr5, 0x1, %dr{8 + c} ? %pred1"
                             for c in range(6)]])
    # set_mark sets the header's sw bit (bundle.md).
    kernel("sw_mark", [["set_mark"]])
    kernel("sw_mark_add", [["set_mark", "addd,0\t%dr5, 0x1, %dr7"]])
    kernel("sw_none", [["nop"]])
    # lcc's fetch-add (atomics.md) on p[0].
    kernel("atom_add", [["nop 1"],
                        ["ldd,0\t%dr1, 0x0, %dr8, mas=0x7", "nop 4"],
                        ["addd,0\t%dr8, 0x1, %dr9"],
                        ["std,2\t%dr1, 0x0, %dr9, mas=0x2", "rbranch\t%%REC"]])
    # DAM: a lock load, then the check with a store in its instruction to
    # the same address, the next doubleword, or 4 KiB on (the same low 12
    # bits); and the 4 KiB store one instruction before the check.
    lock = [["ldd,0,sm\t%dr1, 0x0, %dr8, mas=0x4"], ["nop 2"]]
    for tag, off in (("same", 0), ("next", 8), ("4k", 0x1000)):
        kernel(f"dam_st_{tag}", lock + [[
            "ldd,5\t%dr1, 0x0, %dr8, mas=0x3", f"std,2\t%dr1, {off:#x}, %dr5",
            "rbranch\t%%REC"], ["nop 2"]], buf="l1d")
    kernel("dam_st_4k_before", lock + [[f"std,2\t%dr1, 0x1000, %dr5"], [
        "ldd,5\t%dr1, 0x0, %dr8, mas=0x3", "rbranch\t%%REC"], ["nop 2"]],
        buf="l1d")
    kernel("dam_ok", lock + [["ldd,5\t%dr1, 0x0, %dr8, mas=0x3",
                              "rbranch\t%%REC"], ["nop 2"]], buf="l1d")

    TEXT.append(RAW)
    hdr = ("! SPDX-License-Identifier: GPL-3.0-or-later\n"
           "! Generated by tools/isa-probes/gen_pmu2.py; see pmu2.c.\n"
           "\t.ignore\tld_st_style\n\t.text\n" + MACROS)
    (HERE / "pmu2.s").write_text(hdr + "".join(TEXT))
    (HERE / "pmu2-cases.h").write_text(
        "/* SPDX-License-Identifier: GPL-3.0-or-later */\n"
        "/* Generated by tools/isa-probes/gen_pmu2.py.  */\n"
        + "".join(f"CASE ({n}, {it}, {u}, {b})\n" for n, it, u, b in CASES))
    write_events()


def write_events():
    """pmu2-events.h: { config, v3 name, v5 name, v6 name } for every
    core event of the v3, v5 and v6 lists.  An event on either
    instruction monitor (2,3:EE) is pinned to DIM1 (0x3EE), as pmu.c
    does; one on either data monitor keeps 0x4EE."""
    names = {}
    for v in (3, 5, 6):
        for e in json.loads((EVENTS / f"v{v}" / "Events.json").read_text()):
            code = int(e["EventCode"], 16)
            if code >> 8 == 5:
                code = 0x300 | (code & 0xff)
            names.setdefault(code, {})[v] = e["EventName"]
    out = ["/* SPDX-License-Identifier: GPL-3.0-or-later */\n"
           "/* Generated by tools/isa-probes/gen_pmu2.py from MCST's perf\n"
           "   event lists (tools/perf/pmu-events/arch/e2k/v{3,5,6}).  */\n"]
    # Accepted by the kernel (perf_event.c, hw_raw_event_to_iset) but
    # in no list: DIM 0x14, DDM 0x70-0x72.
    for code in (0x314, 0x070, 0x071, 0x072, 0x170, 0x171, 0x172):
        names.setdefault(code, {3: "undoc", 5: "undoc", 6: "undoc"})
    for code in sorted(names, key=lambda c: (c & 0xff, c >> 8)):
        n = names[code]
        out.append(f"EV ({code:#05x}, \"{n.get(3, '-')}\", "
                   f"\"{n.get(5, '-')}\", \"{n.get(6, '-')}\")\n")
    (HERE / "pmu2-events.h").write_text("".join(out))


if __name__ == "__main__":
    main()
