#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Score how much of the parallelism E2K allows a compiled program uses.

  score.py PROGRAM [--iset N] [--counts FILE] [--function NAME] [--top N]
           [--json FILE]

For every basic block it compares the cycles of the compiler's schedule
("achieved") with the cycles of the best schedule of the same operations
on ISA version N ("possible"), and for single-block loops the initiation
intervals.  Memory is assumed to hit L1, as compilers schedule it.  See
README.md for the model.

Without --counts every block counts once.  With a counts file from
count.sh (executions per instruction address, from a qemu-e2k run) the
blocks are weighted by how often they ran.
"""

import argparse
import functools
import json
import sys
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import analysis  # noqa: E402
import elf as elfmod  # noqa: E402
import model  # noqa: E402


def load_counts(path, elf, bias=0):
    """Executions per instruction address from a count.sh file: each line
    is a translation block's start and its instructions' counts, which
    are assigned by walking the wide instruction sizes in ELF."""
    counts = defaultdict(int)
    for line in open(path):
        parts = line.split()
        if len(parts) < 2:
            continue
        addr = int(parts[0], 16) - bias
        for c in map(int, parts[1:]):
            counts[addr] += c
            data = sec = None
            for sec in elf.sections:
                if sec.addr <= addr < sec.addr + len(sec.data):
                    data = sec.data
                    break
            if data is None:
                break
            addr += (((data[addr - sec.addr] >> 4) & 7) + 1) * 8
    return counts


@functools.lru_cache(maxsize=None)
def decoder(iset):
    return model.Decoder(iset)


def analyse(path, iset, counts, only):
    elf = elfmod.Elf(path)
    dec = decoder(iset)
    rows = []
    for fname, instrs in analysis.functions(elf, dec):
        if only and fname not in only:
            continue
        blocks = analysis.blocks(fname, instrs)
        preds = analysis.predecessors(blocks)
        outs = []                    # per block: {model: out}
        for k, b in enumerate(blocks):
            ops = [op for _, op in b.ops()]
            entry = [j for j in preds[k] if j != k]
            single = entry[0] if len(entry) == 1 and entry[0] < k else None

            def seed(m):
                return outs[single][m] if single is not None else None

            row = dict(func=fname, addr=b.addr, instrs=len(b.instrs),
                       ops=len(ops), loop=b.succ_self,
                       unknown=sorted({op.name for op in ops if not op.known
                                       and op.unit == "alc"}))
            if counts is not None:
                row["count"] = counts.get(b.addr, 0)
                row["entries"] = sum(counts.get(blocks[j].addr, 0)
                                     for j in entry) if b.succ_self else 0
            out = {}
            if b.succ_self:
                ii, first, out["achieved"] = analysis.achieved(
                    b, seed("achieved"), iterations=3)
                row["achieved"] = ii
                row["entry_achieved"] = max(0, first - ii)
                resmii = analysis.res_mii(b, dec)
                row["resmii"] = resmii
                for m in ("regs", "flow"):
                    row[m] = min(ii, max(resmii, analysis.rec_mii(b, m)))
                    one, out[m] = analysis.possible(b, dec, m, seed(m))
                    alone, _ = analysis.possible(b, dec, m)
                    row["entry_" + m] = max(0, one - alone)
            else:
                row["achieved"], _, out["achieved"] = analysis.achieved(
                    b, seed("achieved"))
                for m in ("regs", "flow"):
                    row[m], out[m] = analysis.possible(b, dec, m, seed(m))
                    # The compiler's schedule is one valid schedule of the
                    # same operations; the list scheduler is greedy.
                    own, _, own_out = analysis.achieved(b, seed(m))
                    if own < row[m]:
                        row[m], out[m] = own, own_out
            outs.append(out)
            rows.append(row)
    return rows


def weight(row, dynamic):
    return row.get("count", 0) if dynamic else 1


def cost(row, which):
    """Cycles of a block in measure WHICH per execution; for a loop block
    one iteration (the initiation interval)."""
    return row[which]


def total_cost(row, which, dynamic):
    """Cycles of a block in measure WHICH over the whole run (or once,
    statically), including a loop's one-time entry cost."""
    c = weight(row, dynamic) * row[which]
    if row["loop"]:
        c += row["entry_" + which] * (row.get("entries", 0) if dynamic else 1)
    return c


def totals(rows, dynamic):
    t = defaultdict(float)
    for r in rows:
        for k in ("achieved", "regs", "flow"):
            t[k] += total_cost(r, k, dynamic)
        t["ops"] += weight(r, dynamic) * r["ops"]
    return t


def fmt_ratio(a, b):
    return f"{a / b:5.2f}" if b else "    -"


