# SPDX-License-Identifier: GPL-3.0-or-later
"""gdb script for validate.py: run on the E2K machine, it single-steps each
kernel once and writes how often each of its wide instructions executed.

  gdb -batch -x count.py --args ./validate count

The kernels and their address ranges come from kernels.json (written by
validate.py from the ELF symbols); the counts go to counts.json.
"""

import json
from collections import Counter

import gdb

ranges = json.load(open("kernels.json"))
counts = Counter()
gdb.execute("set pagination off")
gdb.execute("set confirm off")
for name in ranges:
    gdb.execute(f"break *{ranges[name][0]:#x}", to_string=True)
gdb.execute("run", to_string=True)
while True:
    try:
        pc = int(gdb.selected_frame().pc())
    except gdb.error:
        break                          # the program exited
    name = next((n for n, (lo, hi) in ranges.items() if lo <= pc < hi), None)
    if name is None:
        try:
            gdb.execute("continue", to_string=True)
        except gdb.error:
            break
        continue
    lo, hi = ranges[name]
    while lo <= pc < hi:
        counts[pc] += 1
        gdb.execute("stepi", to_string=True)
        pc = int(gdb.selected_frame().pc())
    try:
        gdb.execute("continue", to_string=True)
    except gdb.error:
        break
json.dump({f"{a:#x}": c for a, c in counts.items()}, open("counts.json", "w"))
