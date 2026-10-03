#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Weight isa-score's blocks by a run on E2K hardware and compare the
model's cycles with the measured ones.

  profile.py PROGRAM [ARGS...] --remote USER@HOST [--remote-dir DIR]
             [--period P] [--cpu N] [-o DIR] [--top N] [--json FILE]
  profile.py PROGRAM --samples FILE [--period P]   # reuse a run

Runs PROGRAM under perf on the machine, sampling two hardware monitors
(spec/isa/monitors.md) every P events: EXEC (wide instructions) on DIM0
and TICKS (cycles) on DIM1.  The monitors record the exact instruction,
so a block's EXEC samples divided by its length, times P, estimate how
often it ran, and its TICKS samples times P the cycles spent in it.
The static model (score.py) gives each block's achieved and possible
cycles per execution; weighted by the counts they predict the run.
Where measured and achieved differ, the model is wrong or the block
waited for memory (the model assumes L1 hits).  A perf stat run adds
the program's stall breakdown.
"""

import argparse
import json
import re
import shlex
import subprocess
import sys
from collections import Counter, defaultdict
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
sys.path.insert(0, str(HERE))
import analysis  # noqa: E402
import elf as elfmod  # noqa: E402
import score  # noqa: E402

EXEC, TICKS = "cpu/event=0x213/u", "cpu/event=0x372/u"
# Instructions without a taken transfer: at a branch, EXEC - NO_CT_TAKEN
# samples count the times it was taken.
NO_CT_TAKEN = "cpu/event=0x320/u"
IBRANCH_TAKEN = 6     # cycles of a taken ibranch (control-transfer.md)
# perf stat: instruction-side events, multiplexed over the two monitors.
STAT = [("TICKS", 0x572), ("EXEC", 0x513), ("BUB_E2", 0x511),
        ("BUB_B_NOP", 0x51a), ("IB_NO_COMMAND", 0x518),
        ("BUB_B_RF_SPILLFILL", 0x567)]


def run_remote(a, prog):
    ssh = ["ssh", "-i", a.key, "-o", "IdentitiesOnly=yes", a.remote]
    scp = ["scp", "-q", "-i", a.key, "-o", "IdentitiesOnly=yes"]
    d = a.remote_dir
    name = Path(prog).name
    args = " ".join(shlex.quote(x) for x in a.args)
    subprocess.run(ssh + [f"mkdir -p {d}"], check=True)
    subprocess.run(scp + [prog, f"{a.remote}:{d}/"], check=True)
    events = ",".join(f"cpu/event={c:#x}/u" for _, c in STAT)
    # The program reads --stdin (or nothing) and its output is capped: a
    # program that loops on EOF must not fill the machine's disk.
    stdin = getattr(a, "stdin", None) or ""
    cap = "| head -c 1048576"
    # -m: a small buffer, since every perf record of the account shares
    # one locked-memory quota (perf_event_mlock_kb) on a shared machine.
    cmd = (f"cd {d} && printf %s {shlex.quote(stdin)} > stdin.txt"
           f" && taskset -c {a.cpu} perf record -q -m 32"
           f" -e {EXEC},{TICKS} -c {a.period} -o perf.data"
           f" -- ./{name} {args} < stdin.txt {cap} > run.out"
           f" && perf script -i perf.data -F event,ip,dso > samples.txt"
           f" && taskset -c {a.cpu} perf record -q -m 32"
           f" -e {EXEC},{NO_CT_TAKEN} -c {a.period} -o taken.data"
           f" -- ./{name} {args} < stdin.txt {cap} > /dev/null"
           f" && perf script -i taken.data -F event,ip,dso > taken.txt"
           f" && taskset -c {a.cpu} perf stat -x, -e {events}"
           f" -o stat.txt -- ./{name} {args} < stdin.txt {cap} > /dev/null")
    subprocess.run(ssh + [cmd], check=True)
    out = Path(a.o)
    out.mkdir(parents=True, exist_ok=True)
    for f in ("samples.txt", "taken.txt", "stat.txt", "run.out"):
        subprocess.run(scp + [f"{a.remote}:{d}/{f}", str(out / f)],
                       check=True)
    return out / "samples.txt", out / "taken.txt", out / "stat.txt"


def read_samples(path, prog):
    """{event: Counter(address)} for samples in PROGRAM, and the number
    of samples elsewhere (shared libraries, the kernel's trampolines)."""
    name = Path(prog).name
    got = defaultdict(Counter)
    other = Counter()
    for line in Path(path).read_text().splitlines():
        m = re.match(r"\s*(cpu/event=0x\w+/u):\s+([0-9a-f]+)\s+\((.*)\)",
                     line)
        if not m:
            continue
        if Path(m.group(3)).name == name:
            got[m.group(1)][int(m.group(2), 16)] += 1
        else:
            other[m.group(1)] += 1
    return got, other


def read_stat(path):
    res = {}
    if not path or not Path(path).exists():
        return res
    names = {f"cpu/event={c:#x}/u": n for n, c in STAT}
    for line in Path(path).read_text().splitlines():
        f = line.split(",")
        if len(f) > 2 and f[2] in names and f[0].strip().isdigit():
            res[names[f[2]]] = int(f[0])
    return res


def weigh(prog, iset, samples, period, taken=None):
    """isa-score rows with measured fields: count (executions), measured
    (cycles), exec_samples, tick_samples, and for blocks ending in an
    ibranch, taken (times taken: from TAKEN, samples of EXEC and
    NO_CT_TAKEN from a second run, else as score.py assumes)."""
    rows = score.analyse(prog, iset)
    elf = elfmod.Elf(prog)
    block_of = {}                 # instruction address -> block address
    for fname, instrs in analysis.functions(elf, score.decoder(iset)):
        for b in analysis.blocks(fname, instrs):
            for ins in b.instrs:
                block_of[ins.addr] = b.addr
    per = defaultdict(Counter)
    lost = Counter()
    for ev, c in samples.items():
        for addr, n in c.items():
            if addr in block_of:
                per[block_of[addr]][ev] += n
            else:
                lost[ev] += n
    by_addr = {}
    for r in rows:
        s = per.get(r["addr"], Counter())
        r["exec_samples"] = s[EXEC]
        r["tick_samples"] = s[TICKS]
        r["count"] = s[EXEC] * period / r["instrs"]
        r["measured"] = s[TICKS] * period
        by_addr[r["addr"]] = r
    for r in rows:
        r["entries"] = sum(by_addr[a]["count"] for a in r.get("entry_from", [])
                           if a in by_addr)
        if taken and (r["ibranch"] or "achieved_taken" in r):
            ex, nt = taken[EXEC][r["last"]], taken[NO_CT_TAKEN][r["last"]]
            if ex:
                r["taken_frac"] = max(0, ex - nt) / ex
    return rows, lost


def taken_count(r):
    """How often block R's last transfer, an ibranch, was taken in the
    run (measured if sampled, else as score.py assumes)."""
    if not r["ibranch"]:
        return 0
    f = r.get("taken_frac", 1.0 if r["ibranch"] == "taken" else 0.0)
    return f * r["count"]


def achieved_run(r):
    """The model's cycles for block R in the run: score.py's per-block
    cycles weighted by its count.  A block ending in a conditional
    transfer is charged its taken and fall-through cycles in the measured
    proportion (a taken ibranch costs 6 cycles, a taken ct waits for its
    CTPR); a loop's ibranch back edge is charged 6 cycles per measured
    taken one."""
    if "achieved_taken" in r and "taken_frac" in r:
        f = r["taken_frac"]
        return r["count"] * (f * r["achieved_taken"]
                             + (1 - f) * r["achieved_fall"])
    c = score.total_cost(r, "achieved", r["count"], r["entries"])
    if r["loop"] and r["ibranch"] and "taken_frac" in r:
        assumed = r["count"] if r["ibranch"] == "taken" else 0
        c += IBRANCH_TAKEN * (taken_count(r) - assumed)
    return c


def report(rows, stat, other, lost, period, top, out=sys.stdout):
    p = lambda *x: print(*x, file=out)  # noqa: E731
    t = defaultdict(float)
    for r in rows:
        n, e = r["count"], r["entries"]
        for k in ("regs", "flow"):
            t[k] += score.total_cost(r, k, n, e)
        t["achieved"] += achieved_run(r)
        t["ibranch"] += taken_count(r)
        t["ops"] += r["ops"] * n
        t["exec"] += r["instrs"] * n
        t["measured"] += r["measured"]
        t["es"] += r["exec_samples"]
        t["ts"] += r["tick_samples"]
    p(f"Sampling period {period}: {t['es']:.0f} EXEC and {t['ts']:.0f} TICKS"
      " samples in the program's blocks;"
      f" elsewhere {other[EXEC] + lost[EXEC]} and"
      f" {other[TICKS] + lost[TICKS]}")
    if stat:
        p("")
        p("Whole run (perf stat, user mode, multiplexed estimates):")
        tk = stat.get("TICKS")
        for name, _ in STAT:
            if name in stat:
                v = stat[name]
                extra = ""
                if name == "BUB_E2" and tk:
                    extra = f"   = {4 * v / tk:5.1%} of cycles in interlock stalls"
                elif name in ("BUB_B_NOP", "IB_NO_COMMAND") and tk:
                    extra = f"   = {v / tk:5.1%} of cycles"
                elif name == "EXEC" and tk:
                    extra = f"   = {v / tk:.2f} instructions per cycle"
                p(f"  {name:20s} {v:15,d}{extra}")
    p("")
    p("Program's blocks, weighted by the sampled counts (cycles):")
    p(f"  measured {t['measured']:14.0f}   ILP {score.fmt_ratio(t['ops'], t['measured'])}")
    p(f"  achieved {t['achieved']:14.0f}   ILP {score.fmt_ratio(t['ops'], t['achieved'])}"
      f"   the model of the compiler's schedule (L1 hits);"
      f" measured / achieved {score.fmt_ratio(t['measured'], t['achieved'])}")
    p(f"           {t['ibranch']:14.0f} taken ibranches in it, {IBRANCH_TAKEN} cycles"
      " each")
    p(f"  possible {t['regs']:14.0f}   ILP {score.fmt_ratio(t['ops'], t['regs'])}"
      "   keeping the compiler's registers")
    p(f"  possible {t['flow']:14.0f}   ILP {score.fmt_ratio(t['ops'], t['flow'])}"
      "   true dependences only")
    p(f"  score    {score.fmt_ratio(t['regs'], t['achieved'])} (regs)"
      f"  {score.fmt_ratio(t['flow'], t['achieved'])} (flow)"
      "   = possible / achieved, weighted by the run")
    funcs = defaultdict(lambda: defaultdict(float))
    for r in rows:
        f = funcs[r["func"]]
        f["measured"] += r["measured"]
        f["achieved"] += achieved_run(r)
        f["flow"] += score.total_cost(r, "flow", r["count"], r["entries"])
        f["ops"] += r["ops"] * r["count"]
    p("")
    p(f"Functions by measured cycles (top {top}):")
    p(f"  {'function':28s} {'measured':>12s} {'achieved':>12s} {'flow':>12s}"
      f" {'meas/ach':>8s} {'ILP':>5s}")
    for name, f in sorted(funcs.items(), key=lambda kv: -kv[1]["measured"])[:top]:
        if not f["measured"]:
            break
        p(f"  {name[:28]:28s} {f['measured']:12.0f} {f['achieved']:12.0f}"
          f" {f['flow']:12.0f} {score.fmt_ratio(f['measured'], f['achieved']):>8s}"
          f" {score.fmt_ratio(f['ops'], f['measured'])}")
    p("")
    p(f"Blocks by measured cycles (top {top}); per execution (a loop: per"
      " iteration):")
    p(f"  {'address':>10s} {'function':22s} {'count':>11s} {'measured':>8s}"
      f" {'achieved':>8s} {'flow':>5s}")
    for r in sorted(rows, key=lambda r: -r["measured"])[:top]:
        if not r["count"]:
            break
        tag = " loop" if r["loop"] else ""
        p(f"  {r['addr']:#10x} {r['func'][:22]:22s} {r['count']:11.0f}"
          f" {r['measured'] / r['count']:8.1f}"
          f" {achieved_run(r) / r['count']:8.1f}"
          f" {r['flow']:5d}{tag}")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("program")
    ap.add_argument("args", nargs="*", help="the program's arguments")
    ap.add_argument("--remote")
    ap.add_argument("--remote-dir", default="relcc2/isa-score")
    ap.add_argument("--stdin", help="text for the program's standard input"
                    " (default: none)")
    ap.add_argument("--key", default=str(ROOT / "secrets" / "id_ed25519"))
    ap.add_argument("--cpu", type=int, default=2)
    ap.add_argument("--period", type=int, default=100003,
                    help="events per sample (a prime avoids aliasing with"
                    " loops; keep samples under 100,000 per second)")
    ap.add_argument("--samples", help="perf script output of an earlier run")
    ap.add_argument("--stat", help="perf stat -x, output of an earlier run")
    ap.add_argument("--taken", help="perf script output of an earlier run's"
                    " EXEC and NO_CT_TAKEN samples")
    ap.add_argument("--iset", type=int)
    ap.add_argument("-o", default=str(ROOT / "toolchains" / "isa-score"
                                      / "profile"))
    ap.add_argument("--top", type=int, default=15)
    ap.add_argument("--json", help="also write every block's numbers here")
    a = ap.parse_args()
    iset = a.iset or elfmod.Elf(a.program).iset()
    if not iset:
        ap.error("the ELF header names no ISA version; give --iset")
    if a.samples:
        spath, tpath, stpath = a.samples, a.taken, a.stat
    elif a.remote:
        spath, tpath, stpath = run_remote(a, a.program)
    else:
        ap.error("give --remote or --samples")
    samples, other = read_samples(spath, a.program)
    taken = read_samples(tpath, a.program)[0] if tpath else None
    rows, lost = weigh(a.program, iset, samples, a.period, taken)
    report(rows, read_stat(stpath), other, lost, a.period, a.top)
    if a.json:
        Path(a.json).write_text(json.dumps(rows, indent=1))


if __name__ == "__main__":
    main()
