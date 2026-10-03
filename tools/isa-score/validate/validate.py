#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Compare isa-score's "achieved" cycles with cycles measured on hardware.

  validate.py --remote USER@HOST [--remote-dir DIR] [--key FILE] [-o DIR]

Builds kernels.c with lcc at -O0, -O2 and -O3 (v3) and main.c, then on
the E2K machine over SSH: single-steps each kernel once under gdb
(count.py) to learn how often each wide instruction executes, and times
each kernel with %clkr.  The prediction is isa-score's static per-block
cycles weighted by those counts.  The data fits in L1, so the two should
agree as far as the model is right.
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
import elf as elfmod  # noqa: E402
import score  # noqa: E402


def sh(*cmd, **kw):
    return subprocess.run(cmd, check=True, **kw)


def predict(rows, counts):
    """Cycles per kernel from static rows and executions per address."""
    pred = defaultdict(float)
    for r in rows:
        if not r["func"].startswith("k_"):
            continue
        n = counts.get(r["addr"], 0)
        entries = sum(counts.get(a, 0) for a in r.get("entry_from", []))
        pred[r["func"]] += score.total_cost(r, "achieved", n, entries)
    return pred


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--remote", required=True)
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
    elf = elfmod.Elf(str(prog))
    ranges = {s.name: (s.addr, s.addr + s.size) for s in elf.symbols.values()
              if s.name.startswith("k_")}
    (out / "kernels.json").write_text(json.dumps(ranges))

    ssh = ["ssh", "-i", a.key, "-o", "IdentitiesOnly=yes", a.remote]
    scp = ["scp", "-q", "-i", a.key, "-o", "IdentitiesOnly=yes"]
    sh(*ssh, f"mkdir -p {a.remote_dir}")
    sh(*scp, str(prog), str(out / "kernels.json"), str(HERE / "count.py"),
       f"{a.remote}:{a.remote_dir}/")
    sh(*ssh, f"cd {a.remote_dir} && gdb -batch -x count.py --args "
       "./validate count > /dev/null 2>&1")
    sh(*scp, f"{a.remote}:{a.remote_dir}/counts.json", str(out / "counts.json"))
    counts = {int(k, 16): v for k, v in
              json.loads((out / "counts.json").read_text()).items()}
    res = subprocess.run(ssh + [f"cd {a.remote_dir} && taskset -c 2 ./validate"],
                         check=True, capture_output=True, text=True).stdout
    meas = {m.group(1): int(m.group(2)) for m in
            re.finditer(r"^(k_\w+) \w+ (\d+)$", res, re.M)}

    pred = predict(score.analyse(str(prog), 3), counts)
    print(f"{'kernel':16s} {'predicted':>10s} {'measured':>10s} {'ratio':>6s}")
    for name in sorted(pred):
        p, m = pred[name], meas.get(name)
        ratio = f"{m / p:6.2f}" if m and p else "     -"
        print(f"{name:16s} {p:10.0f} {str(m or '-'):>10s} {ratio}")
    (out / "validate.json").write_text(json.dumps(
        {"predicted": pred, "measured": meas}, indent=1))


if __name__ == "__main__":
    main()