def report(rows, iset, dynamic, top, out):
    p = lambda *a: print(*a, file=out)  # noqa: E731
    p(f"ISA version: elbrus-v{iset}"
      + ("" if iset in model.MEASURED_ISETS else
         " (timing measured on v3 only; v3 latencies used)"))
    p("Weighting: " + ("executions from the counts file" if dynamic
                       else "static, every block once"))
    unknown = sorted({u for r in rows for u in r["unknown"]})
    if unknown:
        p("Operations not in the opcode table (1-cycle integer assumed): "
          + ", ".join(unknown))
    t = totals(rows, dynamic)
    p("")
    p("Whole program (cycles; ILP = operations per cycle):")
    p(f"  achieved {t['achieved']:14.0f}   ILP {fmt_ratio(t['ops'], t['achieved'])}")
    p(f"  possible {t['regs']:14.0f}   ILP {fmt_ratio(t['ops'], t['regs'])}"
      "   keeping the compiler's registers")
    p(f"  possible {t['flow']:14.0f}   ILP {fmt_ratio(t['ops'], t['flow'])}"
      "   true dependences only")
    p(f"  score    {fmt_ratio(t['regs'], t['achieved'])} (regs)"
      f"  {fmt_ratio(t['flow'], t['achieved'])} (flow)"
      "   = possible / achieved; 1.00 means no cycle left to win")
    # Per function.
    funcs = defaultdict(list)
    for r in rows:
        funcs[r["func"]].append(r)
    ft = sorted(((totals(rs, dynamic), name) for name, rs in funcs.items()),
                key=lambda x: -(x[0]["achieved"] - x[0]["flow"]))
    p("")
    p(f"Functions by cycles to win (top {top}):")
    p(f"  {'function':32s} {'achieved':>12s} {'regs':>12s} {'flow':>12s}"
      f" {'score':>6s}")
    for t, name in ft[:top]:
        if not t["achieved"]:
            continue
        p(f"  {name[:32]:32s} {t['achieved']:12.0f} {t['regs']:12.0f}"
          f" {t['flow']:12.0f} {fmt_ratio(t['flow'], t['achieved'])}")
    # Hot blocks and loops.
    hot = sorted(rows, key=lambda r: -(total_cost(r, "achieved", dynamic)
                                       - total_cost(r, "flow", dynamic)))
    p("")
    p(f"Blocks by cycles to win (top {top}); loops show the initiation"
      " interval:")
    p(f"  {'address':>10s} {'function':24s} {'count':>10s} {'ops':>4s}"
      f" {'achieved':>8s} {'regs':>5s} {'flow':>5s}")
    for r in hot[:top]:
        if total_cost(r, "achieved", dynamic) <= total_cost(r, "flow", dynamic):
            continue
        tag = " loop" if r["loop"] else ""
        extra = f"  (ResMII {r['resmii']})" if r["loop"] else ""
        p(f"  {r['addr']:#10x} {r['func'][:24]:24s}"
          f" {str(r.get('count', '-')):>10s} {r['ops']:4d}"
          f" {cost(r, 'achieved'):8d} {cost(r, 'regs'):5d}"
          f" {cost(r, 'flow'):5d}{tag}{extra}")


def explain(path, iset, addrs):
    """Print the achieved simulation of the blocks at ADDRS."""
    elf = elfmod.Elf(path)
    dec = decoder(iset)
    for fname, instrs in analysis.functions(elf, dec):
        for b in analysis.blocks(fname, instrs):
            if b.addr not in addrs:
                continue
            trace = []
            analysis.achieved(b, iterations=2 if b.succ_self else 1,
                              trace=trace)
            print(f"{fname} block {b.addr:#x}"
                  + (" (loop, 2 iterations)" if b.succ_self else ""))
            for it, ins, issue, wait, why in trace:
                stall = f"  stall {wait}: {why}" if wait else ""
                print(f"  [{it}] {ins.addr:#x} t={issue:<4d} nop {ins.nop}{stall}")
                for line in ins.text:
                    print(f"        {line}")
            for m in ("regs", "flow"):
                print(f"  possible ({m}): {analysis.possible(b, dec, m)[0]}"
                      " cycles for one pass")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("program")
    ap.add_argument("--iset", type=int,
                    help="ISA version (default: from the ELF header)")
    ap.add_argument("--counts", help="executions per address (count.sh)")
    ap.add_argument("--bias", type=lambda s: int(s, 0), default=0,
                    help="load address of a PIE, subtracted from counts")
    ap.add_argument("--function", action="append",
                    help="only these functions (repeatable)")
    ap.add_argument("--top", type=int, default=15)
    ap.add_argument("--json", help="also write every block's numbers here")
    ap.add_argument("--explain", type=lambda v: int(v, 0), action="append",
                    help="print the simulated schedule of the block at "
                    "this address (repeatable)")
    a = ap.parse_args()
    iset = a.iset or elfmod.Elf(a.program).iset()
    if not iset:
        ap.error("the ELF header names no ISA version; give --iset")
    counts = load_counts(a.counts, elfmod.Elf(a.program), a.bias) \
        if a.counts else None
    rows = analyse(a.program, iset, counts, a.function)
    if a.explain:
        explain(a.program, iset, a.explain)
        return
    report(rows, iset, counts is not None, a.top, sys.stdout)
    if a.json:
        Path(a.json).write_text(json.dumps(rows, indent=1))


if __name__ == "__main__":
    main()
