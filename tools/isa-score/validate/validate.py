#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Compare isa-score's "achieved" cycles with cycles measured on hardware.

  validate.py --remote USER@HOST [--remote-dir DIR] [--key FILE] [-o DIR]
              [--target T] [--ssh-port N] [--cpu N]

Builds kernels.c with lcc at -O0, -O2 and -O3 and main.c, then on
the E2K machine: runs every kernel REPEAT times under perf, sampling the
EXEC and TICKS monitors (profile.py), which gives how often each block
ran and the cycles spent in each kernel, and times each kernel with
%clkr.  The prediction is isa-score's static per-block cycles weighted
by the sampled counts.  The data fits in L1, so prediction and
measurement should agree as far as the model is right.

--target picks the lcc build by its /opt/mcst directory suffix (default
v3; use the CPU-tuned one for the machine: v3.4c for the 4C, v5.8c2
for the 8C2, v6.2c3 for the 2C3 with --cpu 0 and --period 40009);
the model uses that ISA version.
"""

import argparse
import re
import subprocess
import sys
from collections import defaultdict
from pathlib import Path
from types import SimpleNamespace

HERE = Path(__file__).resolve().parent
SCORE = HERE.parent
ROOT = SCORE.parents[1]
LCC = "/opt/mcst/lcc-1.29.16.e2k-{}.linux-6.1/bin/lcc"
REPEAT = 20000
PERIOD = 10007
sys.path.insert(0, str(SCORE))
import profile as prof  # noqa: E402
import score  # noqa: E402


def sh(*cmd, **kw):
    return subprocess.run(cmd, check=True, **kw)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--remote", required=True)
    ap.add_argument("--remote-dir", default="relcc2/isa-score")
    ap.add_argument("--key", default=str(ROOT / "secrets" / "id_ed25519"))
    ap.add_argument("--ssh-port", type=int)
    ap.add_argument("--cpu", type=int, default=2)
    ap.add_argument("-o", default=str(ROOT / "toolchains" / "isa-score" / "validate"))
    ap.add_argument("--target", default="v3")
    # perf throttles sampling above about 100,000 samples a second, and
    # TICKS at PERIOD 10007 exceeds that above 1 GHz (the 8C2: use 30011).
    ap.add_argument("--period", type=int, default=PERIOD)
    a = ap.parse_args()
    lcc = LCC.format(a.target)
    iset = int(re.match(r"v(\d+)", a.target).group(1))
    out = Path(a.o)
    out.mkdir(parents=True, exist_ok=True)
    objs = []
    for lvl in ("O0", "O2", "O3"):
        o = out / f"kernels-{lvl}.o"
        sh(lcc, f"-{lvl}", f"-DSUF={lvl}", "-c", str(HERE / "kernels.c"),
           "-o", str(o))
        objs.append(str(o))
    prog = out / "validate"
    sh(lcc, "-O1", str(HERE / "main.c"), str(HERE / "clk.s"), *objs,
       "-o", str(prog))

    pa = SimpleNamespace(remote=a.remote, remote_dir=a.remote_dir, key=a.key,
                         ssh_port=a.ssh_port, cpu=a.cpu, period=a.period, args=[str(REPEAT)],
                         o=str(out), stdin="")
    spath, tpath, _ = prof.run_remote(pa, str(prog))
    ssh = ["ssh", *prof.ssh_opts(pa), a.remote]
    res = subprocess.run(ssh + [f"cd {a.remote_dir} && taskset -c {a.cpu}"
                                " ./validate"],
                         check=True, capture_output=True, text=True).stdout
    clk = {m.group(1): int(m.group(2)) for m in
           re.finditer(r"^(k_\w+) \w+ (\d+)$", res, re.M)}

    samples, _ = prof.read_samples(spath, str(prog))
    taken, _ = prof.read_samples(tpath, str(prog))
    rows, _ = prof.weigh(str(prog), iset, samples, a.period, taken)
    pred, meas = defaultdict(float), defaultdict(float)
    for r in rows:
        if r["func"].startswith("k_"):
            pred[r["func"]] += prof.achieved_run(r) / REPEAT
            meas[r["func"]] += r["measured"] / REPEAT
    print("Cycles per call (sampled: from TICKS samples in the kernel;"
          " %clkr: includes the call and return)")
    print(f"{'kernel':16s} {'predicted':>10s} {'sampled':>10s} {'%clkr':>8s}"
          f" {'sampled/pred':>12s}")
    for name in sorted(pred):
        p, m = pred[name], meas[name]
        print(f"{name:16s} {p:10.0f} {m:10.0f} {str(clk.get(name, '-')):>8s}"
              f" {score.fmt_ratio(m, p):>12s}")


if __name__ == "__main__":
    main()
