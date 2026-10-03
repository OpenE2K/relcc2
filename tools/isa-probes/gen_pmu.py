#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Write pmu.s: loop kernels with known behaviour for pmu.c, which counts
hardware monitor events (spec/isa/monitors.md) while they run.

  python3 tools/isa-probes/gen_pmu.py      # rewrites pmu.s and pmu-cases.h

Every kernel is  long pmu_NAME(long n, long *p)  and runs its body n
times in a counted loop.  pmu.c subtracts the counts of the empty kernel
and divides by n, so each figure is per execution of the body.

Registers: %dr0 count, %dr1 p (an 8-aligned buffer of 64 longs, in L1),
%dr3 = 1, %dr5 = 0, %pred1 false.
"""

from pathlib import Path

HERE = Path(__file__).resolve().parent


def kernel(name, body, extra=""):
    """BODY: wide instructions (lists of operations) of one iteration.
    '%%REC' in an operation names a recovery block that branches back
    to the instruction after the one holding it; '%%LOOP' names the
    loop head; '%%NEXT' the instruction after the next ct %ctpr2;
    '%%FWD' the instruction after the one holding it.
    EXTRA: code placed after the function's return (called routines)."""
    lab = f".L{name}"
    out = [f"\n\t.global\tpmu_{name}\n\t.type\tpmu_{name}, #function\n"
           f"\t.align\t8\npmu_{name}:\n"]

    def wi(*ops):
        out.append("\t{\n" + "".join(f"\t  {o}\n" for o in ops) + "\t}\n")

    wi("setwd\twsz = 0x20, nfx = 0x1, dbl = 0x0", "return\t%ctpr3",
       "addd,0\t0x0, 0x1, %dr3", "addd,1\t0x0, 0x0, %dr5")
    wi(f"disp\t%ctpr1, {lab}_loop", "cmpedb,0\t%dr3, 0x0, %pred1")
    wi("nop 5")
    out.append(f"{lab}_loop:\n")
    rec = False
    nxt = 0
    for g in body:
        wi(*[o.replace("%%REC", f"{lab}_rec").replace("%%LOOP", f"{lab}_loop")
             .replace("%%NEXT", f"{lab}_next{nxt}")
             .replace("%%FWD", f"{lab}_fwd{nxt}") for o in g])
        if any("%%FWD" in o for o in g):
            out.append(f"{lab}_fwd{nxt}:\n")
            nxt += 1
        if any("%%REC" in o for o in g):
            rec = True
            out.append(f"{lab}_back:\n")
        if any(o.startswith("ct\t%ctpr2") for o in g):
            out.append(f"{lab}_next{nxt}:\n")
            nxt += 1
    wi("subd,3\t%dr0, 0x1, %dr0")
    wi("cmpedb,3\t%dr0, 0x0, %pred0")
    wi("nop 2")
    wi("ct\t%ctpr1 ? ~%pred0")
    wi("ct\t%ctpr3")
    if rec:
        out.append(f"{lab}_rec:\n")
        wi(f"ibranch\t{lab}_back")
    out.append(extra)
    out.append(f"\t.size\tpmu_{name}, .-pmu_{name}\n")
    return "".join(out)


