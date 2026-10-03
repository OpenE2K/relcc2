#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Compare isa-score's "achieved" cycles with cycles measured on hardware.

  validate.py [--remote USER@HOST] [--remote-dir DIR] [--key FILE] [-o DIR]

Builds kernels.c with lcc at -O0, -O2 and -O3 (v3) and main.c, counts
executions under qemu-e2k (count.sh), predicts each kernel's cycles per
call from the counts, and, with --remote, runs the program on an E2K
machine over SSH and prints both.  The data fits in L1, so the two
should agree as far as the model is right.
"""

import argparse
import json
import re
import subprocess
import sys
from collections import defaultdict
from pathlib import Path

HERE = Path(__file__).resolve().parent
SCORE = HERE.parent
ROOT = SCORE.parents[1]
LCC = "/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc"
sys.path.insert(0, str(SCORE))
import score  # noqa: E402


def sh(*cmd, **kw):
    return subprocess.run(cmd, check=True, **kw)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--remote")
    ap.add_argument("--remote-dir", default="relcc2/isa-score")
    ap.add_argument("--key", default=str(ROOT / "secrets" / "id_ed25519"))
    ap.add_argument("-o", default=str(ROOT / "toolchains" / "isa-score" / "validate"))
    a = ap.parse_args()
    out = Path(a.o)
    out.mkdir(parents=True, exist_ok=True)
    objs = []
    for lvl in ("O0", "O2", "O3"):
        o = out / f"kernels-{lvl}.o"
        sh(LCC, f"-{lvl}", f"-DSUF={lvl}", "-c", str(HERE / "kernels.c"),
           "-o", str(o))
        objs.append(str(o))
    prog = out / "validate"
    sh(LCC, "-O1", str(HERE / "main.c"), str(HERE / "clk.s"), *objs,
       "-o", str(prog))
    counts = out / "validate.counts"
    qemu_out = subprocess.run([str(SCORE / "count.sh"), "-o", str(counts),
                               str(prog)], check=True, capture_output=True,
                              text=True).stdout
    calls = int(re.search(r"calls (\d+)", qemu_out).group(1))
    c = score.load_counts(counts, score.elfmod.Elf(str(prog)))
    rows = score.analyse(str(prog), 3, c, None)
    pred = defaultdict(float)
    for r in rows:
        if r["func"].startswith("k_"):
            pred[r["func"]] += score.total_cost(r, "achieved", True)
    meas = {}
    if a.remote:
        ssh = ["ssh", "-i", a.key, "-o", "IdentitiesOnly=yes", a.remote]
        sh(*ssh, f"mkdir -p {a.remote_dir}")
        sh("scp", "-q", "-i", a.key, "-o", "IdentitiesOnly=yes", str(prog),
           f"{a.remote}:{a.remote_dir}/")
        res = subprocess.run(ssh + [f"cd {a.remote_dir} && taskset -c 2 ./validate"],
                             check=True, capture_output=True, text=True).stdout
        for line in res.splitlines():
            m = re.match(r"(k_\w+) \w+ (\d+)", line)
            if m:
                meas[m.group(1)] = int(m.group(2))
    print(f"{'kernel':16s} {'predicted':>10s} {'measured':>10s} {'ratio':>6s}")
    for name in sorted(pred):
        p = pred[name] / calls
        m = meas.get(name)
        ratio = f"{m / p:6.2f}" if m else "     -"
        print(f"{name:16s} {p:10.0f} {str(m or '-'):>10s} {ratio}")
    (out / "validate.json").write_text(json.dumps(
        {"predicted": {k: v / calls for k, v in pred.items()},
         "measured": meas}, indent=1))


if __name__ == "__main__":
    main()
