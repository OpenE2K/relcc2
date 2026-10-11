#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Ask lcc what __builtin_prefetch becomes (spec/isa/timing.md, "Software
prefetch"): for each installed lcc target, compile prefetches of every
read/write and locality combination with lcc -S and print the load to
%empty each one becomes (operation, channel, MAS). Also asks whether l++
knows std::hardware_destructive_interference_size.

  python3 tools/abi/probes/prefetch.py [--work DIR] [LCC ...]

Without arguments it uses every /opt/mcst/lcc-1.29.16.*/bin/lcc and the
lcc 1.31.05 wrappers in toolchains/ (with -march/-mtune variants).
"""

import argparse
import glob
import os
import re
import subprocess

SRC = """
void d0 (char *p) { __builtin_prefetch (p); }
void r0 (char *p) { __builtin_prefetch (p, 0, 0); }
void r1 (char *p) { __builtin_prefetch (p, 0, 1); }
void r2 (char *p) { __builtin_prefetch (p, 0, 2); }
void r3 (char *p) { __builtin_prefetch (p, 0, 3); }
void w0 (char *p) { __builtin_prefetch (p, 1, 0); }
void w1 (char *p) { __builtin_prefetch (p, 1, 1); }
void w2 (char *p) { __builtin_prefetch (p, 1, 2); }
void w3 (char *p) { __builtin_prefetch (p, 1, 3); }
"""
FUNCS = ["d0", "r0", "r1", "r2", "r3", "w0", "w1", "w2", "w3"]

HDI = """#include <new>
unsigned long a = std::hardware_destructive_interference_size;
"""


def prefetches(lcc, work, opts):
    c = os.path.join(work, "pf.c")
    s = os.path.join(work, "pf.s")
    with open(c, "w") as f:
        f.write(SRC)
    r = subprocess.run([lcc] + opts + ["-S", c, "-o", s],
                       capture_output=True, text=True)
    if r.returncode != 0:
        return "compile failed"
    res, fn = {}, None
    for line in open(s):
        m = re.match(r"^(\w+):$", line.strip())
        if m:
            fn = m.group(1)
        if "%empty" in line and fn:
            op = re.search(r"(ld[bhwd])(?:,(\d))?", line)
            mas = re.search(r"mas\s*=\s*(0x[0-9a-f]+)", line)
            sm = ",sm" in line
            res[fn] = "%s,%s%s mas=%s" % (op.group(1) if op else "?",
                                         op.group(2) if op else "?",
                                         ",sm" if sm else "",
                                         mas.group(1) if mas else "0")
    return "  ".join("%s %s" % (f, res.get(f, "-")) for f in FUNCS)


def hdi(lpp, work):
    c = os.path.join(work, "hdi.cc")
    with open(c, "w") as f:
        f.write(HDI)
    r = subprocess.run([lpp, "-std=c++17", "-S", c, "-o",
                        os.path.join(work, "hdi.s")],
                       capture_output=True, text=True)
    return "accepted" if r.returncode == 0 else "rejected"


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--work", default="toolchains/prefetch")
    ap.add_argument("lcc", nargs="*")
    args = ap.parse_args()
    os.makedirs(args.work, exist_ok=True)
    runs = []
    lccs = args.lcc or sorted(glob.glob("/opt/mcst/lcc-1.29.16.*/bin/lcc"))
    for lcc in lccs:
        name = re.sub(r".*lcc-1\.29\.16\.e2k-([^/]*)\.linux.*", r"1.29.16 \1",
                      lcc)
        for o in ("-O0", "-O2"):
            runs.append((name + " " + o, lcc, [o]))
    if not args.lcc:
        for w in sorted(glob.glob("toolchains/lcc-1.31.05-e2k-*/lcc")):
            name = "1.31.05 " + w.split("e2k-")[1].split("/")[0]
            runs.append((name + " -O2", w, ["-O2"]))
        w = "toolchains/lcc-1.31.05-e2k-v3/lcc"
        if os.path.exists(w):
            for m in ("elbrus-v3", "elbrus-v4", "elbrus-v5", "elbrus-v6",
                      "elbrus-v7"):
                runs.append(("1.31.05 v3 -march=" + m, w, ["-O2", "-march=" + m]))
            for t in ("elbrus-4c", "elbrus-8c", "elbrus-1c+", "elbrus-8c2",
                      "elbrus-12c", "elbrus-16c", "elbrus-2c3", "elbrus-48c",
                      "elbrus-8v7"):
                runs.append(("1.31.05 v3 -mtune=" + t, w, ["-O2", "-mtune=" + t]))
    for name, lcc, opts in runs:
        print("%-32s %s" % (name, prefetches(lcc, args.work, opts)))
    print("std::hardware_destructive_interference_size with -std=c++17:")
    for lcc in lccs:
        lpp = os.path.join(os.path.dirname(lcc), "l++")
        if os.path.exists(lpp):
            print("  %s: %s" % (lpp, hdi(lpp, args.work)))


if __name__ == "__main__":
    main()