def main():
    k = []

    def add(name, body, extra=""):
        k.append((name, kernel(name, body, extra)))

    add("empty", [])
    # 8 independent instructions: counts per instruction and per cycle.
    add("add8", [["addd,0\t%dr5, 0x1, %dr7"]] * 8)
    # One instruction that waits 7 cycles.
    add("nop7", [["nop 7"]])
    # A load used in the next instruction (2 cycles early: one 4-cycle
    # stall step, timing.md), and a multiply used in the next one.
    add("ld_use", [["ldd,0\t%dr1, 0x0, %dr8"], ["addd,0\t%dr8, 0x1, %dr9"]])
    add("ld_use_far", [["ldd,0\t%dr1, 0x0, %dr8"], ["nop 2"],
                       ["addd,0\t%dr8, 0x1, %dr9"]])
    add("mul_use", [["muld,0\t%dr3, %dr3, %dr8"], ["addd,1\t%dr8, 0x1, %dr9"]])
    # Write-back conflict (timing.md): an integer op in the multiply's
    # channel 2 cycles after it; the consumer far enough not to stall.
    far = [["nop 7"], ["nop 3"]]
    add("wb_conf", [["muld,0\t%dr3, %dr3, %dr8"], ["nop"],
                    ["addd,0\t%dr5, %dr5, %dr7"]] + far
        + [["addd,1\t%dr8, 0x1, %dr9"]])
    add("wb_other", [["muld,0\t%dr3, %dr3, %dr8"], ["nop"],
                     ["addd,2\t%dr5, %dr5, %dr7"]] + far
        + [["addd,1\t%dr8, 0x1, %dr9"]])
    # Same, with the conflict timed to stall the consumer: the consumer at
    # the multiply's latency (+ the conflict's cycle).
    add("wb_conf_use", [["muld,0\t%dr3, %dr3, %dr8"], ["nop"],
                        ["addd,0\t%dr5, %dr5, %dr7"], ["nop"],
                        ["addd,1\t%dr8, 0x1, %dr9"]])
    add("wb_other_use", [["muld,0\t%dr3, %dr3, %dr8"], ["nop"],
                         ["addd,2\t%dr5, %dr5, %dr7"], ["nop"],
                         ["addd,1\t%dr8, 0x1, %dr9"]])
    # Four operations predicated off, and four on.
    add("pred_off", [[f"addd,{c}\t%dr5, 0x1, %dr{8 + c} ? %pred1"
                      for c in range(4)]])
    add("pred_on", [[f"addd,{c}\t%dr5, 0x1, %dr{8 + c} ? ~%pred1"
                     for c in range(4)]])
    add("pred_off1", [["addd,0\t%dr5, 0x1, %dr8 ? %pred1"]])
    add("pred_off6", [[f"addd,{c}\t%dr5, 0x1, %dr{8 + c} ? %pred1"
                       for c in range(6)]])
    add("pred_on6", [[f"addd,{c}\t%dr5, 0x1, %dr{8 + c} ? ~%pred1"
                      for c in range(6)]])
    add("add6", [[f"addd,{c}\t%dr5, 0x1, %dr{8 + c}" for c in range(6)]])
    add("nop1", [["nop"]])
    # Data speculation: lock load, then a check with rbranch.
    lock = [["ldd,0,sm\t%dr1, 0x0, %dr8, mas=0x4"], ["nop 2"]]
    check = [["ldd,5\t%dr1, 0x0, %dr8, mas=0x3", "rbranch\t%%REC"], ["nop 2"]]
    add("dam_ok", lock + check)
    add("dam_store", lock + [["std,2\t%dr1, 0x0, %dr5"], ["nop 2"]] + check)
    add("dam_store_other", lock + [["std,2\t%dr1, 0x8, %dr5"], ["nop 2"]]
        + check)
    # 48 lock loads into %dr16..%dr63, then a check of the first.
    cap = [[f"ldd,{c},sm\t%dr1, {8 * (i + j):#x}, %dr{16 + i + j}, mas=0x4"
            for j, c in enumerate((0, 3))] for i in range(0, 48, 2)]
    add("dam_48", cap + [["nop 2"]] + [[
        "ldd,5\t%dr1, 0x0, %dr16, mas=0x3", "rbranch\t%%REC"], ["nop 2"]])
    # Capacity: the table cleared, N lock loads, a check of the first.
    inv = [["mmurw,2\t%dr5, %dam_inv"], ["nop 2"]]
    for n in (16, 24, 28, 30, 31, 32, 33, 34, 36, 40, 48):
        locks = [[f"ldd,{c},sm\t%dr1, {8 * (i + j):#x}, %dr{16 + i + j}, "
                  "mas=0x4" for j, c in enumerate((0, 3)) if i + j < n]
                 for i in range(0, n, 2)]
        add(f"dam_cap{n}", inv + locks + [["nop 2"]] + [[
            "ldd,5\t%dr1, 0x0, %dr16, mas=0x3", "rbranch\t%%REC"],
            ["nop 2"]])
    # A lock, a call DEPTH deep (pmu_deep in pmu.c; 64 spills the
    # register file and the chain stack), then the check.  wbs = 0x1c
    # makes the caller's %dr56/%dr57 the callee's %dr0/%dr1.
    for depth in (2, 64):
        add(f"dam_deep{depth}", inv + lock + [
            [f"addd,0\t0x0, {depth:#x}, %dr56", "addd,1\t0x0, 0x0, %dr57",
             "disp\t%ctpr2, pmu_deep"], ["nop 5"],
            ["call\t%ctpr2, wbs = 0x1c"],
            ["disp\t%ctpr1, %%LOOP"], ["return\t%ctpr3"], ["nop 5"]] + check)
    # %dt_al: a fully speculative load from NULL, checked in ALC1.
    add("dt_al", [["ldd,2,sm\t%dr5, 0x0, %dr8, mas=0x3"], ["nop 2"],
                  ["rbranch\t%%REC ? %dt_al1", "addd,1,sm\t%dr8, 0x0, %dr9"],
                  ["nop 2"]])
    add("dt_al_ok", [["ldd,2,sm\t%dr1, 0x0, %dr8, mas=0x3"], ["nop 2"],
                     ["rbranch\t%%REC ? %dt_al1", "addd,1,sm\t%dr8, 0x0, %dr9"],
                     ["nop 2"]])
    # A conditional ct soon after its disp: does it wait for the CTPR
    # when the condition is false?  And when true (it goes to the next
    # instruction)?
    # ct_dK: K cycles of nop between them (the ct issues K + 1 cycles after
    # the disp; 4 is late enough).
    for cond, tag in (("%pred1", "false"), ("~%pred1", "true")):
        for d in range(5):
            gap = [[f"nop {d - 1}" if d > 1 else "nop"]] if d else []
            add(f"ct_{tag}_d{d}", [["disp\t%ctpr2, %%NEXT"]] + gap
                + [[f"ct\t%ctpr2 ? {cond}"]])
    # A compare D cycles before the ct it conditions (the CTPR prepared
    # well before): %pred2 = (1 == 0) false, %pred3 = (1 == 1) true.
    for cond, tag in (("%pred2", "false"), ("%pred3", "true")):
        for d in range(5):
            gap = [[f"nop {d - 1}" if d > 1 else "nop"]] if d else []
            add(f"cmpct_{tag}_d{d}", [["disp\t%ctpr2, %%NEXT"], ["nop 5"],
                                      ["cmpedb,0\t%dr3, 0x0, %pred2",
                                       "cmpedb,1\t%dr3, 0x1, %pred3"]] + gap
                + [[f"ct\t%ctpr2 ? {cond}"]])
    # Latencies by a sweep: producer, D cycles of nop, consumer.  The
    # stall is TICKS minus the instructions' own cycles.
    lat = {
        "getsp": ("getsp,0\t0x0, %dr8", "addd,0\t%dr8, 0x1, %dr9"),
        "movif": ("movif,0\t%dr3, %r3, %dr8", "addd,0\t%dr8, 0x1, %dr9"),
        "movif_movfi": ("movif,0\t%dr3, %r3, %dr8", "movfi,0\t%dr8, %r9"),
        "movfi": ("movfi,0\t%dr8, %r8", "addd,0\t%dr8, 0x1, %dr9"),
    }
    for name, (prod, cons) in lat.items():
        for d in range(7):
            gap = [[f"nop {d - 1}" if d > 1 else "nop"]] if d else []
            add(f"lat_{name}_d{d}", [[prod]] + gap + [[cons]])
    # A taken forward ibranch to the next instruction.
    add("ibranch_fwd", [["ibranch\t%%FWD"]])
    # A stall (a load used 1 instruction later) just before a taken
    # ibranch, a prepared ct, or nothing.
    ld = [["ldd,0\t%dr1, 0x0, %dr8"]]
    add("stall_none", ld + [["addd,0\t%dr8, 0x1, %dr9"]])
    add("stall_ibranch", ld + [["addd,0\t%dr8, 0x1, %dr9", "ibranch\t%%FWD"]])
    add("stall_ct", [["disp\t%ctpr2, %%NEXT"], ["nop 5"]] + ld
        + [["addd,0\t%dr8, 0x1, %dr9", "ct\t%ctpr2"]])
    add("nostall_ibranch", ld + [["nop 2"],
                                 ["addd,0\t%dr8, 0x1, %dr9", "ibranch\t%%FWD"]])
    # Two taken ibranches (to a block that branches back).
    add("ibranch2", [["ibranch\t%%REC"]])
    # A call to a function that returns at once.  The call empties the
    # CTPRs, so the loop branch and the return are prepared again.
    leaf = f"""\t.type\tpmu_leaf, #function
pmu_leaf:
\t{{
\t  setwd\twsz = 0x4, nfx = 0x1, dbl = 0x0
\t  return\t%ctpr3
\t}}
\t{{
\t  nop 5
\t}}
\t{{
\t  ct\t%ctpr3
\t}}
"""
    add("call", [["disp\t%ctpr2, pmu_leaf"], ["nop 5"],
                 ["call\t%ctpr2, wbs = 0x1c"],
                 ["disp\t%ctpr1, %%LOOP"], ["return\t%ctpr3"], ["nop 5"]],
        leaf)
    hdr = """! SPDX-License-Identifier: GPL-3.0-or-later
! Generated by tools/isa-probes/gen_pmu.py; see pmu.c.
\t.ignore\tld_st_style
\t.text
"""
    (HERE / "pmu.s").write_text(hdr + "".join(t for _, t in k))
    (HERE / "pmu-cases.h").write_text(
        "/* SPDX-License-Identifier: GPL-3.0-or-later */\n"
        "/* Generated by tools/isa-probes/gen_pmu.py.  */\n"
        + "".join(f"CASE ({n})\n" for n, _ in k))


if __name__ == "__main__":
    main()
